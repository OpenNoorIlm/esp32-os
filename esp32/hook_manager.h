// ╔══════════════════════════════════════════════════════════════════════════╗
// ║  NoorOS — hook_manager.h                                                 ║
// ║  os.hook() system — boot/event hooks from Lua + editable .dvr files     ║
// ║                                                                          ║
// ║  Boot stages (fired in order from esp32.ino):                           ║
// ║    early_boot  before WiFi, before screen                               ║
// ║    pre_wifi    right before WiFi.begin()                                ║
// ║    post_wifi   after WiFi connected or timed out                        ║
// ║    pre_shell   before NoorShell TCP server starts                       ║
// ║    boot        everything ready — main user hook                        ║
// ║    idle        called from loop() on a throttled interval               ║
// ║    shutdown    before ESP.restart()                                     ║
// ║                                                                          ║
// ║  Event hooks (triggered by os.emit()):                                  ║
// ║    wifi_connected  wifi_lost  touch  app_start  app_exit  error         ║
// ║                                                                          ║
// ║  .dvr files — editable Lua scripts stored on SPIFFS:                   ║
// ║    /hooks/boot.dvr        runs at "boot" stage                         ║
// ║    /hooks/idle.dvr        runs every idle tick                          ║
// ║    /hooks/<name>.dvr      any stage name                                ║
// ║                                                                          ║
// ║  Lua API (in "os" global table):                                        ║
// ║    os.hook("stage", fn)            register a function handler          ║
// ║    os.hook_str("stage", "code")    register a code string handler       ║
// ║    os.emit("stage", "arg?")        fire a hook stage                   ║
// ║    os.hooks()                      list all registered hooks            ║
// ║    os.dvr("stage", "code")         save/replace a .dvr file            ║
// ║    os.undvr("stage")               delete a .dvr file                  ║
// ║    os.read_dvr("stage")            read current .dvr content           ║
// ║    os.idle_interval(ms)            change idle tick interval            ║
// ║    os.clear_hooks("stage"?)        remove Lua hooks for stage/all      ║
// ║                                                                          ║
// ║  MIT License — NoorRobot / OpenNoorIlm                                  ║
// ╚══════════════════════════════════════════════════════════════════════════╝
#pragma once
#include "lua_config.h"
extern "C" {
#include "lua_src/lua-master/lua.h"
#include "lua_src/lua-master/lauxlib.h"
}
#include <Arduino.h>
#include <SPIFFS.h>
#include <map>
#include <vector>
#include <functional>

// Forward declaration — defined in lua_engine.cpp
namespace LuaEngine { String eval(const String& code, Print& out); }

