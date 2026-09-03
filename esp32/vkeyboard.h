#pragma once
// ── vkeyboard.h ───────────────────────────────────────────────────────────────
// Virtual QWERTY keyboard for NoorRobot TFT
// Uses TFT_eSPI + XPT2046_Touchscreen (already init'd in tft_manager.h)
// Call VKeyboard::open() to show, VKeyboard::getInput() to retrieve result.
// ─────────────────────────────────────────────────────────────────────────────

#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include "apps/hwtest.h"   // HwTest::applyMap() — runtime touch calibration

extern TFT_eSPI tft;
extern XPT2046_Touchscreen touch;

namespace VKeyboard {

// ── Layout ────────────────────────────────────────────────────────────────────
// Keyboard occupies bottom 200px of screen (y: 120-320), input bar at top
// Key size: 22x28px, gap: 2px

static const char* ROWS[] = {
  "1234567890",
  "qwertyuiop",
  "asdfghjkl",
  "zxcvbnm",
};
static const int ROW_COUNT = 4;

#define KB_Y       125
#define KB_KEY_W   21
#define KB_KEY_H   26
#define KB_GAP      2
#define KB_INPUT_H  30
#define KB_INPUT_Y  93

#define KB_CLR_BG      0x1082
#define KB_CLR_KEY     0x4208
#define KB_CLR_KEY_HL  0xFFFF
#define KB_CLR_TEXT    0xFFFF
#define KB_CLR_TEXT_HL 0x0000
#define KB_CLR_INPUT   0x0000
#define KB_CLR_BORDER  0x07FF  // cyan

String _inputBuffer = "";
bool   _active      = false;
bool   _shift       = false;
bool   _submitted   = false;

// ── Draw input bar ────────────────────────────────────────────────────────────
inline void drawInputBar() {
  tft.fillRect(0, KB_INPUT_Y, 240, KB_INPUT_H, KB_CLR_INPUT);
  tft.drawRect(0, KB_INPUT_Y, 240, KB_INPUT_H, KB_CLR_BORDER);
  tft.setTextColor(0x07FF, KB_CLR_INPUT);
  tft.setTextSize(1);
  tft.setCursor(4, KB_INPUT_Y + 4);
  tft.print(">");
  tft.setTextColor(0xFFFF, KB_CLR_INPUT);
  tft.setCursor(14, KB_INPUT_Y + 4);
  // Show last N chars that fit
  String display = _inputBuffer;
  if (display.length() > 26) display = display.substring(display.length() - 26);
  tft.print(display);
  // Blinking cursor
  int cx = 14 + display.length() * 6;
  tft.fillRect(cx, KB_INPUT_Y + 3, 2, 18, 0x07FF);
}

// ── Draw single key ───────────────────────────────────────────────────────────
inline void drawKey(int x, int y, const char* label, uint16_t bg, uint16_t fg) {
  tft.fillRoundRect(x, y, KB_KEY_W, KB_KEY_H, 3, bg);
  tft.drawRoundRect(x, y, KB_KEY_W, KB_KEY_H, 3, 0x4208);
  tft.setTextColor(fg, bg);
  tft.setTextSize(1);
  int lx = x + (KB_KEY_W - strlen(label) * 6) / 2;
  int ly = y + (KB_KEY_H - 8) / 2;
  tft.setCursor(lx, ly);
  tft.print(label);
}

// ── Draw full keyboard ────────────────────────────────────────────────────────
inline void drawKeyboard() {
  tft.fillRect(0, KB_Y - 4, 240, 320 - KB_Y + 4, KB_CLR_BG);
  tft.drawFastHLine(0, KB_Y - 4, 240, KB_CLR_BORDER);

  for (int r = 0; r < ROW_COUNT; r++) {
    const char* row = ROWS[r];
    int len = strlen(row);
    int totalW = len * (KB_KEY_W + KB_GAP) - KB_GAP;
    int startX = (240 - totalW) / 2;
    int y = KB_Y + r * (KB_KEY_H + KB_GAP);

    for (int k = 0; k < len; k++) {
      char c = _shift ? toupper(row[k]) : row[k];
      char label[2] = {c, 0};
      drawKey(startX + k * (KB_KEY_W + KB_GAP), y, label, KB_CLR_KEY, KB_CLR_TEXT);
    }
  }

  // Special keys row: SHIFT, SPACE, BKSP, ENTER
  int specialY = KB_Y + ROW_COUNT * (KB_KEY_H + KB_GAP);

  // SHIFT
  tft.fillRoundRect(2, specialY, 36, KB_KEY_H, 3, _shift ? 0x07FF : KB_CLR_KEY);
  tft.setTextColor(_shift ? 0x0000 : KB_CLR_TEXT, _shift ? 0x07FF : KB_CLR_KEY);
  tft.setTextSize(1);
  tft.setCursor(6, specialY + (KB_KEY_H - 8) / 2);
  tft.print("SHF");

  // SPACE
  tft.fillRoundRect(42, specialY, 100, KB_KEY_H, 3, KB_CLR_KEY);
  tft.setTextColor(KB_CLR_TEXT, KB_CLR_KEY);
  tft.setCursor(74, specialY + (KB_KEY_H - 8) / 2);
  tft.print("SPC");

  // BKSP
  tft.fillRoundRect(146, specialY, 42, KB_KEY_H, 3, 0xF800);
  tft.setTextColor(KB_CLR_TEXT, 0xF800);
  tft.setCursor(150, specialY + (KB_KEY_H - 8) / 2);
  tft.print("BSP");

  // ENTER
  tft.fillRoundRect(192, specialY, 46, KB_KEY_H, 3, 0x07E0);
  tft.setTextColor(0x0000, 0x07E0);
  tft.setCursor(196, specialY + (KB_KEY_H - 8) / 2);
  tft.print("ENT");

  drawInputBar();
}

// ── Hit test ──────────────────────────────────────────────────────────────────
inline void handleTouch(int tx, int ty) {
  // Check special keys row first
  int specialY = KB_Y + ROW_COUNT * (KB_KEY_H + KB_GAP);

  if (ty >= specialY && ty <= specialY + KB_KEY_H) {
    if (tx >= 2 && tx <= 38) {
      // SHIFT
      _shift = !_shift;
      drawKeyboard();
    } else if (tx >= 42 && tx <= 142) {
      // SPACE
      _inputBuffer += " ";
      drawInputBar();
    } else if (tx >= 146 && tx <= 188) {
      // BACKSPACE
      if (_inputBuffer.length() > 0)
        _inputBuffer.remove(_inputBuffer.length() - 1);
      drawInputBar();
    } else if (tx >= 192 && tx <= 238) {
      // ENTER — submit
      _submitted = true;
      _active = false;
    }
    return;
  }

  // Check letter rows
  for (int r = 0; r < ROW_COUNT; r++) {
    const char* row = ROWS[r];
    int len = strlen(row);
    int totalW = len * (KB_KEY_W + KB_GAP) - KB_GAP;
    int startX = (240 - totalW) / 2;
    int y = KB_Y + r * (KB_KEY_H + KB_GAP);

    if (ty >= y && ty <= y + KB_KEY_H) {
      for (int k = 0; k < len; k++) {
        int kx = startX + k * (KB_KEY_W + KB_GAP);
        if (tx >= kx && tx <= kx + KB_KEY_W) {
          char c = _shift ? toupper(row[k]) : row[k];
          // Flash key
          char label[2] = {c, 0};
          drawKey(kx, y, label, KB_CLR_KEY_HL, KB_CLR_TEXT_HL);
          delay(60);
          drawKey(kx, y, label, KB_CLR_KEY, KB_CLR_TEXT);
          _inputBuffer += c;
          if (_shift) { _shift = false; drawKeyboard(); }
          else drawInputBar();
          return;
        }
      }
    }
  }
}

// ── Public API ────────────────────────────────────────────────────────────────

// Open keyboard with optional prefilled text, blocks until Enter pressed
// Returns the typed string
inline String open(const String& prompt = "", const String& prefill = "") {
  _inputBuffer = prefill;
  _active      = true;
  _submitted   = false;
  _shift       = false;

  // Draw prompt above input bar
  tft.fillRect(0, 75, 240, 18, 0x0000);
  tft.setTextColor(0xFEA0, 0x0000);
  tft.setTextSize(1);
  tft.setCursor(4, 78);
  tft.print(prompt);

  drawKeyboard();

  while (_active) {
    if (touch.tirqTouched() && touch.touched()) {
      TS_Point p = touch.getPoint();
      int tx, ty;
      HwTest::applyMap(p.x, p.y, tx, ty);  // uses saved calibration
      handleTouch(tx, ty);
      delay(120); // debounce
    }
    delay(10);
  }

  return _inputBuffer;
}

// Returns current buffer without blocking
inline String getBuffer() { return _inputBuffer; }

} // namespace VKeyboard
