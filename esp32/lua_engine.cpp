// Embeds the official Lua interpreter (source in lua_src/lua-master/,
// fetched from https://github.com/lua/lua) into this sketch as a library,
// using Lua's own "onelua.c" amalgamation build (MAKE_LIB mode = no main(),
// just the VM + stdlib, so it doesn't fight Arduino's setup()/loop()).
//
// IMPORTANT build note: only THIS .cpp should ever compile the Lua sources.
// Under classic Arduino IDE, files in a sketch SUBFOLDER (lua_src/lua-master/)
// are not auto-discovered as separate compilation units, so this is safe.
// If you build with PlatformIO/arduino-cli instead, double check it isn't
// also picking up lua_src/**/*.c as independent sources -- that would define
// every Lua symbol twice and fail to link. If that happens, either exclude
// lua_src from the source scan, or move it outside this sketch folder and
// reference it via an include path.
#include "lua_engine.h"
#include "fs_manager.h"
#include "robot_api.h"
#include "package_manager.h"
#include "capability.h"
#include "task_manager.h"
#include <WiFi.h>

// IMPORTANT: all Arduino/C++ headers above MUST be included before Lua's
// sources below. Lua's llex.c #defines a macro literally named "next(ls)",
// and since extern "C" only affects linkage (not the preprocessor), that
// macro escapes into this whole translation unit and clobbers any later
// use of std::next / ranges::next / ArduinoJson's own next() methods --
// which is exactly what happens if this order is reversed. The #undef
// below is a second safety net in case anything further down this file
// also needs the real "next".
#define MAKE_LIB
#define LUA_32BITS   // ints/floats instead of int64/double -- meaningfully
                     // less RAM per Lua value, worth it on ESP32's limited
                     // heap. Remove this line if you need 64-bit precision.

extern "C" {
#include "lua_src/lua-master/onelua.c"
}
#undef next

namespace LuaEngine {

static lua_State* L = nullptr;

// Overrides Lua's global print() to stream straight to whichever live
// Print& is currently running the script (the NoorShell TCP client) instead
// of buffering into a String that only gets flushed once the whole script
// finishes. This matters a lot for `run <app>`: an app like the tiny
// transformer runtime can take seconds per token, and a buffered print()
// would leave the terminal looking dead the entire time instead of showing
// output as it's actually generated.
//
// liveOut is only non-null for the duration of a single eval()/runApp()
// call (set at the top, cleared before returning) -- there is exactly one
// shell session at a time (see ShellServer::loop()), so this simple global
// is safe and avoids threading a Print& through every single Lua binding.
static Print* liveOut = nullptr;
static bool didPrint = false; // did this eval/runApp call print() at least once

// Guards against two lua_State entries at once: the state (and liveOut
// above) is a single shared global, not thread-safe, so a background job
// running "lua"/"run" and a foreground "lua"/"run" in the interactive
// shell must never both be inside luaL_dostring() at the same time.
static volatile bool busy = false;

// Cooperative cancellation checkpoint, installed as a Lua debug hook (see
// runChunk below) so a background job's `close <name>` can interrupt a
// running script between VM instructions instead of only being able to
// stop it before/after a whole chunk runs. Uses shouldCancelNow() (not the
// raw flag) so this can never misfire against an unrelated foreground
// lua/run command -- see task_manager.h for why that distinction matters.
static void cancelHook(lua_State* Ls, lua_Debug* ar) {
  if (TaskManager::shouldCancelNow()) {
    luaL_error(Ls, "cancelled"); // unwinds via lua_error, caught below
  }
}

static int capturePrint(lua_State* Lstate) {
  int n = lua_gettop(Lstate);
  didPrint = true;
  for (int i = 1; i <= n; i++) {
    size_t len;
    const char* s = luaL_tolstring(Lstate, i, &len);
    if (liveOut) liveOut->print(s);
    lua_pop(Lstate, 1); // pop the tostring() result pushed by luaL_tolstring
    if (i < n && liveOut) liveOut->print("\t");
  }
  if (liveOut) { liveOut->println(); liveOut->flush(); }
  return 0;
}

} // namespace LuaEngine

