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

// Runs one chunk of Lua source. print() output streams live to `out` (the
// shell's TCP client) as it happens -- same live-streaming pattern used by
// package/OS installs -- rather than being buffered and dumped all at once
// after the whole script finishes. Returns "" on success (output already
// went to `out`), or "lua error: ..." on a Lua-side error.
String eval(const String& code, Print& out);

// Runs an installed app's entrypoint: reads /apps/<name>/main.lua and
// executes it exactly like `lua load(esp32.fs.cat("/apps/<name>/main.lua"))()`
// would from an interactive session, streaming print() output live to `out`.
// Returns "error: ..." if the app isn't installed (no main.lua present) or
// if the script raises a Lua-side error; otherwise "".
String runApp(const String& appName, Print& out);

} // namespace LuaEngine