// ════════════════════════════════════════════════════════════════════════════
// HookManager — C++ side
// ════════════════════════════════════════════════════════════════════════════
namespace HookManager {

struct Hook {
  std::vector<std::function<void(const String&)>> cppHandlers;
  std::vector<String> luaChunks;  // inline code registered via os.hook_str()
  String dvrPath;                 // /hooks/<stage>.dvr — empty if none
};

static std::map<String, Hook>& _hooks() {
  static std::map<String, Hook> h;
  return h;
}

// Register a C++ callback
inline void on(const String& stage, std::function<void(const String&)> fn) {
  _hooks()[stage].cppHandlers.push_back(fn);
}
inline void on(const String& stage, std::function<void()> fn) {
  _hooks()[stage].cppHandlers.push_back([fn](const String&){ fn(); });
}

// Register a Lua code string (called from Lua os.hook_str())
inline void onLua(const String& stage, const String& code) {
  _hooks()[stage].luaChunks.push_back(code);
}

// Scan SPIFFS /hooks/ and register any .dvr files found
inline void loadDvrFiles() {
  File root = SPIFFS.open("/hooks");
  if (!root || !root.isDirectory()) return;
  File f = root.openNextFile();
  while (f) {
    String name = f.name();
    if (name.endsWith(".dvr")) {
      int slash = name.lastIndexOf('/');
      int dot   = name.lastIndexOf('.');
      String stage = name.substring(slash + 1, dot);
      _hooks()[stage].dvrPath = name;
      Serial.printf("[hook] .dvr: %s -> '%s'\n", name.c_str(), stage.c_str());
    }
    f = root.openNextFile();
  }
}

// Fire all handlers for a stage — C++ first, then Lua chunks, then .dvr file
inline void emit(const String& stage, const String& arg = "") {
  auto it = _hooks().find(stage);
  if (it == _hooks().end()) return;
  Hook& h = it->second;

  for (auto& fn : h.cppHandlers) fn(arg);

  for (auto& chunk : h.luaChunks) {
    String code = "_hook_stage=\"" + stage + "\"\n_hook_arg=\"" + arg + "\"\n" + chunk;
    LuaEngine::eval(code, Serial);
  }

  if (!h.dvrPath.isEmpty() && SPIFFS.exists(h.dvrPath)) {
    File dv = SPIFFS.open(h.dvrPath, "r");
    if (dv) {
      String code = "_hook_stage=\"" + stage + "\"\n_hook_arg=\"" + arg + "\"\n" + dv.readString();
      dv.close();
      LuaEngine::eval(code, Serial);
    }
  }
}

// Boot-stage helpers — call these from esp32.ino at the right points
inline void runEarlyBoot() { emit("early_boot"); }
inline void runPreWifi()   { emit("pre_wifi"); }
inline void runPostWifi(bool connected) {
  emit("post_wifi", connected ? "connected" : "failed");
  emit(connected ? "wifi_connected" : "wifi_lost");
}
inline void runPreShell()  { emit("pre_shell"); }
inline void runBoot()      { emit("boot"); }
inline void runShutdown()  { emit("shutdown"); }

// Idle hook — call from loop(); self-throttles
static unsigned long _lastIdle       = 0;
static unsigned long _idleIntervalMs = 5000;
inline void setIdleInterval(unsigned long ms) { _idleIntervalMs = ms; }
inline void runIdle() {
  if (millis() - _lastIdle < _idleIntervalMs) return;
  _lastIdle = millis();
  emit("idle");
}

// Save Lua source as a .dvr file
inline bool saveDvr(const String& stage, const String& code) {
  String path = "/hooks/" + stage + ".dvr";
  File f = SPIFFS.open(path, "w");
  if (!f) return false;
  f.print(code); f.close();
  _hooks()[stage].dvrPath = path;
  return true;
}
inline bool removeDvr(const String& stage) {
  String path = "/hooks/" + stage + ".dvr";
  _hooks()[stage].dvrPath = "";
  return SPIFFS.remove(path);
}

inline String listHooks() {
  if (_hooks().empty()) return "(no hooks)\n";
  String out;
  for (auto& kv : _hooks()) {
    out += "  " + kv.first
        + "  cpp=" + String(kv.second.cppHandlers.size())
        + "  lua=" + String(kv.second.luaChunks.size());
    if (!kv.second.dvrPath.isEmpty()) out += "  dvr=" + kv.second.dvrPath;
    out += "\n";
  }
  return out;
}

inline void clearLuaHooks(const String& stage) {
  auto it = _hooks().find(stage); if (it != _hooks().end()) it->second.luaChunks.clear();
}
inline void clearAllLuaHooks() {
  for (auto& kv : _hooks()) kv.second.luaChunks.clear();
}

} // namespace HookManager

