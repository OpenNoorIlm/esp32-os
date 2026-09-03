#pragma once
#include <Arduino.h>

// ── Lua build options ─────────────────────────────────────────────────────────
// Must be defined BEFORE any lua_src header is included, anywhere in the
// sketch. Putting them here (in lua_engine.h, which every Lua-touching header
// includes transitively) ensures they are always set first, regardless of
// include order. lua_engine.cpp also defines them locally before onelua.c,
// which is fine — identical macro redefinitions are not an error.
#ifndef LUA_32BITS
#  define LUA_32BITS   // use 32-bit ints/floats — meaningfully less RAM on ESP32
#endif
#ifndef MAKE_LIB
#  define MAKE_LIB     // onelua.c: build as library (no main()), included only by lua_engine.cpp
#endif

// Thin C++ facade over the embedded Lua interpreter (lua_src/lua-master,
// built via lua_engine.cpp as a single-translation-unit "onelua.c" library).
// Keeps every raw lua_State*/lua_* C-API detail out of shell_server.h.

// ── StringPrint ───────────────────────────────────────────────────────────────
// Adapts Print& to String for callers that need eval() output as a String.
// Usage:  StringPrint sp; LuaEngine::eval(code, sp); String result = sp.buf;
class StringPrint : public Print {
public:
  size_t write(uint8_t c) override { buf += (char)c; return 1; }
  size_t write(const uint8_t* b, size_t n) override {
    buf.reserve(buf.length()+n);
    for (size_t i=0;i<n;i++) buf+=(char)b[i];
    return n;
  }
  String buf;
};

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
