#pragma once
// ── tft_manager.h ─────────────────────────────────────────────────────────────
// Bazzaar TFT display for NoorRobot
// Screen: 2.8" ILI9341 240x320 SPI + XPT2046 touch
//
// Layout (320px tall, 240px wide):
//   [0-40]    Status bar  — IP, WiFi signal, uptime
//   [40-180]  Eyes panel  — big animated color eyes
//   [180-240] Sensor bar  — temp, distance, light, flame icons
//   [240-320] Touch buttons — Forward / Back / Left / Right / Stop
//
// Libraries needed (install via Arduino Library Manager):
//   TFT_eSPI   (by Bodmer) — configure for ILI9341 in User_Setup.h
//   XPT2046_Touchscreen (by Paul Stoffregen)
// ─────────────────────────────────────────────────────────────────────────────

#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <WiFi.h>

// ── Pin definitions ───────────────────────────────────────────────────────────
// TFT SPI (configured in TFT_eSPI User_Setup.h — set these there too)
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST   4
#define TFT_MOSI 23
#define TFT_MISO 19
#define TFT_CLK  18

// Touch
#define TOUCH_CS  5
#define TOUCH_IRQ 27

// Sensors (all on ESP32 now)
#define DHT_PIN      32
#define AVOID_PIN    33   // IR obstacle
#define TRACK_PIN    34   // line tracking
#define FLAME_PIN    35   // flame sensor
#define LIGHT_PIN    36   // photoresistor (analog)
#define SOUND_PIN    39   // sound sensor (analog)
#define HALL_PIN     25   // hall/magnetic sensor
#define LASER_PIN    26   // laser emit (digital out)
#define USD_TRIG_PIN 13   // ultrasonic trig (moved from Arduino)
#define USD_ECHO_PIN 14   // ultrasonic echo (moved from Arduino)
#define SERVO_PIN    12   // servo (moved from Arduino)

// ── Colors (RGB565) ───────────────────────────────────────────────────────────
#define CLR_BG        0x0000   // black background
#define CLR_STATUS    0x4208   // dark grey status bar
#define CLR_WHITE     0xFFFF
#define CLR_NEON_BLUE 0x07FF   // cyan
#define CLR_NEON_PINK 0xF81F   // magenta
#define CLR_NEON_GRN  0x07E0   // green
#define CLR_NEON_YEL  0xFFE0   // yellow
#define CLR_NEON_ORG  0xFC60   // orange
#define CLR_RED       0xF800
#define CLR_PURPLE    0x801F
#define CLR_GOLD      0xFEA0

// ── Eye color per emotion ─────────────────────────────────────────────────────
struct EyeStyle {
  uint16_t iris;
  uint16_t pupil;
  uint16_t sclera;
  uint16_t glow;
};

EyeStyle getEyeStyle(const String& type) {
  if (type == "Happy")     return {CLR_NEON_GRN,  0x0200, CLR_WHITE,  CLR_NEON_GRN};
  if (type == "Sad")       return {CLR_NEON_BLUE,  0x0010, CLR_WHITE,  CLR_NEON_BLUE};
  if (type == "Angry")     return {CLR_RED,         0x4000, CLR_WHITE,  CLR_RED};
  if (type == "Surprised") return {CLR_NEON_YEL,   0x8400, CLR_WHITE,  CLR_NEON_YEL};
  if (type == "Love")      return {CLR_NEON_PINK,  0x8008, CLR_WHITE,  CLR_NEON_PINK};
  if (type == "Sleepy")    return {CLR_PURPLE,      0x4004, 0xC618,     CLR_PURPLE};
  if (type == "Evil")      return {CLR_RED,         0x4000, 0x2104,     CLR_RED};
  if (type == "Cool")      return {CLR_NEON_BLUE,  0x0010, 0x2104,     CLR_NEON_BLUE};
  if (type == "Excited")   return {CLR_NEON_ORG,   0x8200, CLR_WHITE,  CLR_NEON_ORG};
  if (type == "Nervous")   return {CLR_NEON_YEL,   0x8400, CLR_WHITE,  CLR_NEON_YEL};
  if (type == "Wink")      return {CLR_NEON_GRN,  0x0200, CLR_WHITE,  CLR_NEON_GRN};
  if (type == "Dead")      return {0x4208,          0x2104, 0x4208,     0x2104};
  if (type == "Cry")       return {CLR_NEON_BLUE,  0x0010, CLR_WHITE,  CLR_NEON_BLUE};
  if (type == "Bored")     return {0x4208,          0x2104, CLR_WHITE,  0x4208};
  if (type == "Confused")  return {CLR_NEON_ORG,   0x8200, CLR_WHITE,  CLR_NEON_ORG};
  if (type == "Dizzy")     return {CLR_NEON_PINK,  0x8008, CLR_WHITE,  CLR_NEON_PINK};
  if (type == "Shy")       return {CLR_NEON_PINK,  0x8008, CLR_WHITE,  CLR_NEON_PINK};
  // Normal default
  return {CLR_NEON_BLUE, 0x0010, CLR_WHITE, CLR_NEON_BLUE};
}