namespace LuaEngine {

// ── esp32.robot.* ─────────────────────────────────────────────────────────
static int l_robot_forward(lua_State* Ls) {
  int d = (int)luaL_optinteger(Ls, 1, 1);
  int s = (int)luaL_optinteger(Ls, 2, 255);
  lua_pushstring(Ls, RobotApi::forward(String(d), String(s)).c_str());
  return 1;
}
static int l_robot_backward(lua_State* Ls) {
  int d = (int)luaL_optinteger(Ls, 1, 1);
  int s = (int)luaL_optinteger(Ls, 2, 255);
  lua_pushstring(Ls, RobotApi::backward(String(d), String(s)).c_str());
  return 1;
}
static int l_robot_right(lua_State* Ls) {
  int a = (int)luaL_optinteger(Ls, 1, 90);
  lua_pushstring(Ls, RobotApi::right(String(a)).c_str());
  return 1;
}
static int l_robot_left(lua_State* Ls) {
  int a = (int)luaL_optinteger(Ls, 1, 90);
  lua_pushstring(Ls, RobotApi::left(String(a)).c_str());
  return 1;
}
static int l_robot_stop(lua_State* Ls) {
  lua_pushstring(Ls, RobotApi::stop().c_str());
  return 1;
}

} // namespace LuaEngine

namespace LuaEngine {

static int l_robot_distance(lua_State* Ls) {
  int a = (int)luaL_optinteger(Ls, 1, 90);
  lua_pushstring(Ls, RobotApi::distance(String(a)).c_str());
  return 1;
}
static int l_robot_temperature(lua_State* Ls) {
  const char* unit = luaL_optstring(Ls, 1, "c");
  lua_pushstring(Ls, RobotApi::temperature(String(unit)).c_str());
  return 1;
}
static int l_robot_fan(lua_State* Ls) {
  const char* q = luaL_optstring(Ls, 1, "status");
  lua_pushstring(Ls, RobotApi::fan(String(q)).c_str());
  return 1;
}
static int l_robot_clear(lua_State* Ls) {
  lua_pushstring(Ls, RobotApi::clearCmd().c_str());
  return 1;
}
static int l_robot_eyes(lua_State* Ls) {
  const char* t = luaL_optstring(Ls, 1, "Normal");
  lua_pushstring(Ls, RobotApi::eyes(String(t)).c_str());
  return 1;
}
static int l_robot_shutdown(lua_State* Ls) { RobotApi::shutdown(); return 0; }
static int l_robot_shuton(lua_State* Ls)   { RobotApi::shutOn();   return 0; }
static int l_robot_shutdownBySeconds(lua_State* Ls) {
  RobotApi::shutdownBySeconds(String((int)luaL_checkinteger(Ls, 1))); return 0;
}
static int l_robot_shutdownByTime(lua_State* Ls) {
  RobotApi::shutdownByTime(String(luaL_checkstring(Ls, 1))); return 0;
}
static int l_robot_shutonBySeconds(lua_State* Ls) {
  RobotApi::shutOnBySeconds(String((int)luaL_checkinteger(Ls, 1))); return 0;
}
static int l_robot_shutonByTime(lua_State* Ls) {
  RobotApi::shutOnByTime(String(luaL_checkstring(Ls, 1))); return 0;
}

} // namespace LuaEngine

namespace LuaEngine {

// ── esp32.fs.* -- paths are absolute real paths (e.g. "/apps"), resolved
// relative to filesystem root, mirroring the shell's own path handling. ──
static int l_fs_ls(lua_State* Ls) {
  String p = FsManager::toRealPath("/", luaL_optstring(Ls, 1, "/"));
  lua_pushstring(Ls, FsManager::ls(p).c_str());
  return 1;
}
static int l_fs_mkdir(lua_State* Ls) {
  String p = FsManager::toRealPath("/", luaL_checkstring(Ls, 1));
  lua_pushboolean(Ls, FsManager::mkdir(p));
  return 1;
}
static int l_fs_rm(lua_State* Ls) {
  String p = FsManager::toRealPath("/", luaL_checkstring(Ls, 1));
  lua_pushboolean(Ls, FsManager::remove(p));
  return 1;
}
static int l_fs_cat(lua_State* Ls) {
  String p = FsManager::toRealPath("/", luaL_checkstring(Ls, 1));
  lua_pushstring(Ls, FsManager::cat(p).c_str());
  return 1;
}
static int l_fs_df(lua_State* Ls) {
  lua_pushstring(Ls, FsManager::df().c_str());
  return 1;
}

} // namespace LuaEngine

