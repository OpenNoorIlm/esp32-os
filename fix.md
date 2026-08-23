# NoorOS ESP32 Build Fix Tracker

## Status Legend
- [ ] Not started
- [~] In progress
- [x] Done

---

## Fix 1 — `std::map` missing in `nano_editor.h` [x]
**File:** `nano_editor.h`
**Line:** ~19 (after `#include "vkeyboard.h"`)
**Fix:** Add `#include <map>` — was supposedly done last session but error reappeared; confirm it landed.

---

## Fix 2 — `touch` / `TS_Point` not declared in `driver_manager.h` [ ]
**File:** `driver_manager.h`
**Lines:** 217, 264 (tftAskReboot, tftErrorScreen)
**Fix:** Add after the existing `#include <TFT_eSPI.h>` line:
```cpp
#include <XPT2046_Touchscreen.h>
extern XPT2046_Touchscreen touch;
```
Note: `extern TFT_eSPI tft;` is already there. Add the touch extern right below it.

---

## Fix 3 — `LuaEngine::eval(code, String&)` Print& mismatch [ ]
**Files:** `driver_manager.h` (lines 182, 303, 413, 449), `apps/taskmanager.h` (line 460)
**Fix:** Add `StringPrint` adapter to `lua_engine.h` inside `namespace LuaEngine`:
```cpp
// Wraps an Arduino String as a Print& so it can be passed to eval().
struct StringPrint : public Print {
  String& s;
  explicit StringPrint(String& str) : s(str) {}
  size_t write(uint8_t c) override { s += (char)c; return 1; }
  size_t write(const uint8_t* buf, size_t size) override {
    for (size_t i = 0; i < size; i++) s += (char)buf[i];
    return size;
  }
};
```
Then at each call site, change:
```cpp
String out;
LuaEngine::eval(code, out);          // BEFORE (broken)
```
to:
```cpp
String out;
LuaEngine::StringPrint _sp(out);
LuaEngine::eval(code, _sp);         // AFTER
```
Do this for ALL 5 call sites.

---

## Fix 4 — `FsManager::resolve()` doesn't exist [ ]
**Files:** `shell_server.h` (line 183), `nano_editor.h` (line 478)

### shell_server.h:183-184
Replace:
```cpp
String path = FsManager::resolve(rest);
String sid  = client.remoteIP().toString();
```
With:
```cpp
String path = FsManager::toRealPath(cwd, rest);
String sid  = cwd; // use cwd as session key; no WiFiClient in scope here
```

### nano_editor.h:478
Replace:
```cpp
String path = FsManager::resolve(args);
```
With:
```cpp
String path = FsManager::toRealPath("/", args);
```

---

## Fix 5 — C-string + int concatenation in `lua_auto.h` [ ]
**File:** `lua_auto.h`
**Line:** ~65 (stopRecord function)
**Error:** `invalid operands of types 'const char*' and 'const char [5]' to binary 'operator+'`
Replace:
```cpp
src += ",x2=" + e.x2 + ",y2=" + e.y2;
```
With:
```cpp
src += ",x2=" + String(e.x2) + ",y2=" + String(e.y2);
```
Also check the line above it for `x1` and `y1` — same pattern:
```cpp
src += "  {type='" + e.type + "',x1=" + String(e.x1) + ",y1=" + String(e.y1);
src += ",x2=" + String(e.x2) + ",y2=" + String(e.y2);
```

---

## Fix 6 — `quran.h` range-for / `.as<String>()` parse errors [ ]
**File:** `apps/quran.h`
**Lines:** 158-160, 174-178
**Cause:** Range-for over ArduinoJson proxy type conflicts with the many local `begin()` functions in scope. The `as<String>()` template syntax also fails when the parser is confused by this.
**Fix:** Replace range-for with indexed loops.