// ── TFT + Touch objects ───────────────────────────────────────────────────────
TFT_eSPI        tft;
XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);

// ── State ─────────────────────────────────────────────────────────────────────
namespace TftManager {

String  _currentEye   = "Normal";
int     _eyeOffX      = 0;
int     _eyeOffY      = 0;
float   _temperature  = 0;
float   _distance     = 0;
int     _lightLevel   = 0;
bool    _flameDetected = false;
bool    _obstacleDetected = false;
unsigned long _lastBlink  = 0;
bool    _eyeOpen      = true;
unsigned long _lastEyeAnim = 0;
int     _pupilDx      = 0;
int     _pupilDy      = 0;

// Touch button callback — set this from esp32.ino
std::function<void(String)> _onTouchCommand = nullptr;

void setTouchCallback(std::function<void(String)> cb) {
  _onTouchCommand = cb;
}

// ── Drawing helpers ───────────────────────────────────────────────────────────

void drawGlow(int x, int y, int r, uint16_t color) {
  // Multi-ring glow effect
  for (int i = 3; i >= 1; i--) {
    uint16_t dimmed = (((color >> 11) & 0x1F) >> i) << 11 |
                      (((color >> 5)  & 0x3F) >> i) << 5  |
                      (((color)       & 0x1F) >> i);
    tft.drawCircle(x, y, r + i * 3, dimmed);
    tft.drawCircle(x, y, r + i * 3 + 1, dimmed);
  }
}

// ── Status bar (top 40px) ─────────────────────────────────────────────────────
void drawStatusBar() {
  tft.fillRect(0, 0, 240, 40, CLR_STATUS);
  tft.drawFastHLine(0, 40, 240, CLR_NEON_BLUE);

  // IP address
  tft.setTextColor(CLR_NEON_BLUE, CLR_STATUS);
  tft.setTextSize(1);
  tft.setCursor(4, 4);
  tft.print("IP:");
  tft.setTextColor(CLR_WHITE, CLR_STATUS);
  tft.print(WiFi.localIP().toString());

  // WiFi signal strength
  int rssi = WiFi.RSSI();
  uint16_t sigColor = rssi > -60 ? CLR_NEON_GRN : rssi > -75 ? CLR_NEON_YEL : CLR_RED;
  tft.setCursor(4, 18);
  tft.setTextColor(CLR_NEON_BLUE, CLR_STATUS);
  tft.print("WiFi:");
  tft.setTextColor(sigColor, CLR_STATUS);
  tft.print(String(rssi) + "dBm");

  // Uptime
  unsigned long s = millis() / 1000;
  String uptime = String(s / 3600) + "h " + String((s % 3600) / 60) + "m";
  tft.setTextColor(CLR_NEON_BLUE, CLR_STATUS);
  tft.setCursor(140, 4);
  tft.print("UP:");
  tft.setTextColor(CLR_WHITE, CLR_STATUS);
  tft.print(uptime);

  // Obstacle / flame alerts
  tft.setCursor(140, 18);
  if (_flameDetected) {
    tft.setTextColor(CLR_RED, CLR_STATUS);
    tft.print("FLAME!");
  } else if (_obstacleDetected) {
    tft.setTextColor(CLR_NEON_YEL, CLR_STATUS);
    tft.print("AVOID!");
  } else {
    tft.setTextColor(CLR_NEON_GRN, CLR_STATUS);
    tft.print("CLEAR ");
  }
}

// ── Eyes panel (y: 45-185) ────────────────────────────────────────────────────
void drawEyesPanel(const String& type, int ox, int oy) {
  tft.fillRect(0, 45, 240, 140, CLR_BG);

  EyeStyle s = getEyeStyle(type);
  int lx = 65 + ox,  ly = 115 + oy;
  int rx = 175 + ox, ry = 115 + oy;

  if (type == "Normal") {
    drawGlow(lx, ly, 28, s.glow);
    drawGlow(rx, ry, 28, s.glow);
    tft.fillRoundRect(lx-28, ly-20, 56, 40, 12, s.sclera);
    tft.fillRoundRect(rx-28, ry-20, 56, 40, 12, s.sclera);
    tft.fillCircle(lx + _pupilDx + 4, ly + _pupilDy + 4, 14, s.iris);
    tft.fillCircle(rx + _pupilDx + 4, ry + _pupilDy + 4, 14, s.iris);
    tft.fillCircle(lx + _pupilDx + 4, ly + _pupilDy + 4, 7,  s.pupil);
    tft.fillCircle(rx + _pupilDx + 4, ry + _pupilDy + 4, 7,  s.pupil);
    tft.fillCircle(lx + _pupilDx + 9, ly + _pupilDy - 2, 3,  CLR_WHITE);
    tft.fillCircle(rx + _pupilDx + 9, ry + _pupilDy - 2, 3,  CLR_WHITE);

  } else if (type == "Happy") {
    drawGlow(lx, ly, 28, s.glow);
    drawGlow(rx, ry, 28, s.glow);
    tft.fillCircle(lx, ly, 28, s.sclera);
    tft.fillCircle(rx, ry, 28, s.sclera);
    tft.fillRect(lx-30, ly-30, 62, 32, CLR_BG);
    tft.fillRect(rx-30, ry-30, 62, 32, CLR_BG);

  } else if (type == "Sad") {
    drawGlow(lx, ly, 24, s.glow);
    drawGlow(rx, ry, 24, s.glow);
    tft.fillRoundRect(lx-26, ly-18, 52, 36, 10, s.sclera);
    tft.fillRoundRect(rx-26, ry-18, 52, 36, 10, s.sclera);
    tft.fillTriangle(lx-26, ly-18, lx, ly-18, lx-26, ly-6, CLR_BG);
    tft.fillTriangle(rx+26, ry-18, rx, ry-18, rx+26, ry-6, CLR_BG);
    tft.fillCircle(lx-4, ly+4, 10, s.iris);
    tft.fillCircle(rx+4, ry+4, 10, s.iris);
    // Tears
    tft.fillCircle(lx-4, ly+22, 6, CLR_NEON_BLUE);
    tft.fillCircle(rx+4, ry+22, 6, CLR_NEON_BLUE);

  } else if (type == "Angry") {
    drawGlow(lx, ly, 26, s.glow);
    drawGlow(rx, ry, 26, s.glow);
    tft.fillRoundRect(lx-26, ly-18, 52, 36, 10, s.sclera);
    tft.fillRoundRect(rx-26, ry-18, 52, 36, 10, s.sclera);
    tft.fillTriangle(lx, ly-18, lx+26, ly-18, lx+26, ly-6, CLR_BG);
    tft.fillTriangle(rx-26, ry-18, rx, ry-18, rx-26, ry-6, CLR_BG);
    tft.fillCircle(lx, ly+2, 12, s.iris);
    tft.fillCircle(rx, ry+2, 12, s.iris);
    tft.fillCircle(lx, ly+2, 6, s.pupil);
    tft.fillCircle(rx, ry+2, 6, s.pupil);

  } else if (type == "Surprised") {
    drawGlow(lx, ly, 30, s.glow);
    drawGlow(rx, ry, 30, s.glow);
    tft.fillCircle(lx, ly, 28, s.sclera);
    tft.fillCircle(rx, ry, 28, s.sclera);
    tft.fillCircle(lx+2, ly+2, 14, s.iris);
    tft.fillCircle(rx+2, ry+2, 14, s.iris);
    tft.fillCircle(lx+2, ly+2, 7,  s.pupil);
    tft.fillCircle(rx+2, ry+2, 7,  s.pupil);
    tft.fillCircle(lx+7, ly-4, 5,  CLR_WHITE);
    tft.fillCircle(rx+7, ry-4, 5,  CLR_WHITE);

  } else if (type == "Love") {
    drawGlow(lx, ly, 28, CLR_NEON_PINK);
    drawGlow(rx, ry, 28, CLR_NEON_PINK);
    tft.fillRoundRect(lx-28, ly-20, 56, 40, 12, s.sclera);
    tft.fillRoundRect(rx-28, ry-20, 56, 40, 12, s.sclera);
    // Hearts
    tft.fillCircle(lx-6, ly, 8,  CLR_NEON_PINK);
    tft.fillCircle(lx+6, ly, 8,  CLR_NEON_PINK);
    tft.fillTriangle(lx-14, ly+4, lx+14, ly+4, lx, ly+16, CLR_NEON_PINK);
    tft.fillCircle(rx-6, ry, 8,  CLR_NEON_PINK);
    tft.fillCircle(rx+6, ry, 8,  CLR_NEON_PINK);
    tft.fillTriangle(rx-14, ry+4, rx+14, ry+4, rx, ry+16, CLR_NEON_PINK);

  } else if (type == "Sleepy") {
    drawGlow(lx, ly, 22, s.glow);
    drawGlow(rx, ry, 22, s.glow);
    tft.fillRoundRect(lx-26, ly-18, 52, 36, 10, s.sclera);
    tft.fillRoundRect(rx-26, ry-18, 52, 36, 10, s.sclera);
    tft.fillRect(lx-28, ly-20, 58, 24, CLR_BG);
    tft.fillRect(rx-28, ry-20, 58, 24, CLR_BG);
    tft.fillCircle(lx, ly+6, 10, s.iris);
    tft.fillCircle(rx, ry+6, 10, s.iris);
    // ZZZ
    tft.setTextColor(CLR_PURPLE); tft.setTextSize(2);
    tft.setCursor(rx+30, ry-20); tft.print("z");
    tft.setTextSize(3);
    tft.setCursor(rx+40, ry-34); tft.print("Z");

  } else if (type == "Evil") {
    drawGlow(lx, ly, 26, CLR_RED);
    drawGlow(rx, ry, 26, CLR_RED);
    tft.fillRoundRect(lx-26, ly-18, 52, 36, 10, 0x2104);
    tft.fillRoundRect(rx-26, ry-18, 52, 36, 10, 0x2104);
    tft.fillTriangle(lx, ly-18, lx+26, ly-18, lx+26, ly, CLR_BG);
    tft.fillTriangle(rx-26, ry-18, rx, ry-18, rx-26, ry, CLR_BG);
    tft.fillCircle(lx, ly+4, 10, CLR_RED);
    tft.fillCircle(rx, ry+4, 10, CLR_RED);
    tft.fillCircle(lx+4, ly+2, 4, CLR_WHITE);
    tft.fillCircle(rx+4, ry+2, 4, CLR_WHITE);

  } else if (type == "Cool") {
    // Sunglasses
    tft.fillRoundRect(lx-28, ly-14, 56, 28, 8, 0x2104);
    tft.fillRoundRect(rx-28, ry-14, 56, 28, 8, 0x2104);
    tft.fillRect(lx+28, ly-4, rx-lx-28, 10, 0x2104);
    tft.drawRoundRect(lx-28, ly-14, 56, 28, 8, CLR_NEON_BLUE);
    tft.drawRoundRect(rx-28, ry-14, 56, 28, 8, CLR_NEON_BLUE);
    tft.drawFastHLine(lx-18, ly-8, 14, CLR_NEON_BLUE);
    tft.drawFastHLine(rx-18, ry-8, 14, CLR_NEON_BLUE);

  } else if (type == "Dead") {
    tft.drawLine(lx-20, ly-20, lx+20, ly+20, CLR_RED); tft.drawLine(lx+20, ly-20, lx-20, ly+20, CLR_RED);
    tft.drawLine(lx-21, ly-20, lx+21, ly+20, CLR_RED); tft.drawLine(lx+21, ly-20, lx-21, ly+20, CLR_RED);
    tft.drawLine(rx-20, ry-20, rx+20, ry+20, CLR_RED); tft.drawLine(rx+20, ry-20, rx-20, ry+20, CLR_RED);
    tft.drawLine(rx-21, ry-20, rx+21, ry+20, CLR_RED); tft.drawLine(rx+21, ry-20, rx-21, ry+20, CLR_RED);

  } else if (type == "Wink") {
    drawGlow(lx, ly, 28, s.glow);
    tft.fillRoundRect(lx-28, ly-20, 56, 40, 12, s.sclera);
    tft.fillCircle(lx+4, ly+4, 14, s.iris);
    tft.fillCircle(lx+4, ly+4, 7,  s.pupil);
    tft.fillCircle(lx+9, ly-2,  3,  CLR_WHITE);
    // Wink right eye
    tft.drawLine(rx-22, ry, rx, ry-10, s.sclera);
    tft.drawLine(rx, ry-10, rx+22, ry, s.sclera);
    tft.drawLine(rx-22, ry+2, rx, ry-8, s.sclera);
    tft.drawLine(rx, ry-8, rx+22, ry+2, s.sclera);

  } else if (type == "Cry") {
    drawGlow(lx, ly, 24, CLR_NEON_BLUE);
    drawGlow(rx, ry, 24, CLR_NEON_BLUE);
    tft.fillRoundRect(lx-26, ly-18, 52, 36, 10, s.sclera);
    tft.fillRoundRect(rx-26, ry-18, 52, 36, 10, s.sclera);
    tft.fillTriangle(lx-26, ly-18, lx, ly-18, lx-26, ly-6, CLR_BG);
    tft.fillTriangle(rx+26, ry-18, rx, ry-18, rx+26, ry-6, CLR_BG);
    tft.fillCircle(lx-4, ly+4, 10, CLR_NEON_BLUE);
    tft.fillCircle(rx+4, ry+4, 10, CLR_NEON_BLUE);
    // Streaming tears
    for (int i = 0; i < 4; i++) {
      tft.fillCircle(lx-8, ly+20+i*8, 4-i, CLR_NEON_BLUE);
      tft.fillCircle(rx+8, ry+20+i*8, 4-i, CLR_NEON_BLUE);
    }

  } else {
    // Fallback Normal
    drawGlow(lx, ly, 28, CLR_NEON_BLUE);
    drawGlow(rx, ry, 28, CLR_NEON_BLUE);
    tft.fillRoundRect(lx-28, ly-20, 56, 40, 12, CLR_WHITE);
    tft.fillRoundRect(rx-28, ry-20, 56, 40, 12, CLR_WHITE);
    tft.fillCircle(lx+4, ly+4, 14, CLR_NEON_BLUE);
    tft.fillCircle(rx+4, ry+4, 14, CLR_NEON_BLUE);
    tft.fillCircle(lx+4, ly+4, 7,  0x0010);
    tft.fillCircle(rx+4, ry+4, 7,  0x0010);
    tft.fillCircle(lx+9, ly-2,  3,  CLR_WHITE);
    tft.fillCircle(rx+9, ry-2,  3,  CLR_WHITE);
  }

  // Eye label
  tft.setTextColor(s.glow, CLR_BG);
  tft.setTextSize(1);
  int labelX = 120 - (type.length() * 3);
  tft.setCursor(labelX, 175);
  tft.print(type);
}

// ── Sensor bar (y: 190-235) ───────────────────────────────────────────────────
void drawSensorBar() {
  tft.fillRect(0, 190, 240, 46, 0x0821);
  tft.drawFastHLine(0, 190, 240, CLR_NEON_BLUE);
  tft.drawFastHLine(0, 235, 240, CLR_NEON_BLUE);

  // Temperature
  tft.setTextSize(1);
  tft.setTextColor(CLR_NEON_ORG, 0x0821);
  tft.setCursor(4, 195);
  tft.print("TEMP");
  tft.setTextColor(CLR_WHITE, 0x0821);
  tft.setCursor(4, 207);
  tft.print(String(_temperature, 1) + "C");

  // Distance
  tft.setTextColor(CLR_NEON_BLUE, 0x0821);
  tft.setCursor(60, 195);
  tft.print("DIST");
  tft.setTextColor(CLR_WHITE, 0x0821);
  tft.setCursor(60, 207);
  tft.print(String(_distance, 0) + "cm");

  // Light level
  tft.setTextColor(CLR_NEON_YEL, 0x0821);
  tft.setCursor(120, 195);
  tft.print("LIGHT");
  tft.setTextColor(CLR_WHITE, 0x0821);
  tft.setCursor(120, 207);
  tft.print(String(map(_lightLevel, 0, 4095, 0, 100)) + "%");

  // Flame
  tft.setTextColor(CLR_RED, 0x0821);
  tft.setCursor(180, 195);
  tft.print("FLAME");
  tft.setTextColor(_flameDetected ? CLR_RED : CLR_NEON_GRN, 0x0821);
  tft.setCursor(180, 207);
  tft.print(_flameDetected ? "YES!" : "NO");

  // Obstacle bar
  tft.setTextColor(CLR_NEON_PINK, 0x0821);
  tft.setCursor(4, 222);
  tft.print(_obstacleDetected ? "!! OBSTACLE DETECTED !!" : "Path clear");
}

// ── Touch buttons (y: 240-320) ────────────────────────────────────────────────
struct TouchBtn {
  int x, y, w, h;
  const char* label;
  uint16_t color;
  const char* cmd;
};

const TouchBtn BUTTONS[] = {
  { 80,  242, 80, 36, "FWD",  CLR_NEON_GRN,  "forward"  },
  {  0,  282, 80, 36, "LEFT", CLR_NEON_BLUE, "left"     },
  { 80,  282, 80, 36, "STOP", CLR_RED,       "stop"     },
  {160,  282, 80, 36, "RGHT", CLR_NEON_BLUE, "right"    },
  { 80,  282+38, 80, 36, "BCK", CLR_NEON_ORG, "backward"},
};
const int BTN_COUNT = 5;

void drawButtons() {
  tft.fillRect(0, 240, 240, 80, 0x1082);
  tft.drawFastHLine(0, 240, 240, CLR_NEON_BLUE);
  for (int i = 0; i < BTN_COUNT; i++) {
    const TouchBtn& b = BUTTONS[i];
    tft.fillRoundRect(b.x + 1, b.y + 1, b.w - 2, b.h - 2, 6, b.color);
    // Dark border
    tft.drawRoundRect(b.x, b.y, b.w, b.h, 6, CLR_WHITE);
    tft.setTextColor(CLR_BG, b.color);
    tft.setTextSize(2);
    int tx = b.x + (b.w - strlen(b.label) * 12) / 2;
    int ty = b.y + (b.h - 14) / 2;
    tft.setCursor(tx, ty);
    tft.print(b.label);
  }
}

// ── Full screen redraw ────────────────────────────────────────────────────────
void redrawAll() {
  tft.fillScreen(CLR_BG);
  drawStatusBar();
  drawEyesPanel(_currentEye, _eyeOffX, _eyeOffY);
  drawSensorBar();
  drawButtons();
}

// ── Public API ────────────────────────────────────────────────────────────────
void begin() {
  tft.init();
  tft.setRotation(0);  // portrait
  tft.fillScreen(CLR_BG);

  touch.begin();
  touch.setRotation(1);

  // Boot splash
  tft.setTextColor(CLR_NEON_BLUE);
  tft.setTextSize(3);
  tft.setCursor(10, 80);
  tft.print("NoorRobot");
  tft.setTextColor(CLR_NEON_PINK);
  tft.setTextSize(2);
  tft.setCursor(20, 130);
  tft.print("Booting...");
  tft.setTextColor(CLR_NEON_GRN);
  tft.setTextSize(1);
  tft.setCursor(20, 160);
  tft.print("Connecting to WiFi");

  delay(1500);
  redrawAll();
}

void setEyes(const String& type, int ox, int oy) {
  _currentEye = type;
  _eyeOffX = ox;
  _eyeOffY = oy;
  drawEyesPanel(type, ox, oy);
}

void clearEyes() {
  tft.fillRect(0, 45, 240, 140, CLR_BG);
}

void updateSensors(float temp, float dist, int light, bool flame, bool obstacle) {
  _temperature      = temp;
  _distance         = dist;
  _lightLevel       = light;
  _flameDetected    = flame;
  _obstacleDetected = obstacle;
  drawSensorBar();
  if (flame || obstacle) drawStatusBar(); // refresh alerts
}

// Call this every loop()
void loop() {
  unsigned long now = millis();

  // Blink every 4 seconds
  if (_currentEye == "Normal" || _currentEye == "Happy" || _currentEye == "Cool") {
    if (now - _lastBlink > 4000) {
      _lastBlink = now;
      // Quick blink: cover eyes briefly
      tft.fillRect(20, 95, 200, 20, CLR_BG);
      delay(80);
      drawEyesPanel(_currentEye, _eyeOffX, _eyeOffY);
    }
  }

  // Subtle pupil drift every 2 seconds
  if (now - _lastEyeAnim > 2000 && _currentEye == "Normal") {
    _lastEyeAnim = now;
    _pupilDx = random(-4, 5);
    _pupilDy = random(-3, 4);
    drawEyesPanel(_currentEye, _eyeOffX, _eyeOffY);
  }

  // Status bar refresh every 10 seconds
  static unsigned long lastStatus = 0;
  if (now - lastStatus > 10000) {
    lastStatus = now;
    drawStatusBar();
  }

  // Touch handling
  if (touch.tirqTouched() && touch.touched()) {
    TS_Point p = touch.getPoint();
    // Map raw touch coords to screen coords (calibrate if needed)
    int tx = map(p.x, 200, 3800, 0, 240);
    int ty = map(p.y, 200, 3800, 0, 320);

    for (int i = 0; i < BTN_COUNT; i++) {
      const TouchBtn& b = BUTTONS[i];
      if (tx >= b.x && tx <= b.x + b.w && ty >= b.y && ty <= b.y + b.h) {
        // Visual feedback — flash button
        tft.fillRoundRect(b.x + 1, b.y + 1, b.w - 2, b.h - 2, 6, CLR_WHITE);
        delay(80);
        tft.fillRoundRect(b.x + 1, b.y + 1, b.w - 2, b.h - 2, 6, b.color);
        tft.setTextColor(CLR_BG, b.color);
        tft.setTextSize(2);
        int tlx = b.x + (b.w - strlen(b.label) * 12) / 2;
        int tly = b.y + (b.h - 14) / 2;
        tft.setCursor(tlx, tly);
        tft.print(b.label);

        if (_onTouchCommand) _onTouchCommand(String(b.cmd));
        break;
      }
    }
    delay(150); // debounce
  }
}

} // namespace TftManager
