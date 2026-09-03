// ╔══════════════════════════════════════════════════════════════════════╗
// ║  NoorOS — lua_config.h                                               ║
// ║  Include this BEFORE any Lua header in every translation unit that   ║
// ║  uses the Lua C API (but does NOT compile onelua.c itself).         ║
// ║  lua_engine.cpp defines these before #include onelua.c as well.     ║
// ╚══════════════════════════════════════════════════════════════════════╝
#pragma once

// Use 32-bit integers and floats instead of int64/double.
// Saves ~8 bytes per Lua value — critical on ESP32's limited heap.
// Remove if you ever need 64-bit precision.
#ifndef LUA_32BITS
#  define LUA_32BITS
#endif

// Suppress Lua's standalone main() — we embed the VM only.
#ifndef MAKE_LIB
#  define MAKE_LIB
#endif
