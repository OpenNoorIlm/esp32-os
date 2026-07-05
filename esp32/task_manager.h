#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

// TaskManager -- runs ONE shell command in the background on a dedicated
// FreeRTOS task, so a long-running `lua`, `run <app>`, `apt install`, or
// `app-installer install/upgrade` doesn't block the interactive shell
// session that started it.
//
// Deliberately a SINGLE job slot -- not a queue, not multiple concurrent
// jobs. ESP32's limited free heap (~278KB on this build) doesn't have room
// to safely run several of these at once: installs allocate JSON docs and
// HTTP buffers, Lua scripts allocate their own tables/closures on top of
// the shared interpreter. One thing running in the background at a time
// keeps the RAM story simple and predictable; everything else in the
// shell (status/kill/close/jobs, and anything that doesn't need the
// specific resource the job is using) stays immediately responsive.
namespace TaskManager {

// Matches ShellServer::runCommand's signature exactly -- kept as a raw
// function pointer (not a direct #include of shell_server.h) specifically
// so package_manager.h / lua_engine.cpp can each include just this tiny
// header to poll cancellation without dragging in the entire shell
// command dispatcher and everything IT includes.
using CommandRunner = String (*)(const String& cmdLine, String& cwd, Print& out);

inline CommandRunner g_runner = nullptr;

// Wired up once from esp32.ino's setup(), e.g.:
//   TaskManager::begin(ShellServer::runCommand);
inline void begin(CommandRunner runner) { g_runner = runner; }

} // namespace TaskManager

namespace TaskManager {

// ── cancellation, correctly scoped ──────────────────────────────────────
// g_cancelRequested alone would be a bug: if it were polled directly by
// LuaEngine/PackageManager, closing background job A would also abort an
// unrelated FOREGROUND `lua`/`apt install` command that happens to be
// running at the same moment, purely because they poll the same global
// flag. g_inBackgroundJob scopes it: shouldCancelNow() is only ever true
// while code is executing *as* the tracked background job, so a foreground
// command polling the exact same check function is correctly unaffected.
inline volatile bool g_cancelRequested = false;
inline volatile bool g_inBackgroundJob = false;

// The one function every cancellable loop (PackageManager's per-file/
// per-package loops, LuaEngine's per-instruction debug hook) should poll.
inline bool shouldCancelNow() { return g_cancelRequested && g_inBackgroundJob; }

} // namespace TaskManager