namespace LuaEngine {

// ── esp32.wifi.* ────────────────────────────────────────────────────────
static int l_wifi_ip(lua_State* Ls)   { lua_pushstring(Ls, WiFi.localIP().toString().c_str()); return 1; }
static int l_wifi_ssid(lua_State* Ls) { lua_pushstring(Ls, WiFi.SSID().c_str()); return 1; }

// ── esp32.storage.* ─────────────────────────────────────────────────────
static int l_storage_df(lua_State* Ls) { lua_pushstring(Ls, FsManager::df().c_str()); return 1; }
static int l_storage_change(lua_State* Ls) {
  lua_pushstring(Ls, FsManager::changeStorage(String(luaL_checkstring(Ls, 1))).c_str());
  return 1;
}

// installOfficial() streams every install stage live to a Print& (the
// shell's TCP client during a normal shell session) instead of returning a
// String -- there's no such live client when the call originates from Lua,
// so this tiny Print buffers everything written to it into a String, which
// we then hand back to Lua as a single return value.
class StringPrint : public Print {
 public:
  size_t write(uint8_t c) override { buf += (char)c; return 1; }
  size_t write(const uint8_t* buffer, size_t size) override {
    buf.reserve(buf.length() + size);
    for (size_t i = 0; i < size; i++) buf += (char)buffer[i];
    return size;
  }
  String buf;
};

// ── esp32.apt.* ──────────────────────────────────────────────────────────
static int l_apt_list(lua_State* Ls)          { lua_pushstring(Ls, PackageManager::listOfficial(APT_REPO).c_str()); return 1; }
static int l_apt_listInstalled(lua_State* Ls) { lua_pushstring(Ls, PackageManager::listInstalled("/pkgs").c_str()); return 1; }
static int l_apt_install(lua_State* Ls) {
  String name = luaL_checkstring(Ls, 1);
  StringPrint sp;
  PackageManager::installOfficial(APT_REPO, name, "/pkgs", sp);
  lua_pushstring(Ls, sp.buf.c_str());
  return 1;
}

// ── esp32.appinstaller.* ─────────────────────────────────────────────────
static int l_app_list(lua_State* Ls)          { lua_pushstring(Ls, PackageManager::listOfficial(APP_REPO).c_str()); return 1; }
static int l_app_listInstalled(lua_State* Ls) { lua_pushstring(Ls, PackageManager::listInstalled("/apps").c_str()); return 1; }
static int l_app_install(lua_State* Ls) {
  String name = luaL_checkstring(Ls, 1);
  StringPrint sp;
  PackageManager::installOfficial(APP_REPO, name, "/apps", sp);
  lua_pushstring(Ls, sp.buf.c_str());
  return 1;
}

} // namespace LuaEngine

namespace LuaEngine {

// ── esp32.sysinfo() / esp32.restart() ───────────────────────────────────
static int l_sysinfo(lua_State* Ls) {
  String out = capabilitiesReport();
  out += "\nInstalled OS packages (/pkgs):\n" + PackageManager::listInstalled("/pkgs");
  out += "\nInstalled apps (/apps):\n" + PackageManager::listInstalled("/apps");
  lua_pushstring(Ls, out.c_str());
  return 1;
}
static int l_restart(lua_State* Ls) { ESP.restart(); return 0; } // does not return

static const luaL_Reg robotFuncs[] = {
  {"forward", l_robot_forward}, {"backward", l_robot_backward},
  {"right", l_robot_right},     {"left", l_robot_left}, {"stop", l_robot_stop},
  {"distance", l_robot_distance}, {"temperature", l_robot_temperature},
  {"fan", l_robot_fan}, {"clear", l_robot_clear}, {"eyes", l_robot_eyes},
  {"shutdown", l_robot_shutdown}, {"shuton", l_robot_shuton},
  {"shutdownBySeconds", l_robot_shutdownBySeconds}, {"shutdownByTime", l_robot_shutdownByTime},
  {"shutonBySeconds", l_robot_shutonBySeconds}, {"shutonByTime", l_robot_shutonByTime},
  {NULL, NULL}
};
static const luaL_Reg fsFuncs[] = {
  {"ls", l_fs_ls}, {"mkdir", l_fs_mkdir}, {"rm", l_fs_rm}, {"cat", l_fs_cat}, {"df", l_fs_df},
  {NULL, NULL}
};
static const luaL_Reg wifiFuncs[] = { {"ip", l_wifi_ip}, {"ssid", l_wifi_ssid}, {NULL, NULL} };
static const luaL_Reg storageFuncs[] = { {"df", l_storage_df}, {"change", l_storage_change}, {NULL, NULL} };
static const luaL_Reg aptFuncs[] = {
  {"list", l_apt_list}, {"listInstalled", l_apt_listInstalled}, {"install", l_apt_install}, {NULL, NULL}
};
static const luaL_Reg appFuncs[] = {
  {"list", l_app_list}, {"listInstalled", l_app_listInstalled}, {"install", l_app_install}, {NULL, NULL}
};

static void pushSubtable(lua_State* Ls, const luaL_Reg* funcs) {
  lua_newtable(Ls);
  luaL_setfuncs(Ls, funcs, 0);
}

} // namespace LuaEngine

