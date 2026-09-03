// ╔══════════════════════════════════════════════════════════════════════════╗
// ║  NoorOS — shell_add_manager.h                                            ║
// ║  shell.add() — register custom NoorShell commands from Lua              ║
// ║                                                                          ║
// ║  Lua API (in "shell" global table):                                     ║
// ║    shell.add("name", "description", fn)                                 ║
// ║       Register a new shell command. fn(args_table, out_fn) is called    ║
// ║       when the user types "name [args...]" in NoorShell.               ║
// ║       args_table[1..n] = positional args (strings)                     ║
// ║       out_fn(str) = print to shell client                              ║
// ║                                                                          ║
// ║    shell.remove("name")        unregister a command                    ║
// ║    shell.commands()            list all custom commands                 ║
// ║    shell.help("name")          get description of a command            ║
// ║    shell.alias("alias","name") create an alias for a command           ║
// ║                                                                          ║
// ║  C++ API:                                                               ║
// ║    ShellAddManager::add(name, desc, handler)                           ║
// ║    ShellAddManager::dispatch(line, client)  — call from shell_server.h ║
// ║    ShellAddManager::has(name)               — check before dispatch    ║
// ║                                                                          ║
// ║  Example Lua usage:                                                     ║
// ║    shell.add("greet", "Say hello", function(args, out)                 ║
// ║      out("Hello, " .. (args[1] or "world") .. "!")                     ║
// ║    end)                                                                 ║
// ║                                                                          ║
// ║    shell.add("temp_log", "Log temperature every N seconds",            ║
// ║      function(args, out)                                                ║
// ║        local n = tonumber(args[1]) or 5                                ║
// ║        for i = 1, n do                                                  ║
// ║          out(sensor.temp() .. "°C")                                    ║
// ║          Qt.msleep(1000)                                                ║
// ║        end                                                              ║
// ║      end)                                                               ║
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
#include <map>
#include <vector>
#include <functional>

// ════════════════════════════════════════════════════════════════════════════
// ShellAddManager — C++ side
// ════════════════════════════════════════════════════════════════════════════
namespace ShellAddManager {

using Handler = std::function<void(const std::vector<String>&, Print&)>;

struct Command {
  String  name;
  String  description;
  Handler handler;
};

static std::map<String, Command>& _cmds() {
  static std::map<String, Command> c;
  return c;
}
static std::map<String, String>& _aliases() {
  static std::map<String, String> a;
  return a;
}

// Register a C++ command
inline void add(const String& name, const String& desc, Handler fn) {
  _cmds()[name] = {name, desc, fn};
}

inline void remove(const String& name) {
  _cmds().erase(name);
  // Remove any aliases pointing to this name
  for (auto it = _aliases().begin(); it != _aliases().end(); ) {
    if (it->second == name) it = _aliases().erase(it);
    else ++it;
  }
}

inline void alias(const String& aliasName, const String& targetName) {
  _aliases()[aliasName] = targetName;
}

inline bool has(const String& name) {
  if (_cmds().count(name)) return true;
  if (_aliases().count(name)) return true;
  return false;
}

// Dispatch a raw shell line — returns true if handled, false if not found
inline bool dispatch(const String& line, Print& client) {
  // Tokenise
  std::vector<String> tokens;
  String tok;
  bool inQuote = false;
  for (unsigned int i = 0; i <= line.length(); i++) {
    char c = i < line.length() ? line[i] : ' ';
    if (c == '"' || c == '\'') { inQuote = !inQuote; continue; }
    if (c == ' ' && !inQuote) {
      if (tok.length()) { tokens.push_back(tok); tok = ""; }
    } else tok += c;
  }

  if (tokens.empty()) return false;
  String cmd = tokens[0];
  tokens.erase(tokens.begin()); // args = tokens[1..]

  // Resolve alias
  if (_aliases().count(cmd)) cmd = _aliases()[cmd];

  auto it = _cmds().find(cmd);
  if (it == _cmds().end()) return false;

  it->second.handler(tokens, client);
  return true;
}

// List all commands as "  name  —  description\n"
inline String listCommands() {
  if (_cmds().empty()) return "(no custom commands)\n";
  String out;
  for (auto& kv : _cmds())
    out += "  " + kv.first + "  —  " + kv.second.description + "\n";
  for (auto& kv : _aliases())
    out += "  " + kv.first + "  (alias for " + kv.second + ")\n";
  return out;
}

inline String helpFor(const String& name) {
  String resolved = _aliases().count(name) ? _aliases()[name] : name;
  auto it = _cmds().find(resolved);
  if (it == _cmds().end()) return "Unknown command: " + name + "\n";
  return it->second.name + " — " + it->second.description + "\n";
}

} // namespace ShellAddManager

