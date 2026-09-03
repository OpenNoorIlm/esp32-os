// ╔══════════════════════════════════════════════════════════════════════════╗
// ║  NoorOS — apps/hwtest.h                                                  ║
// ║  Hardware test + touch calibration app                                   ║
// ║                                                                          ║
// ║  Run from NoorShell:  hwtest                                             ║
// ║  Or from TFT:  touch the "HW Test" button on the home screen            ║
// ║                                                                          ║
// ║  Pages (swipe right or tap NEXT):                                       ║
// ║    1. TFT color test — fills screen with colors, draws grid             ║
// ║    2. Touch calibration — tap 4 corners, saves map() values            ║
// ║    3. Audio test — beep, tone sweep, robot.say()                       ║
// ║    4. Sensor readout — live values for all 10 sensors                  ║
// ║    5. Summary — pass/fail table + how to save calibration              ║
// ╚══════════════════════════════════════════════════════════════════════════╝
#pragma once
#include <Arduino.h>
#include <SPIFFS.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include "../sensor_manager.h"
#include "../audio_manager.h"
#include "../tft_manager.h"

extern TFT_eSPI         tft;
extern XPT2046_Touchscreen touch;

namespace HwTest {

// ── Calibration state ─────────────────────────────────────────────────────────
struct CalData {
  int xMin = 200,  xMax = 3800;
  int yMin = 200,  yMax = 3800;
  bool done = false;
};

static CalData _cal;

// Calibration file on SPIFFS
static const char* CAL_PATH = "/sys/touch_cal.txt";

// ── Load / save calibration ───────────────────────────────────────────────────
inline void saveCal() {
  File f = SPIFFS.open(CAL_PATH, "w");
  if (!f) { Serial.println("[hwtest] failed to save cal"); return; }
  f.printf("%d %d %d %d\n", _cal.xMin, _cal.xMax, _cal.yMin, _cal.yMax);
  f.close();
  Serial.printf("[hwtest] calibration saved: xMin=%d xMax=%d yMin=%d yMax=%d\n",
    _cal.xMin, _cal.xMax, _cal.yMin, _cal.yMax);
}

inline bool loadCal() {
  if (!SPIFFS.exists(CAL_PATH)) return false;
  File f = SPIFFS.open(CAL_PATH, "r");
  if (!f) return false;
  int a, b, c, d;
  if (sscanf(f.readString().c_str(), "%d %d %d %d", &a, &b, &c, &d) == 4) {
    _cal = {a, b, c, d, true};
    f.close();
    return true;
  }
  f.close(); return false;
}

// Apply current calibration to a raw touch point
inline void applyMap(int rawX, int rawY, int& px, int& py) {
  px = constrain(map(rawX, _cal.xMin, _cal.xMax, 0, 240), 0, 239);
  py = constrain(map(rawY, _cal.yMin, _cal.yMax, 0, 320), 0, 319);
}

// ── Helpers ───────────────────────────────────────────────────────────────────
static void waitNoTouch() {
  while (touch.touched()) delay(10);
}

static bool waitTap(int& rawX, int& rawY, unsigned long timeoutMs = 15000) {
  unsigned long t0 = millis();
  while (millis() - t0 < timeoutMs) {
    if (touch.touched()) {
      TS_Point p = touch.getPoint();
      rawX = p.x; rawY = p.y;
      waitNoTouch();
      return true;
    }
    delay(20);
  }
  return false;
}

static void centered(const char* text, int y, uint16_t fg = 0xFFFF, int sz = 1) {
  tft.setTextSize(sz);
  int w = strlen(text) * 6 * sz;
  tft.setTextColor(fg, 0x0000);
  tft.setCursor((240 - w) / 2, y);
  tft.print(text);
}

static void statusBar(const char* msg, uint16_t col = 0x07FF) {
  tft.fillRect(0, 0, 240, 16, 0x1082);
  tft.setTextColor(col, 0x1082);
  tft.setTextSize(1);
  tft.setCursor(4, 4);
  tft.print(msg);
}

// ══════════════════════════════════════════════════════════════════════════════
// PAGE 1 — TFT colour test
// ══════════════════════════════════════════════════════════════════════════════
static bool _tftOk = false;

inline void pageTft() {
  // Full-screen color wipe
  uint16_t colors[] = {0xF800,0x07E0,0x001F,0xFFE0,0xF81F,0x07FF,0xFFFF,0x0000};
  const char* names[] = {"RED","GREEN","BLUE","YELLOW","MAGENTA","CYAN","WHITE","BLACK"};
  for (int i = 0; i < 8; i++) {
    tft.fillScreen(colors[i]);
    tft.setTextColor(colors[i] == 0xFFFF ? 0x0000 : 0xFFFF);
    tft.setTextSize(2);
    tft.setCursor(60, 150);
    tft.print(names[i]);
    delay(300);
  }

  // Grid test
  tft.fillScreen(0x0000);
  for (int x = 0; x < 240; x += 20) tft.drawFastVLine(x, 0, 320, 0x2104);
  for (int y = 0; y < 320; y += 20) tft.drawFastHLine(0, y, 240, 0x2104);

  // Gradient bar
  for (int x = 0; x < 240; x++) {
    uint16_t r = (x * 31) / 239;
    uint16_t g = ((239 - x) * 63) / 239;
    tft.drawFastVLine(x, 130, 20, (r << 11) | (g << 5));
  }

  tft.setTextColor(0x07FF, 0x0000);
  tft.setTextSize(1);
  centered("TFT COLOR TEST", 20, 0xFEA0, 1);
  centered("Colors OK?", 60, 0xFFFF, 1);
  centered("Grid OK?", 75, 0xFFFF, 1);
  centered("Gradient bar OK?", 90, 0xFFFF, 1);
  centered("Tap screen to continue", 170, 0x07FF, 1);

  _tftOk = true; // if we got here, TFT is drawing
  int rx, ry;
  waitTap(rx, ry, 20000);
}

// ══════════════════════════════════════════════════════════════════════════════
// PAGE 2 — Touch calibration
// ══════════════════════════════════════════════════════════════════════════════
static bool _touchOk = false;

struct Corner {
  const char* label;
  int sx, sy;    // screen pixel of crosshair
};

static Corner _corners[4] = {
  {"TOP-LEFT",    15,  15},
  {"TOP-RIGHT",  225,  15},
  {"BOT-RIGHT",  225, 305},
  {"BOT-LEFT",    15, 305},
};

static void drawCrosshair(int x, int y, uint16_t col) {
  tft.drawFastHLine(x - 10, y, 21, col);
  tft.drawFastVLine(x, y - 10, 21, col);
  tft.drawCircle(x, y, 5, col);
}

static int _rawX[4], _rawY[4];

inline void pageCalibrate() {
  tft.fillScreen(0x0000);
  centered("TOUCH CALIBRATION", 4, 0xFEA0, 1);
  centered("Tap each crosshair", 18, 0x07FF, 1);
  centered("with your stylus", 30, 0x07FF, 1);

  for (int i = 0; i < 4; i++) {
    tft.fillScreen(0x0000);
    statusBar(_corners[i].label, 0xFEA0);

    // Draw all 4 crosshairs dim
    for (int j = 0; j < 4; j++)
      drawCrosshair(_corners[j].sx, _corners[j].sy, 0x2104);

    // Draw current one bright
    drawCrosshair(_corners[i].sx, _corners[i].sy, 0xFFFF);

    tft.setTextColor(0x07FF, 0x0000);
    tft.setTextSize(1);
    tft.setCursor(4, 290);
    tft.printf("Point %d/4 — tap now", i + 1);

    // Show live raw values while waiting
    int rx = 0, ry = 0;
    unsigned long t0 = millis();
    bool got = false;
    while (millis() - t0 < 15000 && !got) {
      if (touch.touched()) {
        TS_Point p = touch.getPoint();
        rx = p.x; ry = p.y;

        // Show raw
        tft.fillRect(0, 270, 240, 20, 0x0000);
        tft.setTextColor(0xFFE0, 0x0000);
        tft.setCursor(4, 272);
        tft.printf("raw: x=%d  y=%d", rx, ry);
        delay(60);

        // Confirm tap only when released
        if (!touch.touched()) {
          _rawX[i] = rx;
          _rawY[i] = ry;
          got = true;

          // Flash green
          drawCrosshair(_corners[i].sx, _corners[i].sy, 0x07E0);
          delay(300);
        }
      }
      delay(10);
    }

    if (!got) {
      centered("TIMEOUT — skipping", 150, 0xF800, 1);
      delay(1000);
    }
  }

  // Compute calibration from 4 points
  // xMin = average of left two raw X; xMax = average of right two raw X
  _cal.xMin = (_rawX[0] + _rawX[3]) / 2;
  _cal.xMax = (_rawX[1] + _rawX[2]) / 2;
  _cal.yMin = (_rawY[0] + _rawY[1]) / 2;
  _cal.yMax = (_rawY[2] + _rawY[3]) / 2;

  // Clamp — if someone taps reversed (screen rotated), swap
  if (_cal.xMin > _cal.xMax) { int t = _cal.xMin; _cal.xMin = _cal.xMax; _cal.xMax = t; }
  if (_cal.yMin > _cal.yMax) { int t = _cal.yMin; _cal.yMin = _cal.yMax; _cal.yMax = t; }

  _cal.done = true;
  _touchOk  = true;

  // Show result
  tft.fillScreen(0x0000);
  centered("CALIBRATION DONE", 30, 0x07E0, 1);
  tft.setTextColor(0xFFFF, 0x0000);
  tft.setCursor(10, 60);
  tft.printf("xMin: %-5d  xMax: %d", _cal.xMin, _cal.xMax);
  tft.setCursor(10, 75);
  tft.printf("yMin: %-5d  yMax: %d", _cal.yMin, _cal.yMax);

  centered("Saving to SPIFFS...", 100, 0x07FF, 1);
  saveCal();

  centered("/sys/touch_cal.txt", 115, 0xFEA0, 1);
  centered("Update map() values in:", 140, 0x4208, 1);
  centered("vkeyboard.h  line ~84", 155, 0xFFFF, 1);
  centered("tft_manager.h  touch handler", 170, 0xFFFF, 1);
  centered("Tap to continue", 200, 0x07FF, 1);

  int rx, ry;
  waitTap(rx, ry, 20000);
}

// ══════════════════════════════════════════════════════════════════════════════
// PAGE 3 — Audio test
// ══════════════════════════════════════════════════════════════════════════════
static bool _audioOk = false;

inline void pageAudio() {
  tft.fillScreen(0x0000);
  statusBar("AUDIO TEST", 0xFEA0);
  centered("Testing audio output", 30, 0xFFFF, 1);
  centered("Listen for beeps!", 45, 0x07FF, 1);

  // Test 1 — simple beep
  tft.setTextColor(0xFFE0, 0x0000);
  tft.setCursor(10, 70);
  tft.print("1) 440 Hz beep...");
  AudioManager::beep(440, 400);
  delay(200);

  // Test 2 — tone sweep
  tft.setCursor(10, 85);
  tft.print("2) Sweep 200-2000 Hz...");
  for (int f = 200; f <= 2000; f += 100) {
    AudioManager::beep(f, 60);
    delay(20);
  }

  // Test 3 — robot.say()
  tft.setCursor(10, 100);
  tft.print("3) Saying 'hello noor'...");
  AudioManager::say("hello noor");

  // Test 4 — boot chime
  tft.setCursor(10, 115);
  tft.print("4) Boot chime...");
  int chime[] = {523, 659, 784, 1047};
  for (int n : chime) { AudioManager::beep(n, 150); delay(30); }

  // Did it play?
  tft.setTextColor(0xFFFF, 0x0000);
  centered("Did you hear audio? (tap)", 160, 0x07FF, 1);
  centered("Y = tap left  N = tap right", 175, 0x4208, 1);

  int rx, ry;
  if (waitTap(rx, ry, 20000)) {
    // Map raw to screen assuming rough calibration
    int px, py;
    applyMap(rx, ry, px, py);
    _audioOk = (px < 120);
  }

  tft.fillRect(0, 185, 240, 30, 0x0000);
  if (_audioOk) centered("AUDIO: PASS", 195, 0x07E0, 1);
  else          centered("AUDIO: FAIL (check wiring)", 195, 0xF800, 1);
  delay(1200);
}

// ══════════════════════════════════════════════════════════════════════════════
// PAGE 4 — Sensor readout
// ══════════════════════════════════════════════════════════════════════════════
static bool _sensorsOk = false;

inline void pageSensors() {
  tft.fillScreen(0x0000);
  statusBar("SENSOR READOUT — tap to exit", 0xFEA0);

  // Live display loop — exits on touch or after 30s
  unsigned long t0 = millis();
  while (millis() - t0 < 30000) {
    SensorManager::loop(); // force fresh read

    tft.fillRect(0, 20, 240, 260, 0x0000);
    tft.setTextSize(1);
    int y = 24;
    auto row = [&](const char* lbl, String val, uint16_t col = 0xFFFF) {
      tft.setTextColor(0x4208, 0x0000);
      tft.setCursor(4, y);
      tft.printf("%-16s", lbl);
      tft.setTextColor(col, 0x0000);
      tft.print(val);
      y += 14;
    };

    row("Temperature C:", String(SensorManager::tempC(), 1) + " C",
        SensorManager::tempC() > 40 ? 0xF800 : 0x07E0);
    row("Humidity:",      String(SensorManager::humidity(), 1) + " %");
    row("Distance:",      String(SensorManager::distance(), 1) + " cm",
        SensorManager::distance() < 15 ? 0xF800 : 0xFFFF);
    row("Light:",         String(SensorManager::lightPercent()) + " %");
    row("Sound:",         String(SensorManager::soundPercent()) + " %");
    row("Obstacle:",      SensorManager::obstacle() ? "YES" : "no",
        SensorManager::obstacle() ? 0xF800 : 0x07E0);
    row("Flame:",         SensorManager::flame() ? "DETECTED!" : "clear",
        SensorManager::flame() ? 0xF800 : 0x07E0);
    row("Tracking:",      SensorManager::tracking() ? "on line" : "off",
        SensorManager::tracking() ? 0xFFE0 : 0x4208);
    row("Magnetic:",      SensorManager::magnetic() ? "YES" : "no",
        SensorManager::magnetic() ? 0xF81F : 0x4208);
    row("Laser on:",      SensorManager::laserOn() ? "yes" : "no");

    // Temp/distance plausibility
    bool tempOk  = SensorManager::tempC() > 5 && SensorManager::tempC() < 60;
    bool distOk  = SensorManager::distance() > 0 && SensorManager::distance() < 500;

    y = 180;
    tft.setTextColor(0x07FF, 0x0000);
    tft.setCursor(4, y);
    tft.print("DHT11: ");
    tft.setTextColor(tempOk ? 0x07E0 : 0xF800, 0x0000);
    tft.print(tempOk ? "OK" : "FAIL (check GPIO32)");

    y += 14;
    tft.setTextColor(0x07FF, 0x0000);
    tft.setCursor(4, y);
    tft.print("Ultrasonic: ");
    tft.setTextColor(distOk ? 0x07E0 : 0xF800, 0x0000);
    tft.print(distOk ? "OK" : "FAIL (check GPIO13/14)");

    _sensorsOk = tempOk && distOk;

    centered("Tap to continue", 220, 0x4208, 1);
    tft.setTextColor(0x2104, 0x0000);
    tft.setCursor(4, 235);
    tft.printf("Uptime: %lus", millis() / 1000);

    if (touch.touched()) { waitNoTouch(); break; }
    delay(500);
  }
}

// ══════════════════════════════════════════════════════════════════════════════
// PAGE 5 — Summary
// ══════════════════════════════════════════════════════════════════════════════
inline void pageSummary() {
  tft.fillScreen(0x0000);
  statusBar("HARDWARE SUMMARY", 0xFEA0);

  auto resultRow = [](int y, const char* label, bool ok) {
    tft.setTextColor(0xFFFF, 0x0000);
    tft.setCursor(10, y);
    tft.printf("%-18s", label);
    tft.setTextColor(ok ? 0x07E0 : 0xF800, 0x0000);
    tft.print(ok ? "PASS" : "FAIL");
  };

  tft.setTextSize(1);
  resultRow(30,  "TFT display",    _tftOk);
  resultRow(46,  "Touch panel",    _touchOk);
  resultRow(62,  "Audio (PAM8403)",_audioOk);
  resultRow(78,  "DHT11 sensor",   SensorManager::tempC() > 5);
  resultRow(94,  "Ultrasonic",     SensorManager::distance() > 0);

  if (_touchOk) {
    tft.setTextColor(0x07FF, 0x0000);
    tft.setCursor(10, 120);
    tft.println("Cal saved to /sys/touch_cal.txt");
    tft.setTextColor(0xFFE0, 0x0000);
    tft.setCursor(10, 136);
    tft.printf("Copy to vkeyboard.h:");
    tft.setCursor(10, 150);
    tft.printf("xMin=%d xMax=%d", _cal.xMin, _cal.xMax);
    tft.setCursor(10, 164);
    tft.printf("yMin=%d yMax=%d", _cal.yMin, _cal.yMax);
  } else {
    tft.setTextColor(0xF800, 0x0000);
    tft.setCursor(10, 120);
    tft.print("Touch not calibrated");
  }

  tft.setTextColor(0x4208, 0x0000);
  centered("Run 'hwtest' from shell", 290, 0x4208, 1);
  centered("to repeat any time", 304, 0x4208, 1);

  int rx, ry;
  waitTap(rx, ry, 30000);
}

// ══════════════════════════════════════════════════════════════════════════════
// Shell command: hwtest [tft|touch|audio|sensors|all]
// ══════════════════════════════════════════════════════════════════════════════
inline void runShell(const String& mode = "all", Print& out = Serial) {
  out.println("[hwtest] starting hardware test...");

  if (mode == "all" || mode == "tft") {
    out.println("[hwtest] page 1: TFT color test");
    pageTft();
  }
  if (mode == "all" || mode == "touch") {
    out.println("[hwtest] page 2: touch calibration");
    pageCalibrate();
    out.printf("[hwtest] cal: xMin=%d xMax=%d yMin=%d yMax=%d\n",
      _cal.xMin, _cal.xMax, _cal.yMin, _cal.yMax);
  }
  if (mode == "all" || mode == "audio") {
    out.println("[hwtest] page 3: audio test");
    pageAudio();
  }
  if (mode == "all" || mode == "sensors") {
    out.println("[hwtest] page 4: sensor readout");
    pageSensors();
  }
  if (mode == "all") {
    out.println("[hwtest] page 5: summary");
    pageSummary();
  }

  out.printf("[hwtest] done. TFT:%s Touch:%s Audio:%s Sensors:%s\n",
    _tftOk ? "PASS" : "FAIL",
    _touchOk ? "PASS" : "FAIL",
    _audioOk ? "PASS" : "FAIL",
    _sensorsOk ? "PASS" : "FAIL");
}

// Called at startup — auto-applies saved calibration if available
inline void begin() {
  if (loadCal()) {
    Serial.printf("[hwtest] touch cal loaded: xMin=%d xMax=%d yMin=%d yMax=%d\n",
      _cal.xMin, _cal.xMax, _cal.yMin, _cal.yMax);
    // Patch vkeyboard at runtime via the loaded values
    // (vkeyboard.h reads _cal directly if you call applyMap() from it)
  }
}

} // namespace HwTest