namespace LuaEngine {

void begin() {
  if (L) return; // already initialized
  L = luaL_newstate();
  if (!L) return; // out of memory creating the state
  luaL_openlibs(L);
  lua_pushcfunction(L, capturePrint);
  lua_setglobal(L, "print");

  // esp32.* table -- every shell capability, callable from Lua scripts.
  // NOTE: this deliberately does NOT expose curl/"install from arbitrary
  // URL" -- that path has an interactive [Y/n] untrusted-source warning in
  // the shell (shell_server.h), and a script has no human to ask. Silently
  // wiring that up here would let any Lua code fetch+run arbitrary remote
  // files with zero confirmation, which isn't a safe default to ship.
  lua_newtable(L);
  pushSubtable(L, robotFuncs);   lua_setfield(L, -2, "robot");
  pushSubtable(L, fsFuncs);      lua_setfield(L, -2, "fs");
  pushSubtable(L, wifiFuncs);    lua_setfield(L, -2, "wifi");
  pushSubtable(L, storageFuncs); lua_setfield(L, -2, "storage");
  pushSubtable(L, aptFuncs);     lua_setfield(L, -2, "apt");
  pushSubtable(L, appFuncs);     lua_setfield(L, -2, "appinstaller");
  lua_pushcfunction(L, l_sysinfo); lua_setfield(L, -2, "sysinfo");
  lua_pushcfunction(L, l_restart); lua_setfield(L, -2, "restart");
  lua_setglobal(L, "esp32");
}

// Shared runner: executes one chunk of Lua source with print() streaming
// live to `out`, used by both eval() (raw "lua <code>" shell command) and
// runApp() (reads an app's main.lua first, then hands it the same source).
static String runChunk(const String& src, Print& out) {
  if (!L) begin();
  if (!L) return "error: could not allocate Lua state (out of memory)";
  if (busy) return "error: lua engine busy (a background job is currently running lua code)";
  busy = true;

  liveOut = &out;
  didPrint = false;
  // Check for cancellation every 1000 VM instructions -- frequent enough
  // that `close` on a background lua job feels near-immediate, rare
  // enough that the overhead is not worth worrying about.
  lua_sethook(L, cancelHook, LUA_MASKCOUNT, 1000);
  int status = luaL_dostring(L, src.c_str());
  lua_sethook(L, nullptr, 0, 0); // clear so it never leaks into an unrelated later call
  liveOut = nullptr; // always clear -- luaL_dostring is protected (lua_pcall
                      // under the hood), so it returns normally even on a
                      // Lua-side error instead of longjmp'ing past this line.
  busy = false;

  if (status != LUA_OK) {
    String err = "lua error: " + String(lua_tostring(L, -1));
    lua_pop(L, 1); // pop the error message off the stack
    return err;
  }
  // Output (if any) already streamed live via out during execution -- only
  // fall back to a friendly status line when the script printed nothing at
  // all, so a silent script (or one that just calls esp32.* side effects)
  // doesn't leave the shell looking like it hung.
  return didPrint ? "" : "ok";
}

String eval(const String& code, Print& out) {
  return runChunk(code, out);
}

String runApp(const String& appName, Print& out) {
  if (appName.length() == 0) return "error: run needs an app name, e.g. run esp32-cpp";

  String path = "/apps/" + appName + "/main.lua";
  if (!FsManager::activeFs().exists(path))
    return "error: '" + appName + "' is not installed (no " + path + " found -- "
           "check 'app-installer list --installed')";

  String src = FsManager::cat(path);
  if (src.length() == 0 || src.startsWith("error:"))
    return "error: could not read " + path;

  String result = runChunk(src, out);
  return result == "ok" ? "" : result; // "run" itself doesn't need the "ok" filler
                                        // eval() prints for bare one-liners --
                                        // an app finishing with no output is
                                        // just normal, silent completion.
}

} // namespace LuaEngine
