#pragma once
// ── lua_auto.h ────────────────────────────────────────────────────────────────
// NoorAuto — Lua automation / PyAutoGUI for TFT
//
// Lua API:
//   auto.tap(x, y)                    — tap screen at x,y
//   auto.swipe(x1,y1,x2,y2,[ms])      — swipe gesture
//   auto.type(text)                    — type text via virtual keyboard
//   auto.wait(ms)                      — wait milliseconds
//   auto.waitTouch([timeout])          — wait for any touch, returns {x,y}
//   auto.screenshot()                  — save screen to /screenshots/
//   auto.pixel(x,y)                    — get pixel color at x,y (RGB565)
//   auto.findColor(color,[tolerance])  — find first pixel matching color, returns {x,y} or nil
//   auto.record()                      — start recording touch sequence
//   auto.stopRecord(name)              — stop and save macro as name
//   auto.play(name,[loops])            — replay saved macro
//   auto.macros()                      — list saved macros
//   auto.if_pixel(x,y,color,fn)        — conditional: run fn if pixel matches
//   auto.loop(n, fn)                   — loop fn n times (-1 = forever until touch)
//   auto.on(event, fn)                 — hook: "touch","flame","obstacle","magnetic"
// ─────────────────────────────────────────────────────────────────────────────

#include <Arduino.h>
#include <SPIFFS.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

extern TFT_eSPI tft;
extern XPT2046_Touchscreen touch;

namespace LuaAuto {

// ── Macro recording ───────────────────────────────────────────────────────────
struct MacroEvent {
  String type;  // "tap", "swipe", "wait", "type"
  int x1, y1, x2, y2;
  unsigned long duration;
  String text;
};

bool _recording = false;
std::vector<MacroEvent> _recordBuffer;
unsigned long _recordStart = 0;
std::map<String, std::vector<MacroEvent>> _macros;
std::vector<std::function<void(int,int)>> _touchHooks;
std::vector<std::function<void()>> _flameHooks;
std::vector<std::function<void()>> _obstacleHooks;
std::vector<std::function<void()>> _magneticHooks;

void startRecord() {
  _recording = true;
  _recordBuffer.clear();
  _recordStart = millis();
}

String stopRecord(const String& name) {
  _recording = false;
  _macros[name] = _recordBuffer;
  // Save to SPIFFS as Lua
  String path = "/macros/" + name + ".lua";
  String src = "-- Macro: " + name + "\n";
  src += "return {\n";
  for (auto& e : _recordBuffer) {
    src += "  {type='" + e.type + "',x1=" + e.x1 + ",y1=" + e.y1;
    src += ",x2=" + e.x2 + ",y2=" + e.y2;
    src += ",duration=" + e.duration;
    if (!e.text.isEmpty()) src += ",text='" + e.text + "'";
    src += "},\n";
  }
  src += "}\n";
  File f = SPIFFS.open(path, "w");
  if (f) { f.print(src); f.close(); }
  return "Macro '" + name + "' saved (" + String(_recordBuffer.size()) + " events)\n";
}

void loadMacro(const String& name) {
  if (_macros.count(name)) return; // already loaded
  String path = "/macros/" + name + ".lua";
  // Would need Lua to parse — for now skip auto-load
}

String playMacro(const String& name, int loops = 1) {
  if (!_macros.count(name)) return "Macro '" + name + "' not found.\n";
  auto& events = _macros[name];
  int count = 0;
  while (loops < 0 || count < loops) {
    for (auto& e : events) {
      if (e.type == "wait") { delay(e.duration); }
      else if (e.type == "tap") {
        // Simulate touch — just fire hooks
        for (auto& h : _touchHooks) h(e.x1, e.y1);
        // Also draw visual feedback
        tft.fillCircle(e.x1, e.y1, 5, 0x07FF);
        delay(80);
        tft.fillCircle(e.x1, e.y1, 5, tft.readPixel(e.x1, e.y1));
        delay(e.duration);
      }
      else if (e.type == "swipe") { delay(e.duration); }
    }
    count++;
    // Check for cancel touch
    if (loops < 0 && touch.tirqTouched() && touch.touched()) break;
  }
  return "Macro '" + name + "' played " + String(count) + " time(s).\n";
}

String listMacros() {
  if (_macros.empty()) return "No macros recorded.\n";
  String out = "Saved macros:\n";
  for (auto& kv : _macros) out += "  " + kv.first + " (" + String(kv.second.size()) + " events)\n";
  return out;
}

String takeScreenshot() {
  // Save raw RGB565 pixels to SPIFFS
  String path = "/screenshots/scr_" + String(millis()) + ".raw";
  File f = SPIFFS.open(path, "w");
  if (!f) return "Screenshot failed.\n";
  for (int y=0; y<320; y++) {
    for (int x=0; x<240; x++) {
      uint16_t p = tft.readPixel(x, y);
      f.write(p >> 8); f.write(p & 0xFF);
    }
  }
  f.close();
  return path + "\n";
}

std::pair<int,int> findColor(uint16_t color, int tolerance=4) {
  for (int y=0; y<320; y++)
    for (int x=0; x<240; x++) {
      uint16_t p = tft.readPixel(x, y);
      int dr = abs(((p>>11)&0x1F)-((color>>11)&0x1F));
      int dg = abs(((p>>5)&0x3F)-((color>>5)&0x3F));
      int db = abs((p&0x1F)-(color&0x1F));
      if (dr<=tolerance && dg<=tolerance && db<=tolerance) return {x,y};
    }
  return {-1,-1};
}

void recordEvent(const String& type, int x1=0, int y1=0, int x2=0, int y2=0, unsigned long dur=0, const String& text="") {
  if (!_recording) return;
  MacroEvent e;
  e.type = type; e.x1=x1; e.y1=y1; e.x2=x2; e.y2=y2;
  e.duration = millis() - _recordStart; e.text = text;
  _recordBuffer.push_back(e);
  _recordStart = millis();
}

void fireEvent(const String& event, int x=0, int y=0) {
  if (event == "touch") for (auto& h : _touchHooks) h(x, y);
  if (event == "flame") for (auto& h : _flameHooks) h();
  if (event == "obstacle") for (auto& h : _obstacleHooks) h();
  if (event == "magnetic") for (auto& h : _magneticHooks) h();
}

} // namespace LuaAuto