### surah block (line ~158):
```cpp
// BEFORE
for (auto a : ayahs) {
  out += "[" + a["numberInSurah"].as<String>() + "] ";
  out += a["text"].as<String>() + "\n\n";
}
// AFTER
for (int _i = 0; _i < (int)ayahs.size(); _i++) {
  out += "[" + String(ayahs[_i]["numberInSurah"].as<int>()) + "] ";
  out += String(ayahs[_i]["text"].as<const char*>()) + "\n\n";
}
```

### search/matches block (line ~174):
```cpp
// BEFORE
for (auto m : matches) {
  int sn = m["surah"]["number"];
  int an = m["numberInSurah"];
  out += String(sn) + ":" + String(an) + " [" + SURAH_NAMES[sn] + "] ";
  out += m["text"].as<String>() + "\n\n";
}
// AFTER
for (int _i = 0; _i < (int)matches.size(); _i++) {
  int sn = matches[_i]["surah"]["number"].as<int>();
  int an = matches[_i]["numberInSurah"].as<int>();
  out += String(sn) + ":" + String(an) + " [" + SURAH_NAMES[sn] + "] ";
  out += String(matches[_i]["text"].as<const char*>()) + "\n\n";
}
```

---

## Fix 7 — `runCommand()` called with too few args in `esp32.ino` [ ]
**File:** `esp32.ino`
**Line:** ~90
**Error:** `too few arguments to function 'String ShellServer::runCommand(const String&, String&, Print&)'`
Replace:
```cpp
ShellServer::runCommand("robot " + cmd + " 1");
```
With:
```cpp
{ String _cwd = "/"; ShellServer::runCommand("robot " + cmd + " 1", _cwd, Serial); }
```

---

## Fix 8 — `dynamic_cast` with `-fno-rtti` in `lua_engine.cpp` [ ]
**File:** `lua_engine.cpp`
**Lines:** 613-616, 623-624, 631-632, 640-642, 649-650, 656-657, 664-665, 673-674, 683, 691
**Cause:** ESP32 Arduino disables RTTI; `dynamic_cast` is illegal.

**Strategy:** Add a `widgetType()` virtual method to the NoorUI Widget base class in `lua_widgets.h`, then replace every `dynamic_cast<NoorUI::Foo*>(w)` in `lua_engine.cpp` with a type-tag check + `static_cast`.

### Step A — find the Widget base struct/class in `lua_widgets.h`
Add an enum and virtual method:
```cpp
enum class WType { Base, Label, Button, TextInput, PasswordInput,
                   CheckBox, RadioButton, Slider, Spinner, ProgressBar,
                   Switch, ComboBox, ListBox, Panel, TabWidget,
                   ScrollArea, Spacer, Image, ColorPicker, Separator,
                   Badge, Container };

// Inside the Widget base:
virtual WType widgetType() const { return WType::Base; }
// Each subclass overrides, e.g.:
// struct Label : Widget { WType widgetType() const override { return WType::Label; } ... };
```

### Step B — replace dynamic_cast in lua_engine.cpp
Pattern:
```cpp
// BEFORE
if (auto* l = dynamic_cast<NoorUI::Label*>(w)) l->setText(t);
// AFTER
if (w->widgetType() == NoorUI::WType::Label) static_cast<NoorUI::Label*>(w)->setText(t);
```
Apply to every occurrence.

---

## Order to Apply
1. [x] Fix 1 — nano_editor.h map include (confirm)
2. [ ] Fix 2 — driver_manager.h touch extern
3. [ ] Fix 3 — lua_engine.h StringPrint + 5 call sites
4. [ ] Fix 4 — FsManager::resolve → toRealPath (2 sites)
5. [ ] Fix 5 — lua_auto.h String(int) concatenation
6. [ ] Fix 6 — quran.h indexed loops
7. [ ] Fix 7 — esp32.ino runCommand args
8. [ ] Fix 8 — lua_engine.cpp dynamic_cast → widgetType()