// ════════════════════════════════════════════════════════════════════════════
// Lua bindings — registered as "shell" global table
// ════════════════════════════════════════════════════════════════════════════
namespace LuaShellBindings {



// shell.add("name", "desc", function(args, out) ... end)
static int l_shell_add(lua_State* L) {
  const char* name = luaL_checkstring(L, 1);
  const char* desc = luaL_checkstring(L, 2);
  luaL_checktype(L, 3, LUA_TFUNCTION);

  lua_pushvalue(L, 3);
  int ref = luaL_ref(L, LUA_REGISTRYINDEX);
  lua_State* Lref = L;
  String cmdName(name);

  ShellAddManager::add(String(name), String(desc),
    [Lref, ref, cmdName](const std::vector<String>& args, Print& out) {
      lua_rawgeti(Lref, LUA_REGISTRYINDEX, ref);

      // Push args as a Lua table
      lua_newtable(Lref);
      for (int i = 0; i < (int)args.size(); i++) {
        lua_pushstring(Lref, args[i].c_str());
        lua_rawseti(Lref, -2, i + 1);
      }

      // Push out as a Lua function: out(str)
      // We capture a pointer to Print& — valid for the duration of this call
      Print* outp = &out;
      lua_pushlightuserdata(Lref, outp);
      lua_pushcclosure(Lref, [](lua_State* Ls) -> int {
        Print* p = static_cast<Print*>(lua_touserdata(Ls, lua_upvalueindex(1)));
        size_t len;
        const char* s = luaL_tolstring(Ls, 1, &len);
        p->println(s);
        lua_pop(Ls, 1);
        return 0;
      }, 1);

      if (lua_pcall(Lref, 2, 0, 0) != LUA_OK) {
        out.printf("[shell:%s] %s\n", cmdName.c_str(), lua_tostring(Lref, -1));
        lua_pop(Lref, 1);
      }
    });

  lua_pushboolean(L, 1);
  return 1;
}

// shell.remove("name")
static int l_shell_remove(lua_State* L) {
  ShellAddManager::remove(luaL_checkstring(L, 1));
  return 0;
}

// shell.commands() -> string
static int l_shell_commands(lua_State* L) {
  lua_pushstring(L, ShellAddManager::listCommands().c_str());
  return 1;
}

// shell.help("name") -> string
static int l_shell_help(lua_State* L) {
  lua_pushstring(L, ShellAddManager::helpFor(luaL_checkstring(L, 1)).c_str());
  return 1;
}

// shell.alias("alias", "name")
static int l_shell_alias(lua_State* L) {
  ShellAddManager::alias(luaL_checkstring(L, 1), luaL_checkstring(L, 2));
  return 0;
}

// shell.exec("command line") — run a command programmatically from Lua
// Output goes to Serial
static int l_shell_exec(lua_State* L) {
  const char* line = luaL_checkstring(L, 1);
  bool handled = ShellAddManager::dispatch(String(line), Serial);
  lua_pushboolean(L, handled);
  return 1;
}

static const luaL_Reg _shell_fns[] = {
  {"add",      l_shell_add},
  {"remove",   l_shell_remove},
  {"commands", l_shell_commands},
  {"help",     l_shell_help},
  {"alias",    l_shell_alias},
  {"exec",     l_shell_exec},
  {nullptr,    nullptr}
};

inline void registerShell(lua_State* L) {
  lua_newtable(L);
  for (const luaL_Reg* r = _shell_fns; r->name; r++) {
    lua_pushcfunction(L, r->func);
    lua_setfield(L, -2, r->name);
  }
  lua_setglobal(L, "shell");
}

} // namespace LuaShellBindings