// ════════════════════════════════════════════════════════════════════════════
// Lua bindings — merged into the "os" global by registerOsHooks(L)
// ════════════════════════════════════════════════════════════════════════════
namespace LuaHookBindings {



// os.hook("stage", fn [, persist_bool])
static int l_os_hook(lua_State* L) {
  const char* stage = luaL_checkstring(L, 1);
  luaL_checktype(L, 2, LUA_TFUNCTION);
  bool persist = lua_toboolean(L, 3);

  lua_pushvalue(L, 2);
  int ref = luaL_ref(L, LUA_REGISTRYINDEX);
  lua_State* Lref = L;
  String stageName(stage);

  HookManager::on(stageName, [Lref, ref, stageName](const String& arg) {
    lua_rawgeti(Lref, LUA_REGISTRYINDEX, ref);
    lua_pushstring(Lref, arg.c_str());
    if (lua_pcall(Lref, 1, 0, 0) != LUA_OK) {
      Serial.printf("[hook:%s] %s\n", stageName.c_str(), lua_tostring(Lref, -1));
      lua_pop(Lref, 1);
    }
  });

  if (persist) {
    // Save a stub .dvr so user knows to edit it; function bodies can't be
    // serialised portably — user should use os.dvr() with source code instead
    HookManager::saveDvr(stage,
      "-- Hook registered from Lua (function form).\n"
      "-- Replace this file with your Lua source to make it persistent.\n"
      "-- Stage: " + String(stage) + "\n");
  }

  lua_pushboolean(L, 1);
  return 1;
}

// os.hook_str("stage", "code" [, persist_bool])
static int l_os_hook_str(lua_State* L) {
  const char* stage = luaL_checkstring(L, 1);
  const char* code  = luaL_checkstring(L, 2);
  bool persist      = lua_toboolean(L, 3);
  HookManager::onLua(stage, code);
  if (persist) HookManager::saveDvr(stage, code);
  lua_pushboolean(L, 1);
  return 1;
}

// os.emit("stage" [, "arg"])
static int l_os_emit(lua_State* L) {
  const char* stage = luaL_checkstring(L, 1);
  const char* arg   = luaL_optstring(L, 2, "");
  HookManager::emit(stage, arg);
  return 0;
}

// os.hooks() -> string
static int l_os_hooks(lua_State* L) {
  lua_pushstring(L, HookManager::listHooks().c_str());
  return 1;
}

// os.dvr("stage", "code") -> bool
static int l_os_dvr(lua_State* L) {
  lua_pushboolean(L, HookManager::saveDvr(luaL_checkstring(L,1), luaL_checkstring(L,2)));
  return 1;
}

// os.undvr("stage") -> bool
static int l_os_undvr(lua_State* L) {
  lua_pushboolean(L, HookManager::removeDvr(luaL_checkstring(L, 1)));
  return 1;
}

// os.read_dvr("stage") -> string or nil, err
static int l_os_read_dvr(lua_State* L) {
  String path = "/hooks/" + String(luaL_checkstring(L, 1)) + ".dvr";
  File f = SPIFFS.open(path, "r");
  if (!f) { lua_pushnil(L); lua_pushstring(L, "not found"); return 2; }
  String s = f.readString(); f.close();
  lua_pushlstring(L, s.c_str(), s.length());
  return 1;
}

// os.idle_interval(ms)
static int l_os_idle_interval(lua_State* L) {
  HookManager::setIdleInterval((unsigned long)luaL_checkinteger(L, 1));
  return 0;
}

// os.clear_hooks("stage"?) — omit arg to clear all
static int l_os_clear_hooks(lua_State* L) {
  const char* stage = luaL_optstring(L, 1, "");
  if (stage[0]) HookManager::clearLuaHooks(stage);
  else          HookManager::clearAllLuaHooks();
  return 0;
}

static const luaL_Reg _os_fns[] = {
  {"hook",          l_os_hook},
  {"hook_str",      l_os_hook_str},
  {"emit",          l_os_emit},
  {"hooks",         l_os_hooks},
  {"dvr",           l_os_dvr},
  {"undvr",         l_os_undvr},
  {"read_dvr",      l_os_read_dvr},
  {"idle_interval", l_os_idle_interval},
  {"clear_hooks",   l_os_clear_hooks},
  {nullptr,         nullptr}
};

// Merge into existing "os" global or create it
inline void registerOsHooks(lua_State* L) {
  lua_getglobal(L, "os");
  if (!lua_istable(L, -1)) { lua_pop(L, 1); lua_newtable(L); }
  for (const luaL_Reg* r = _os_fns; r->name; r++) {
    lua_pushcfunction(L, r->func);
    lua_setfield(L, -2, r->name);
  }
  lua_setglobal(L, "os");
}

} // namespace LuaHookBindings