namespace TaskManager {

// ── the single job slot ─────────────────────────────────────────────────
struct Job {
  String name;
  String cmdLine;
  TaskHandle_t handle = nullptr;
  volatile bool running = false;
  volatile bool killed = false;   // so status() can distinguish killed vs finished
  unsigned long startedAt = 0;
  unsigned long endedAt = 0;
  SemaphoreHandle_t outMutex = nullptr;
  String output;                  // bounded, see appendOutput()
};

inline Job g_job;
inline bool g_hasJob = false; // has a job EVER been started (vs never used yet)

const size_t MAX_OUTPUT_BYTES = 6000; // bounded so a chatty job can't slowly eat all heap

inline void appendOutput(const String& s) {
  if (!g_job.outMutex) return;
  if (xSemaphoreTake(g_job.outMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
    g_job.output += s;
    if (g_job.output.length() > MAX_OUTPUT_BYTES) {
      // Keep the tail -- most recent output matters most when checking on
      // a still-running (or just-finished) job.
      g_job.output = g_job.output.substring(g_job.output.length() - MAX_OUTPUT_BYTES);
    }
    xSemaphoreGive(g_job.outMutex);
  }
}

// A Print& that streams into the job's bounded, mutex-protected buffer
// instead of a live TCP client -- there isn't one; the background job may
// still be running long after the session that launched it disconnects.
class JobPrint : public Print {
 public:
  size_t write(uint8_t c) override { appendOutput(String((char)c)); return 1; }
  size_t write(const uint8_t* buffer, size_t size) override {
    String s; s.reserve(size);
    for (size_t i = 0; i < size; i++) s += (char)buffer[i];
    appendOutput(s);
    return size;
  }
};

} // namespace TaskManager

namespace TaskManager {

inline void taskTrampoline(void* param) {
  JobPrint out;
  String cwd = "/"; // background jobs get their own virtual cwd, independent
                     // of whichever shell session launched them (which may
                     // since have disconnected -- "background" means it
                     // outlives that connection).
  String result;
  g_inBackgroundJob = true;
  if (g_runner) result = g_runner(g_job.cmdLine, cwd, out);
  g_inBackgroundJob = false;
  if (result.length()) appendOutput("\n" + result + "\n");

  g_job.running = false;
  g_job.endedAt = millis();
  g_cancelRequested = false;
  vTaskDelete(nullptr);
}

inline bool isBusy() { return g_hasJob && g_job.running; }

// Starts `cmdLine` (e.g. "lua print(1+1)", "run myapp", "apt install foo",
// "app-installer upgrade") as the single background job. Fails if a job is
// already running -- no queueing, see the namespace doc comment for why.
inline String start(const String& name, const String& cmdLine) {
  if (isBusy())
    return "error: a background job ('" + g_job.name + "') is already running -- "
           "check 'jobs', or 'close " + g_job.name + "' / 'kill " + g_job.name + "' first.";
  if (!g_runner) return "error: task manager not initialized (internal error).";

  if (g_job.outMutex) vSemaphoreDelete(g_job.outMutex);
  g_job.outMutex = xSemaphoreCreateMutex();
  g_job.name = name;
  g_job.cmdLine = cmdLine;
  g_job.output = "";
  g_job.running = true;
  g_job.killed = false;
  g_job.startedAt = millis();
  g_job.endedAt = 0;
  g_cancelRequested = false;
  g_hasJob = true;

  BaseType_t ok = xTaskCreate(taskTrampoline, "noor_bg_job", 8192, nullptr, 1, &g_job.handle);
  if (ok != pdPASS) {
    g_job.running = false;
    return "error: could not start background task (out of memory?).";
  }
  return "started background job '" + name + "': " + cmdLine;
}

} // namespace TaskManager

namespace TaskManager {

inline String status() {
  if (!g_hasJob) return "(no background job has been started)";
  String s = "Job: " + g_job.name + "\n";
  s += "Command: " + g_job.cmdLine + "\n";
  s += "Status: " + String(g_job.running ? "running" : (g_job.killed ? "killed" : "finished")) + "\n";
  unsigned long elapsed = (g_job.running ? millis() : g_job.endedAt) - g_job.startedAt;
  s += "Elapsed: " + String(elapsed / 1000) + "s\n";
  return s;
}

inline String output(const String& name) {
  if (!g_hasJob || !name.equalsIgnoreCase(g_job.name))
    return "error: no job named '" + name + "' (check 'jobs')";
  String snapshot;
  if (g_job.outMutex && xSemaphoreTake(g_job.outMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
    snapshot = g_job.output;
    xSemaphoreGive(g_job.outMutex);
  }
  return snapshot.length() ? snapshot : "(no output yet)";
}

// Cooperative stop: the job's own loop (PackageManager's per-file checks,
// LuaEngine's per-instruction hook) notices shouldCancelNow() and unwinds
// itself at its next safe checkpoint. May take a moment if the job is deep
// in one long call with no checkpoint in between (e.g. a single slow
// network request) -- reach for kill() when that matters.
inline String close(const String& name) {
  if (!g_hasJob || !name.equalsIgnoreCase(g_job.name))
    return "error: no job named '" + name + "' (check 'jobs')";
  if (!g_job.running) return "job '" + name + "' isn't running.";
  g_cancelRequested = true;
  return "close requested for '" + name + "' -- it will stop at its next safe checkpoint.";
}

// Forceful stop: immediately deletes the FreeRTOS task. No cleanup runs on
// the job's side (a half-written file from an install, or a Lua script
// mid-chunk, is simply abandoned) -- try close() first when you can wait a
// moment; kill() is for when a job is stuck with no checkpoint to reach.
inline String kill(const String& name) {
  if (!g_hasJob || !name.equalsIgnoreCase(g_job.name))
    return "error: no job named '" + name + "' (check 'jobs')";
  if (!g_job.running) return "job '" + name + "' isn't running.";
  if (g_job.handle) vTaskDelete(g_job.handle);
  g_job.running = false;
  g_job.killed = true;
  g_job.endedAt = millis();
  g_cancelRequested = false;
  g_inBackgroundJob = false;
  return "killed '" + name + "'.";
}

} // namespace TaskManager
