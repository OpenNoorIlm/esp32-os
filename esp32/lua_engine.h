#pragma once
#include <Arduino.h>

// Thin C++ facade over the embedded Lua interpreter (lua_src/lua-master,
// built via lua_engine.cpp as a single-translation-unit "onelua.c" library).
// Keeps every raw lua_State*/lua_* C-API detail out of shell_server.h.
namespace LuaEngine {

// Lazily creates the global lua_State on first use (also callable up front
// from setup() if you'd rather pay the ~few-KB init cost at boot instead of
// on first "lua" command).
void begin();

// Runs one chunk of Lua source. print() output is captured (there's no real
// stdout attached to this shell) and returned as the result string; on a
// Lua-side error, the error message is appended instead.
String eval(const String& code);

} // namespace LuaEngine
