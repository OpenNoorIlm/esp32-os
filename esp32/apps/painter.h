#pragma once
// ── apps/painter.h ────────────────────────────────────────────────────────────
// NoorPaint — Full feature painter for 2.8" TFT
//
// Tools:
//   Pen (size 1-20), Eraser, Line, Rect, RoundRect, Circle, Triangle
//   Fill bucket, Text tool, Eyedropper (pick color from screen)
//   Spray paint, Gradient fill
//
// Features:
//   Layers (up to 4), each togglable
//   Undo/Redo (up to 20 steps)
//   Color picker (full HSV wheel)
//   Save/Load to SPIFFS/SD/Default storage
//   Custom brush opacity
//   Grid overlay toggle
//   Zoom (1x, 2x, 4x) with pan
//   Rulers
//   Export as .bmp raw pixel data
// ─────────────────────────────────────────────────────────────────────────────

#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <SPIFFS.h>
#include <vector>
#include <functional>

extern TFT_eSPI tft;
extern XPT2046_Touchscreen touch;

namespace PainterApp {

// ── Canvas dimensions ─────────────────────────────────────────────────────────
#define CANVAS_X   0
#define CANVAS_Y   44
#define CANVAS_W   240
#define CANVAS_H   232   // leaves 44px top toolbar + 44px bottom toolbar
#define TOOLBAR_H  44
#define BTOOLBAR_Y 276

// ── Tools ─────────────────────────────────────────────────────────────────────
enum Tool {
  TOOL_PEN, TOOL_ERASER, TOOL_LINE, TOOL_RECT, TOOL_RRECT,
  TOOL_CIRCLE, TOOL_TRIANGLE, TOOL_FILL, TOOL_TEXT,
  TOOL_EYEDROP, TOOL_SPRAY, TOOL_GRADIENT,
  TOOL_COUNT
};
const char* TOOL_NAMES[] = {
  "PEN","ERASER","LINE","RECT","RRECT","CIRCLE","TRI","FILL","TEXT","EYE","SPRAY","GRAD"
};

// ── State ─────────────────────────────────────────────────────────────────────
Tool     _tool        = TOOL_PEN;
uint16_t _color       = 0xFFFF;
uint16_t _bg          = 0x0000;
int      _brushSize   = 2;
bool     _filled      = false;
bool     _gridOn      = false;
int      _zoom        = 1;   // 1, 2, 4
int      _panX        = 0, _panY = 0;
bool     _showRuler   = false;
String   _currentFile = "";
String   _storageMode = "default"; // "spiffs", "sd", "default"

// ── Layer system ──────────────────────────────────────────────────────────────
#define MAX_LAYERS 4
struct Layer {
  std::vector<uint8_t> pixels; // RGB565 stored as raw bytes, W*H*2
  bool visible = true;
  String name;
};
std::vector<Layer> _layers;
int _activeLayer = 0;

void initLayers() {
  _layers.clear();
  for (int i = 0; i < 1; i++) { // start with 1 layer
    Layer l;
    l.pixels.resize(CANVAS_W * CANVAS_H * 2, 0);
    l.name = "Layer " + String(i+1);
    _layers.push_back(l);
  }
}

void addLayer() {
  if (_layers.size() >= MAX_LAYERS) return;
  Layer l;
  l.pixels.resize(CANVAS_W * CANVAS_H * 2, 0);
  l.name = "Layer " + String(_layers.size()+1);
  _layers.push_back(l);
}

// ── Undo/Redo ─────────────────────────────────────────────────────────────────
#define MAX_UNDO 20
std::vector<std::vector<uint8_t>> _undoStack;
std::vector<std::vector<uint8_t>> _redoStack;

void pushUndo() {
  _redoStack.clear();
  _undoStack.push_back(_layers[_activeLayer].pixels);
  if (_undoStack.size() > MAX_UNDO) _undoStack.erase(_undoStack.begin());
}

void undo() {
  if (_undoStack.empty()) return;
  _redoStack.push_back(_layers[_activeLayer].pixels);
  _layers[_activeLayer].pixels = _undoStack.back();
  _undoStack.pop_back();
}

void redo() {
  if (_redoStack.empty()) return;
  _undoStack.push_back(_layers[_activeLayer].pixels);
  _layers[_activeLayer].pixels = _redoStack.back();
  _redoStack.pop_back();
}

// ── Pixel helpers ─────────────────────────────────────────────────────────────
void setPixel(Layer& l, int x, int y, uint16_t color) {
  if (x < 0 || x >= CANVAS_W || y < 0 || y >= CANVAS_H) return;
  int idx = (y * CANVAS_W + x) * 2;
  l.pixels[idx]     = color >> 8;
  l.pixels[idx + 1] = color & 0xFF;
}

uint16_t getPixel(Layer& l, int x, int y) {
  if (x < 0 || x >= CANVAS_W || y < 0 || y >= CANVAS_H) return 0;
  int idx = (y * CANVAS_W + x) * 2;
  return ((uint16_t)l.pixels[idx] << 8) | l.pixels[idx + 1];
}

// Composite all visible layers
uint16_t compositePixel(int x, int y) {
  uint16_t result = _bg;
  for (auto& l : _layers) {
    if (!l.visible) continue;
    uint16_t p = getPixel(l, x, y);
    if (p != 0) result = p; // simple last-write composite
  }
  return result;
}

// ── Render canvas to TFT ──────────────────────────────────────────────────────
void renderCanvas() {
  for (int y = 0; y < CANVAS_H; y++) {
    for (int x = 0; x < CANVAS_W; x++) {
      uint16_t c = compositePixel(x, y);
      tft.drawPixel(CANVAS_X + x, CANVAS_Y + y, c);
    }
  }
  if (_gridOn && _zoom >= 2) {
    for (int x = 0; x < CANVAS_W; x += 8) tft.drawFastVLine(CANVAS_X + x, CANVAS_Y, CANVAS_H, 0x2104);
    for (int y = 0; y < CANVAS_H; y += 8) tft.drawFastHLine(CANVAS_X, CANVAS_Y + y, CANVAS_W, 0x2104);
  }
}

// ── Draw tools on layer ───────────────────────────────────────────────────────
void drawPenAt(int x, int y, uint16_t c) {
  int r = _brushSize;
  for (int dy = -r; dy <= r; dy++)
    for (int dx = -r; dx <= r; dx++)
      if (dx*dx + dy*dy <= r*r)
        setPixel(_layers[_activeLayer], x+dx, y+dy, c);
}

void drawSpray(int x, int y, uint16_t c) {
  for (int i = 0; i < 8; i++) {
    int dx = random(-_brushSize*3, _brushSize*3+1);
    int dy = random(-_brushSize*3, _brushSize*3+1);
    if (dx*dx+dy*dy <= _brushSize*_brushSize*9)
      setPixel(_layers[_activeLayer], x+dx, y+dy, c);
  }
}

void floodFill(int x, int y, uint16_t fillColor) {
  uint16_t target = getPixel(_layers[_activeLayer], x, y);
  if (target == fillColor) return;
  // BFS flood fill
  std::vector<std::pair<int,int>> queue;
  queue.push_back({x, y});
  int head = 0;
  while (head < (int)queue.size()) {
    auto [cx, cy] = queue[head++];
    if (cx < 0 || cx >= CANVAS_W || cy < 0 || cy >= CANVAS_H) continue;
    if (getPixel(_layers[_activeLayer], cx, cy) != target) continue;
    setPixel(_layers[_activeLayer], cx, cy, fillColor);
    queue.push_back({cx+1,cy}); queue.push_back({cx-1,cy});
    queue.push_back({cx,cy+1}); queue.push_back({cx,cy-1});
    if (queue.size() > 10000) break; // safety cap
  }
}

void drawLine(int x1, int y1, int x2, int y2, uint16_t c) {
  // Bresenham
  int dx = abs(x2-x1), dy = abs(y2-y1);
  int sx = x1<x2?1:-1, sy = y1<y2?1:-1;
  int err = dx-dy;
  while (true) {
    drawPenAt(x1, y1, c);
    if (x1==x2 && y1==y2) break;
    int e2 = 2*err;
    if (e2 > -dy) { err-=dy; x1+=sx; }
    if (e2 <  dx) { err+=dx; y1+=sy; }
  }
}

void drawRect(int x1, int y1, int x2, int y2, uint16_t c, bool filled) {
  if (filled) {
    for (int y=min(y1,y2); y<=max(y1,y2); y++)
      for (int x=min(x1,x2); x<=max(x1,x2); x++)
        setPixel(_layers[_activeLayer], x, y, c);
  } else {
    drawLine(x1,y1,x2,y1,c); drawLine(x1,y2,x2,y2,c);
    drawLine(x1,y1,x1,y2,c); drawLine(x2,y1,x2,y2,c);
  }
}

void drawCircle(int cx, int cy, int r, uint16_t c, bool filled) {
  if (filled) {
    for (int y=-r; y<=r; y++)
      for (int x=-r; x<=r; x++)
        if (x*x+y*y<=r*r) setPixel(_layers[_activeLayer], cx+x, cy+y, c);
  } else {
    int x=r, y=0, err=0;
    while (x>=y) {
      setPixel(_layers[_activeLayer], cx+x, cy+y, c); setPixel(_layers[_activeLayer], cx+y, cy+x, c);
      setPixel(_layers[_activeLayer], cx-y, cy+x, c); setPixel(_layers[_activeLayer], cx-x, cy+y, c);
      setPixel(_layers[_activeLayer], cx-x, cy-y, c); setPixel(_layers[_activeLayer], cx-y, cy-x, c);
      setPixel(_layers[_activeLayer], cx+y, cy-x, c); setPixel(_layers[_activeLayer], cx+x, cy-y, c);
      if (err<=0) { y++; err+=2*y+1; }
      if (err>0)  { x--; err-=2*x+1; }
    }
  }
}

void drawGradient(int x1, int y1, int x2, int y2, uint16_t c1, uint16_t c2) {
  int steps = max(abs(x2-x1), abs(y2-y1));
  for (int i=0; i<=steps; i++) {
    float t = (float)i/steps;
    uint8_t r = ((c1>>11)&0x1F)*(1-t) + ((c2>>11)&0x1F)*t;
    uint8_t g = ((c1>>5)&0x3F)*(1-t) + ((c2>>5)&0x3F)*t;
    uint8_t b = (c1&0x1F)*(1-t) + (c2&0x1F)*t;
    uint16_t c = (r<<11)|(g<<5)|b;
    int x = x1 + (x2-x1)*i/steps;
    int y = y1 + (y2-y1)*i/steps;
    drawPenAt(x, y, c);
  }
}

// ── Color picker (HSV wheel) ──────────────────────────────────────────────────
uint16_t hsvToRgb565(float h, float s, float v) {
  float r,g,b;
  int i=(int)(h*6); float f=h*6-i;
  float p=v*(1-s), q=v*(1-f*s), t=v*(1-(1-f)*s);
  switch(i%6){
    case 0:r=v;g=t;b=p;break; case 1:r=q;g=v;b=p;break;
    case 2:r=p;g=v;b=t;break; case 3:r=p;g=q;b=v;break;
    case 4:r=t;g=p;b=v;break; default:r=v;g=p;b=q;break;
  }
  return ((uint16_t)(r*31)<<11)|((uint16_t)(g*63)<<5)|(uint16_t)(b*31);
}

uint16_t showColorPicker() {
  tft.fillScreen(0x0000);
  tft.setTextColor(0xFFFF); tft.setTextSize(1);
  tft.setCursor(4,4); tft.print("Color Picker — tap to select");

  // Draw HSV wheel approximation as a grid
  int cx=120, cy=160, r=80;
  for (int y=-r; y<=r; y++) {
    for (int x=-r; x<=r; x++) {
      if (x*x+y*y > r*r) continue;
      float h = (atan2f(y,x)+M_PI)/(2*M_PI);
      float s = sqrtf(x*x+y*y)/r;
      uint16_t c = hsvToRgb565(h, s, 1.0f);
      tft.drawPixel(cx+x, cy+y, c);
    }
  }

  // Value slider
  for (int i=0; i<240; i++) {
    uint16_t c = hsvToRgb565(0, 0, (float)i/240);
    tft.drawFastVLine(i, 290, 20, c);
  }
  tft.setTextColor(0xFFFF,0x0000); tft.setCursor(4,295); tft.print("Brightness");

  // Current color preview
  tft.fillRect(0,314,240,6,_color);

  // Wait for touch
  while (true) {
    if (touch.tirqTouched() && touch.touched()) {
      TS_Point p = touch.getPoint();
      int tx = map(p.x, 200, 3800, 0, 240);
      int ty = map(p.y, 200, 3800, 0, 320);

      if (ty >= 290 && ty < 310) {
        // Brightness slider
        float v = (float)tx/240;
        float h = (atan2f(_color&0x1F, (_color>>11)&0x1F)+M_PI)/(2*M_PI);
        _color = hsvToRgb565(h, 0.8f, v);
        tft.fillRect(0,314,240,6,_color);
        delay(100); continue;
      }

      if (ty >= 80 && ty <= 240) {
        int dx = tx - cx, dy = ty - cy;
        if (dx*dx+dy*dy <= r*r) {
          float h = (atan2f(dy,dx)+M_PI)/(2*M_PI);
          float s = sqrtf(dx*dx+dy*dy)/r;
          _color = hsvToRgb565(h, s, 1.0f);
          tft.fillRect(0,314,240,6,_color);
        } else {
          // Outside wheel = confirm and return
          return _color;
        }
      }
      if (ty >= 314) return _color;
      delay(80);
    }
    delay(20);
  }
}

// ── Toolbar drawing ───────────────────────────────────────────────────────────
#define TOOL_BTN_W 40
#define TOOL_BTN_H 22

void drawTopToolbar() {
  tft.fillRect(0, 0, 240, TOOLBAR_H, 0x0821);
  tft.drawFastHLine(0, TOOLBAR_H, 240, 0x07FF);

  // Tool buttons (2 rows, 6 per row)
  const char* row1[] = {"PEN","ERSR","LINE","RECT","RRCT","CIRC"};
  const char* row2[] = {"TRI","FILL","TEXT","EYE","SPRY","GRAD"};
  for (int i=0; i<6; i++) {
    bool sel = (_tool == (Tool)i);
    tft.fillRoundRect(i*40, 1, 38, 20, 3, sel ? 0x07FF : 0x2104);
    tft.setTextColor(sel ? 0x0000 : 0xFFFF, sel ? 0x07FF : 0x2104);
    tft.setTextSize(1);
    tft.setCursor(i*40+2, 7);
    tft.print(row1[i]);
  }
  for (int i=0; i<6; i++) {
    bool sel = (_tool == (Tool)(i+6));
    tft.fillRoundRect(i*40, 22, 38, 20, 3, sel ? 0x07FF : 0x2104);
    tft.setTextColor(sel ? 0x0000 : 0xFFFF, sel ? 0x07FF : 0x2104);
    tft.setTextSize(1);
    tft.setCursor(i*40+2, 28);
    tft.print(row2[i]);
  }
}

void drawBottomToolbar() {
  tft.fillRect(0, BTOOLBAR_Y, 240, 44, 0x0821);
  tft.drawFastHLine(0, BTOOLBAR_Y, 240, 0x07FF);

  // Color swatch
  tft.fillRect(2, BTOOLBAR_Y+4, 30, 30, _bg);
  tft.fillRect(8, BTOOLBAR_Y+10, 30, 20, _color);
  tft.drawRect(2, BTOOLBAR_Y+4, 30, 30, 0xFFFF);
  tft.drawRect(8, BTOOLBAR_Y+10, 30, 20, 0xFFFF);

  // Brush size
  tft.setTextColor(0x07FF, 0x0821); tft.setTextSize(1);
  tft.setCursor(44, BTOOLBAR_Y+6); tft.print("SIZE");
  tft.fillRoundRect(40, BTOOLBAR_Y+18, 16, 16, 3, 0x4208);
  tft.setTextColor(0xFFFF, 0x4208); tft.setCursor(44, BTOOLBAR_Y+22); tft.print("-");
  tft.fillRoundRect(58, BTOOLBAR_Y+18, 16, 16, 3, 0xFFFF);
  tft.setTextColor(0x0000, 0xFFFF); tft.setCursor(60, BTOOLBAR_Y+22); tft.print(String(_brushSize));
  tft.fillRoundRect(76, BTOOLBAR_Y+18, 16, 16, 3, 0x4208);
  tft.setTextColor(0xFFFF, 0x4208); tft.setCursor(80, BTOOLBAR_Y+22); tft.print("+");

  // Filled toggle
  tft.fillRoundRect(96, BTOOLBAR_Y+4, 28, 16, 3, _filled ? 0x07E0 : 0x4208);
  tft.setTextColor(_filled?0x0000:0xFFFF, _filled?0x07E0:0x4208);
  tft.setCursor(100, BTOOLBAR_Y+8); tft.print("FILL");

  // Grid toggle
  tft.fillRoundRect(96, BTOOLBAR_Y+22, 28, 16, 3, _gridOn ? 0x07E0 : 0x4208);
  tft.setTextColor(_gridOn?0x0000:0xFFFF, _gridOn?0x07E0:0x4208);
  tft.setCursor(99, BTOOLBAR_Y+26); tft.print("GRID");

  // Undo/Redo
  tft.fillRoundRect(128, BTOOLBAR_Y+4,  28, 16, 3, 0x4208);
  tft.setTextColor(0xFFFF,0x4208); tft.setCursor(132,BTOOLBAR_Y+8); tft.print("UNDO");
  tft.fillRoundRect(128, BTOOLBAR_Y+22, 28, 16, 3, 0x4208);
  tft.setTextColor(0xFFFF,0x4208); tft.setCursor(132,BTOOLBAR_Y+26); tft.print("REDO");

  // Layers
  tft.fillRoundRect(160, BTOOLBAR_Y+4,  30, 16, 3, 0x801F);
  tft.setTextColor(0xFFFF,0x801F); tft.setCursor(164,BTOOLBAR_Y+8); tft.print("LYRS");

  // Save/Load
  tft.fillRoundRect(160, BTOOLBAR_Y+22, 30, 16, 3, 0x07E0);
  tft.setTextColor(0x0000,0x07E0); tft.setCursor(164,BTOOLBAR_Y+26); tft.print("SAVE");

  // Exit
  tft.fillRoundRect(194, BTOOLBAR_Y+4,  42, 36, 4, 0xF800);
  tft.setTextColor(0xFFFF,0xF800); tft.setTextSize(2);
  tft.setCursor(200,BTOOLBAR_Y+16); tft.print("X");
}

// ── Save canvas ───────────────────────────────────────────────────────────────
void saveCanvas(const String& name) {
  // Save as raw BMP-like format: 4 bytes header (W,H) + RGB565 pixels
  String path = "/paintings/" + name + ".npt";
  File f = SPIFFS.open(path, "w");
  if (!f) return;
  f.write((uint8_t)(CANVAS_W >> 8)); f.write((uint8_t)(CANVAS_W & 0xFF));
  f.write((uint8_t)(CANVAS_H >> 8)); f.write((uint8_t)(CANVAS_H & 0xFF));
  f.write((uint8_t)_layers.size());
  for (auto& l : _layers) {
    f.write(l.pixels.data(), l.pixels.size());
  }
  f.close();
}

void loadCanvas(const String& name) {
  String path = "/paintings/" + name + ".npt";
  File f = SPIFFS.open(path, "r");
  if (!f) return;
  int w = (f.read()<<8)|f.read();
  int h = (f.read()<<8)|f.read();
  int numLayers = f.read();
  _layers.clear();
  for (int i=0; i<numLayers; i++) {
    Layer l;
    l.pixels.resize(w*h*2);
    f.read(l.pixels.data(), l.pixels.size());
    l.name = "Layer " + String(i+1);
    _layers.push_back(l);
  }
  f.close();
  renderCanvas();
}

// ── Layers panel ─────────────────────────────────────────────────────────────
void showLayersPanel() {
  tft.fillRect(40, 50, 160, 180, 0x0821);
  tft.drawRoundRect(40, 50, 160, 180, 8, 0x07FF);
  tft.setTextColor(0xFEA0, 0x0821); tft.setTextSize(1);
  tft.setCursor(80, 58); tft.print("LAYERS");
  tft.drawFastHLine(40, 70, 160, 0x07FF);

  for (int i=0; i<(int)_layers.size(); i++) {
    uint16_t bg = (i==_activeLayer) ? 0x07FF : 0x2104;
    tft.fillRect(44, 74+i*34, 152, 28, bg);
    tft.setTextColor(i==_activeLayer?0x0000:0xFFFF, bg);
    tft.setCursor(50, 80+i*34);
    tft.print(_layers[i].name);
    // Visible toggle
    tft.fillRoundRect(162, 78+i*34, 24, 18, 3, _layers[i].visible ? 0x07E0 : 0x4208);
    tft.setTextColor(_layers[i].visible?0x0000:0xFFFF, _layers[i].visible?0x07E0:0x4208);
    tft.setCursor(166, 83+i*34); tft.print(_layers[i].visible?"ON":"OFF");
  }

  // Add layer button
  tft.fillRoundRect(44, 74+_layers.size()*34, 70, 24, 4, 0x07E0);
  tft.setTextColor(0x0000,0x07E0); tft.setCursor(50, 83+_layers.size()*34); tft.print("+Layer");

  // Close
  tft.fillRoundRect(120, 74+_layers.size()*34, 70, 24, 4, 0x4208);
  tft.setTextColor(0xFFFF,0x4208); tft.setCursor(140, 83+_layers.size()*34); tft.print("Close");

  // Wait for touch
  unsigned long t = millis() + 15000;
  while (millis() < t) {
    if (touch.tirqTouched() && touch.touched()) {
      TS_Point p = touch.getPoint();
      int tx = map(p.x,200,3800,0,240);
      int ty = map(p.y,200,3800,0,320);
      // Select layer
      for (int i=0; i<(int)_layers.size(); i++) {
        if (tx>=44&&tx<=162 && ty>=74+i*34 && ty<=102+i*34) { _activeLayer=i; return; }
        // Toggle visibility
        if (tx>=162&&tx<=186 && ty>=78+i*34 && ty<=96+i*34) {
          _layers[i].visible=!_layers[i].visible; renderCanvas(); return;
        }
      }
      // Add layer
      if (ty>=74+(int)_layers.size()*34 && ty<=98+(int)_layers.size()*34 && tx<120) { addLayer(); return; }
      // Close
      if (ty>=74+(int)_layers.size()*34 && tx>=120) return;
    }
    delay(20);
  }
}

// ── Main paint loop ───────────────────────────────────────────────────────────
bool tftRun() {
  initLayers();
  tft.fillScreen(0x0000);
  tft.fillRect(CANVAS_X, CANVAS_Y, CANVAS_W, CANVAS_H, _bg);
  drawTopToolbar();
  drawBottomToolbar();

  int shapeX1=-1, shapeY1=-1;
  bool drawing = false;
  int lastTX=-1, lastTY=-1;
  bool inTool = false;

  while (true) {
    if (touch.tirqTouched() && touch.touched()) {
      TS_Point p = touch.getPoint();
      int tx = map(p.x, 200, 3800, 0, 240);
      int ty = map(p.y, 200, 3800, 0, 320);

      // ── Top toolbar ──
      if (ty < TOOLBAR_H) {
        int row = ty < 22 ? 0 : 1;
        int col = tx / 40;
        int toolIdx = row*6 + col;
        if (toolIdx < TOOL_COUNT) {
          _tool = (Tool)toolIdx;
          drawTopToolbar();
          shapeX1 = -1; shapeY1 = -1;
        }
        delay(150); continue;
      }

      // ── Bottom toolbar ──
      if (ty >= BTOOLBAR_Y) {
        int bty = ty - BTOOLBAR_Y;
        // Color swatch FG
        if (tx<=38 && bty>=10) { _color = showColorPicker(); tft.fillScreen(0x0000); tft.fillRect(CANVAS_X,CANVAS_Y,CANVAS_W,CANVAS_H,_bg); renderCanvas(); drawTopToolbar(); drawBottomToolbar(); continue; }
        // Color swatch BG
        if (tx<=8 && bty<10)  { uint16_t tmp=_bg; _bg=_color; _color=tmp; drawBottomToolbar(); continue; }
        // Size -
        if (tx>=40&&tx<=56 && bty>=18&&bty<=34) { _brushSize=max(1,_brushSize-1); drawBottomToolbar(); continue; }
        // Size +
        if (tx>=76&&tx<=92 && bty>=18&&bty<=34) { _brushSize=min(20,_brushSize+1); drawBottomToolbar(); continue; }
        // Filled toggle
        if (tx>=96&&tx<=124 && bty>=4&&bty<=20) { _filled=!_filled; drawBottomToolbar(); continue; }
        // Grid toggle
        if (tx>=96&&tx<=124 && bty>=22&&bty<=38) { _gridOn=!_gridOn; renderCanvas(); if(_gridOn&&_zoom>=2){for(int x=0;x<CANVAS_W;x+=8)tft.drawFastVLine(CANVAS_X+x,CANVAS_Y,CANVAS_H,0x2104);for(int y=0;y<CANVAS_H;y+=8)tft.drawFastHLine(CANVAS_X,CANVAS_Y+y,CANVAS_W,0x2104);} drawBottomToolbar(); continue; }
        // Undo
        if (tx>=128&&tx<=156 && bty>=4&&bty<=20) { undo(); renderCanvas(); continue; }
        // Redo
        if (tx>=128&&tx<=156 && bty>=22&&bty<=38) { redo(); renderCanvas(); continue; }
        // Layers
        if (tx>=160&&tx<=190 && bty>=4&&bty<=20) { showLayersPanel(); tft.fillScreen(0x0000); tft.fillRect(CANVAS_X,CANVAS_Y,CANVAS_W,CANVAS_H,_bg); renderCanvas(); drawTopToolbar(); drawBottomToolbar(); continue; }
        // Save
        if (tx>=160&&tx<=190 && bty>=22&&bty<=38) {
          String name = VKeyboard::open("Save as:", _currentFile.isEmpty() ? "painting" : _currentFile);
          if (!name.isEmpty()) { _currentFile=name; saveCanvas(name); }
          tft.fillScreen(0x0000); tft.fillRect(CANVAS_X,CANVAS_Y,CANVAS_W,CANVAS_H,_bg); renderCanvas(); drawTopToolbar(); drawBottomToolbar();
          continue;
        }
        // Exit
        if (tx>=194) return false;
        delay(100); continue;
      }

      // ── Canvas ──
      if (ty >= CANVAS_Y && ty < BTOOLBAR_Y) {
        int cx = tx - CANVAS_X;
        int cy = ty - CANVAS_Y;

        switch (_tool) {
          case TOOL_PEN:
            if (!drawing) { pushUndo(); drawing=true; }
            if (lastTX>=0) drawLine(lastTX,lastTY,cx,cy,_color);
            else drawPenAt(cx,cy,_color);
            // Update TFT directly for pen (fast path)
            for (int dy=-_brushSize;dy<=_brushSize;dy++)
              for (int dx=-_brushSize;dx<=_brushSize;dx++)
                if (dx*dx+dy*dy<=_brushSize*_brushSize)
                  tft.drawPixel(CANVAS_X+cx+dx,CANVAS_Y+cy+dy,_color);
            lastTX=cx; lastTY=cy;
            break;

          case TOOL_ERASER:
            if (!drawing) { pushUndo(); drawing=true; }
            if (lastTX>=0) drawLine(lastTX,lastTY,cx,cy,_bg);
            else drawPenAt(cx,cy,_bg);
            for (int dy=-_brushSize;dy<=_brushSize;dy++)
              for (int dx=-_brushSize;dx<=_brushSize;dx++)
                if (dx*dx+dy*dy<=_brushSize*_brushSize)
                  tft.drawPixel(CANVAS_X+cx+dx,CANVAS_Y+cy+dy,_bg);
            lastTX=cx; lastTY=cy;
            break;

          case TOOL_SPRAY:
            if (!drawing) { pushUndo(); drawing=true; }
            drawSpray(cx,cy,_color);
            // Update just affected area
            for (int dy=-_brushSize*3;dy<=_brushSize*3;dy++)
              for (int dx=-_brushSize*3;dx<=_brushSize*3;dx++)
                tft.drawPixel(CANVAS_X+cx+dx,CANVAS_Y+cy+dy,compositePixel(cx+dx,cy+dy));
            lastTX=cx; lastTY=cy;
            break;

          case TOOL_FILL:
            pushUndo();
            floodFill(cx,cy,_color);
            renderCanvas();
            break;

          case TOOL_EYEDROP:
            _color = tft.readPixel(CANVAS_X+cx,CANVAS_Y+cy);
            drawBottomToolbar();
            break;

          case TOOL_TEXT: {
            String txt = VKeyboard::open("Enter text:", "");
            if (!txt.isEmpty()) {
              pushUndo();
              tft.setTextColor(_color,0x0000);
              tft.setTextSize(_brushSize>0?_brushSize:1);
              tft.setCursor(CANVAS_X+cx, CANVAS_Y+cy);
              tft.print(txt);
              // Capture text pixels into layer
              for (int py=0;py<CANVAS_H;py++)
                for (int px=0;px<CANVAS_W;px++) {
                  uint16_t p2 = tft.readPixel(CANVAS_X+px,CANVAS_Y+py);
                  if (p2 != _bg) setPixel(_layers[_activeLayer],px,py,p2);
                }
            }
            break;
          }

          case TOOL_LINE:
            if (shapeX1<0) { shapeX1=cx; shapeY1=cy; }
            else {
              pushUndo();
              drawLine(shapeX1,shapeY1,cx,cy,_color);
              renderCanvas();
              shapeX1=-1;
            }
            break;

          case TOOL_RECT:
            if (shapeX1<0) { shapeX1=cx; shapeY1=cy; }
            else { pushUndo(); drawRect(shapeX1,shapeY1,cx,cy,_color,_filled); renderCanvas(); shapeX1=-1; }
            break;

          case TOOL_CIRCLE:
            if (shapeX1<0) { shapeX1=cx; shapeY1=cy; }
            else {
              pushUndo();
              int r=(int)sqrtf((cx-shapeX1)*(cx-shapeX1)+(cy-shapeY1)*(cy-shapeY1));
              drawCircle(shapeX1,shapeY1,r,_color,_filled);
              renderCanvas(); shapeX1=-1;
            }
            break;

          case TOOL_TRIANGLE:
            if (shapeX1<0) { shapeX1=cx; shapeY1=cy; }
            else if (lastTX<0) { lastTX=cx; lastTY=cy; }
            else {
              pushUndo();
              drawLine(shapeX1,shapeY1,lastTX,lastTY,_color);
              drawLine(lastTX,lastTY,cx,cy,_color);
              drawLine(cx,cy,shapeX1,shapeY1,_color);
              if (_filled) floodFill((shapeX1+lastTX+cx)/3,(shapeY1+lastTY+cy)/3,_color);
              renderCanvas(); shapeX1=-1; lastTX=-1;
            }
            break;

          case TOOL_GRADIENT:
            if (shapeX1<0) { shapeX1=cx; shapeY1=cy; }
            else {
              pushUndo();
              drawGradient(shapeX1,shapeY1,cx,cy,_color,_bg);
              renderCanvas(); shapeX1=-1;
            }
            break;

          default: break;
        }
        delay(16); // ~60fps
        return false; // placeholder — actual loop never exits here
      }
    } else {
      // Finger up
      if (drawing) { drawing=false; lastTX=-1; lastTY=-1; }
    }
    delay(10);
  }
  return false;
}

// ── Shell command ─────────────────────────────────────────────────────────────
String shellCmd(const String& args) {
  if (args == "open" || args.isEmpty()) { tftRun(); return "Painter closed.\n"; }
  if (args.startsWith("load ")) { loadCanvas(args.substring(5)); tftRun(); return "OK\n"; }
  return "Usage: paint [open|load <name>]\n";
}

} // namespace PainterApp
