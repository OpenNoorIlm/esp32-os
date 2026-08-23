#pragma once
// ── apps/taskmanager.h ────────────────────────────────────────────────────────
// NoorTaskManager — TFT Task Manager app
//
// Tabs:
//   PROCS  — running tasks (FreeRTOS), CPU%, stack, kill button
//   SYSINFO— chip info, RAM, flash, WiFi, uptime, temp
//   DRIVERS— loaded drivers, enable/disable/reload
//   LOGS   — crash.log, driver.log, live scroll
//   PERF   — live CPU/RAM graph (last 60 readings)
// ─────────────────────────────────────────────────────────────────────────────

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include "../task_manager.h"
#include "../driver_manager.h"
#include "../sensor_manager.h"

extern TFT_eSPI tft;
extern XPT2046_Touchscreen touch;

namespace TaskManagerApp {

#define TM_TAB_COUNT 5
const char* TAB_NAMES[] = {"PROCS","SYSINFO","DRVRS","LOGS","PERF"};
int _activeTab = 0;
int _scrollIdx = 0;
int _selectedProc = -1;

// ── RAM/CPU history for perf graph ────────────────────────────────────────────
#define PERF_HISTORY 60
uint32_t _freeHeapHistory[PERF_HISTORY] = {0};
int      _heapHead = 0;
unsigned long _lastPerfSample = 0;

void samplePerf() {
  unsigned long now = millis();
  if (now - _lastPerfSample < 1000) return;
  _lastPerfSample = now;
  _freeHeapHistory[_heapHead] = ESP.getFreeHeap();
  _heapHead = (_heapHead + 1) % PERF_HISTORY;
}

// ── Tab bar ───────────────────────────────────────────────────────────────────
void drawTabBar() {
  int tabW = 240 / TM_TAB_COUNT;
  for (int i = 0; i < TM_TAB_COUNT; i++) {
    bool sel = (i == _activeTab);
    uint16_t bg = sel ? 0x07FF : 0x2104;
    uint16_t fg = sel ? 0x0000 : 0xFFFF;
    tft.fillRect(i * tabW, 0, tabW, 22, bg);
    tft.drawRect(i * tabW, 0, tabW, 22, 0x4208);
    tft.setTextColor(fg, bg);
    tft.setTextSize(1);
    int tx = i * tabW + (tabW - strlen(TAB_NAMES[i]) * 6) / 2;
    tft.setCursor(tx, 7);
    tft.print(TAB_NAMES[i]);
  }
  tft.drawFastHLine(0, 22, 240, 0x07FF);
}

// ── Header bar ────────────────────────────────────────────────────────────────
void drawHeader(const String& title) {
  tft.fillRect(0, 23, 240, 16, 0x0821);
  tft.setTextColor(0xFEA0, 0x0821);
  tft.setTextSize(1);
  tft.setCursor(4, 28);
  tft.print(title);
  // Uptime
  unsigned long s = millis() / 1000;
  String up = String(s/3600) + "h" + String((s%3600)/60) + "m" + String(s%60) + "s";
  tft.setTextColor(0x4208, 0x0821);
  tft.setCursor(240 - up.length()*6 - 4, 28);
  tft.print(up);
  tft.drawFastHLine(0, 39, 240, 0x2104);
}

// ── PROCS tab ─────────────────────────────────────────────────────────────────
void drawProcsTab() {
  tft.fillRect(0, 40, 240, 240, 0x0000);
  drawHeader("Processes");

  // Column headers
  tft.setTextColor(0x07FF, 0x0000);
  tft.setTextSize(1);
  tft.setCursor(4, 42);  tft.print("NAME");
  tft.setCursor(100, 42); tft.print("STACK");
  tft.setCursor(150, 42); tft.print("PRIO");
  tft.setCursor(190, 42); tft.print("STATE");
  tft.drawFastHLine(0, 52, 240, 0x2104);

  // FreeRTOS task list
  UBaseType_t taskCount = uxTaskGetNumberOfTasks();
  TaskStatus_t* taskArray = (TaskStatus_t*)pvPortMalloc(taskCount * sizeof(TaskStatus_t));
  uint32_t totalRuntime = 0;

  if (taskArray) {
    taskCount = uxTaskGetSystemState(taskArray, taskCount, &totalRuntime);
    int y = 55;
    for (int i = _scrollIdx; i < (int)taskCount && y < 270; i++) {
      bool sel = (i == _selectedProc);
      uint16_t bg = sel ? 0x0C21 : (i % 2 == 0 ? 0x0000 : 0x0821);
      tft.fillRect(0, y, 240, 18, bg);

      // Name (truncate to 12 chars)
      String name = String(taskArray[i].pcTaskName);
      if (name.length() > 12) name = name.substring(0, 11) + "~";
      tft.setTextColor(sel ? 0x07FF : 0xFFFF, bg);
      tft.setTextSize(1);
      tft.setCursor(4, y + 5);
      tft.print(name);

      // Stack high water mark
      tft.setTextColor(taskArray[i].usStackHighWaterMark < 100 ? 0xF800 : 0x07E0, bg);
      tft.setCursor(100, y + 5);
      tft.print(String(taskArray[i].usStackHighWaterMark));

      // Priority
      tft.setTextColor(0xFFE0, bg);
      tft.setCursor(150, y + 5);
      tft.print(String(taskArray[i].uxCurrentPriority));

      // State
      const char* states[] = {"RUN","READY","BLOCK","SUSP","DEL","INV"};
      uint16_t stateColor[] = {0x07E0, 0x07FF, 0xFFE0, 0x4208, 0xF800, 0xF800};
      int state = min((int)taskArray[i].eCurrentState, 5);
      tft.setTextColor(stateColor[state], bg);
      tft.setCursor(190, y + 5);
      tft.print(states[state]);

      y += 18;
    }
    vPortFree(taskArray);
  }

  // Bottom bar
  tft.fillRect(0, 272, 240, 48, 0x0821);
  tft.drawFastHLine(0, 272, 240, 0x07FF);

  // RAM bar
  uint32_t total = ESP.getHeapSize();
  uint32_t free2 = ESP.getFreeHeap();
  uint32_t used  = total - free2;
  int barW = (int)((float)used / total * 180);
  tft.fillRoundRect(4, 276, 180, 12, 3, 0x2104);
  tft.fillRoundRect(4, 276, barW, 12, 3, barW > 140 ? 0xF800 : 0x07E0);
  tft.setTextColor(0xFFFF, 0x0821); tft.setTextSize(1);
  tft.setCursor(4, 292);
  tft.print("RAM: " + String(used/1024) + "K/" + String(total/1024) + "K");

  // Kill button
  if (_selectedProc >= 0) {
    tft.fillRoundRect(186, 274, 50, 42, 6, 0xF800);
    tft.setTextColor(0xFFFF, 0xF800);
    tft.setTextSize(1);
    tft.setCursor(190, 284);
    tft.print("KILL");
    tft.setCursor(190, 298);
    tft.print("PROC");
  }

  // Task count
  tft.setTextColor(0x4208, 0x0821);
  tft.setCursor(4, 308);
  tft.print("Tasks: " + String(uxTaskGetNumberOfTasks()));
}

// ── SYSINFO tab ───────────────────────────────────────────────────────────────
void drawSysinfoTab() {
  tft.fillRect(0, 40, 240, 280, 0x0000);
  drawHeader("System Information");

  auto row = [](int y, const char* label, const String& val, uint16_t vc=0xFFFF) {
    tft.setTextColor(0x07FF, 0x0000); tft.setTextSize(1);
    tft.setCursor(4, y); tft.print(label);
    tft.setTextColor(vc, 0x0000);
    tft.setCursor(100, y); tft.print(val);
    tft.drawFastHLine(0, y+10, 240, 0x0821);
  };

  int y = 44;
  row(y, "Chip Model:",   String(ESP.getChipModel())); y+=14;
  row(y, "CPU Freq:",     String(ESP.getCpuFreqMHz()) + " MHz"); y+=14;
  row(y, "Cores:",        String(ESP.getChipCores())); y+=14;
  row(y, "SDK Version:",  String(ESP.getSdkVersion())); y+=14;
  row(y, "Flash Size:",   String(ESP.getFlashChipSize()/1024) + " KB"); y+=14;
  row(y, "Flash Speed:",  String(ESP.getFlashChipSpeed()/1000000) + " MHz"); y+=14;
  row(y, "Heap Total:",   String(ESP.getHeapSize()/1024) + " KB"); y+=14;
  row(y, "Heap Free:",    String(ESP.getFreeHeap()/1024) + " KB",
      ESP.getFreeHeap() < 20000 ? 0xF800 : 0x07E0); y+=14;
  row(y, "Min Heap:",     String(ESP.getMinFreeHeap()/1024) + " KB"); y+=14;
  row(y, "PSRAM:",        ESP.getPsramSize()>0 ? String(ESP.getPsramSize()/1024)+"KB" : "None"); y+=14;
  row(y, "SPIFFS Total:", String(SPIFFS.totalBytes()/1024) + " KB"); y+=14;
  row(y, "SPIFFS Used:",  String(SPIFFS.usedBytes()/1024) + " KB"); y+=14;

  // WiFi
  tft.setTextColor(0xFEA0, 0x0000); tft.setCursor(4, y); tft.print("── WiFi ──"); y+=12;
  row(y, "SSID:",     WiFi.SSID()); y+=14;
  row(y, "IP:",       WiFi.localIP().toString()); y+=14;
  row(y, "RSSI:",     String(WiFi.RSSI()) + " dBm",
      WiFi.RSSI() > -60 ? 0x07E0 : WiFi.RSSI() > -75 ? 0xFFE0 : 0xF800); y+=14;
  row(y, "MAC:",      WiFi.macAddress()); y+=14;

  // Sensor summary
  tft.setTextColor(0xFEA0, 0x0000); tft.setCursor(4, y); tft.print("── Sensors ──"); y+=12;
  row(y, "Temp:", String(SensorManager::tempC(),1) + "C", 0x07FF); y+=14;
  row(y, "Distance:", String(SensorManager::distance(),1) + "cm"); y+=14;
  row(y, "Flame:", SensorManager::flame() ? "DETECTED!" : "Clear",
      SensorManager::flame() ? 0xF800 : 0x07E0);
}

// ── DRIVERS tab ───────────────────────────────────────────────────────────────
void drawDriversTab() {
  tft.fillRect(0, 40, 240, 280, 0x0000);
  drawHeader("Driver Manager");

  auto list = DriverManager::loadManifest();
  // Add editable.dvr always
  bool hasEditable = false;
  for (auto& n : list) if (n == "editable") hasEditable = true;
  if (!hasEditable) list.insert(list.begin(), "editable");

  int y = 44;
  tft.setTextColor(0x07FF, 0x0000); tft.setTextSize(1);
  tft.setCursor(4, y); tft.print("NAME");
  tft.setCursor(120, y); tft.print("VER");
  tft.setCursor(160, y); tft.print("ACTIONS");
  tft.drawFastHLine(0, y+10, 240, 0x2104);
  y += 14;

  for (int i = _scrollIdx; i < (int)list.size() && y < 270; i++) {
    String src = DriverManager::readFile(DriverManager::dvrPath(list[i]));
    DriverManager::DvrMeta m = DriverManager::parseMeta(src);
    bool sel = (i == _selectedProc);
    uint16_t bg = sel ? 0x0C21 : (i%2==0 ? 0x0000 : 0x0821);
    tft.fillRect(0, y, 240, 22, bg);

    tft.setTextColor(sel ? 0x07FF : 0xFFFF, bg);
    tft.setCursor(4, y+7);
    String nm = list[i].length() > 12 ? list[i].substring(0,11)+"~" : list[i];
    tft.print(nm);

    tft.setTextColor(0x4208, bg);
    tft.setCursor(120, y+7);
    tft.print(m.version.isEmpty() ? "?" : m.version);

    // Edit button
    tft.fillRoundRect(160, y+2, 32, 18, 3, 0x07FF);
    tft.setTextColor(0x0000, 0x07FF);
    tft.setCursor(164, y+7); tft.print("EDIT");

    // Reload button
    tft.fillRoundRect(196, y+2, 38, 18, 3, 0x07E0);
    tft.setTextColor(0x0000, 0x07E0);
    tft.setCursor(200, y+7); tft.print("RELD");

    y += 22;
  }

  // Bottom: install new
  tft.fillRect(0, 272, 240, 48, 0x0821);
  tft.drawFastHLine(0, 272, 240, 0x07FF);
  tft.fillRoundRect(4, 278, 110, 34, 6, 0x07E0);
  tft.setTextColor(0x0000, 0x07E0); tft.setTextSize(1);
  tft.setCursor(14, 291); tft.print("INSTALL .DVR");

  tft.fillRoundRect(120, 278, 110, 34, 6, 0x4208);
  tft.setTextColor(0xFFFF, 0x4208);
  tft.setCursor(130, 291); tft.print("EDIT editable");
}

// ── LOGS tab ──────────────────────────────────────────────────────────────────
void drawLogsTab() {
  tft.fillRect(0, 40, 240, 280, 0x0000);
  drawHeader("System Logs");

  // Log file selector
  tft.fillRoundRect(2,  42, 76, 18, 3, 0x07FF);
  tft.fillRoundRect(82, 42, 76, 18, 3, 0x4208);
  tft.fillRoundRect(162,42, 74, 18, 3, 0x4208);
  tft.setTextColor(0x0000,0x07FF); tft.setTextSize(1);
  tft.setCursor(10, 48); tft.print("CRASH.LOG");
  tft.setTextColor(0xFFFF,0x4208);
  tft.setCursor(88, 48); tft.print("DRIVER.LOG");
  tft.setCursor(166,48); tft.print("SHELL.LOG");
  tft.drawFastHLine(0, 62, 240, 0x2104);

  // Log content
  String logContent = "";
  File f = SPIFFS.open("/logs/crash.log", "r");
  if (f) { logContent = f.readString(); f.close(); }
  if (logContent.isEmpty()) logContent = "No entries in crash.log";

  // Render last N lines
  std::vector<String> lines;
  String cur;
  for (char c : logContent) {
    if (c=='\n') { lines.push_back(cur); cur=""; }
    else cur+=c;
  }
  if (!cur.isEmpty()) lines.push_back(cur);

  int startLine = max(0, (int)lines.size() - 16 - _scrollIdx);
  int y = 66;
  tft.setTextColor(0x07E0, 0x0000); tft.setTextSize(1);
  for (int i = startLine; i < (int)lines.size() && y < 270; i++) {
    String line = lines[i];
    if (line.length() > 37) line = line.substring(0, 36) + "~";
    uint16_t c = 0x07E0;
    if (line.indexOf("error")>=0||line.indexOf("ERROR")>=0||line.indexOf("crash")>=0) c=0xF800;
    if (line.indexOf("warn")>=0||line.indexOf("WARN")>=0) c=0xFFE0;
    tft.setTextColor(c, 0x0000);
    tft.setCursor(2, y); tft.print(line);
    y += 13;
  }

  // Clear log button
  tft.fillRect(0, 272, 240, 48, 0x0821);
  tft.drawFastHLine(0, 272, 240, 0x07FF);
  tft.fillRoundRect(4, 278, 100, 34, 6, 0xF800);
  tft.setTextColor(0xFFFF,0xF800); tft.setCursor(14, 291); tft.print("CLEAR LOG");
  tft.fillRoundRect(120, 278, 116, 34, 6, 0x4208);
  tft.setTextColor(0xFFFF,0x4208); tft.setCursor(130,291); tft.print("COPY TO CLIP");
}

// ── PERF tab (live graph) ─────────────────────────────────────────────────────
void drawPerfTab() {
  tft.fillRect(0, 40, 240, 280, 0x0000);
  drawHeader("Performance");

  uint32_t total = ESP.getHeapSize();
  uint32_t free2 = ESP.getFreeHeap();
  uint32_t used  = total - free2;

  // Current stats
  tft.setTextColor(0x07FF, 0x0000); tft.setTextSize(1);
  tft.setCursor(4, 44);
  tft.print("RAM Used: " + String(used/1024) + "KB / " + String(total/1024) + "KB");
  tft.setCursor(4, 56);
  tft.print("Free: " + String(free2/1024) + "KB  Min ever: " + String(ESP.getMinFreeHeap()/1024) + "KB");
  tft.setCursor(4, 68);
  tft.print("CPU: " + String(ESP.getCpuFreqMHz()) + "MHz  Temp: " + String(SensorManager::tempC(),1) + "C");

  // Graph area
  int gx=4, gy=84, gw=232, gh=140;
  tft.drawRect(gx, gy, gw, gh, 0x2104);
  // Grid lines
  for (int i=1; i<4; i++) {
    tft.drawFastHLine(gx, gy+gh*i/4, gw, 0x0821);
    tft.setTextColor(0x4208,0x0000); tft.setTextSize(1);
    uint32_t val = total*(4-i)/4;
    tft.setCursor(gx+2, gy+gh*i/4+2);
    tft.print(String(val/1024)+"K");
  }

  // Plot heap history
  for (int i=0; i<PERF_HISTORY-1; i++) {
    int idx1 = (_heapHead + i) % PERF_HISTORY;
    int idx2 = (_heapHead + i + 1) % PERF_HISTORY;
    if (_freeHeapHistory[idx1]==0 || _freeHeapHistory[idx2]==0) continue;
    int x1 = gx + i * gw / PERF_HISTORY;
    int x2 = gx + (i+1) * gw / PERF_HISTORY;
    int y1 = gy + gh - (int)((float)_freeHeapHistory[idx1]/total*gh);
    int y2 = gy + gh - (int)((float)_freeHeapHistory[idx2]/total*gh);
    uint16_t c = _freeHeapHistory[idx2] < 20000 ? 0xF800 : _freeHeapHistory[idx2] < 50000 ? 0xFFE0 : 0x07E0;
    tft.drawLine(x1, y1, x2, y2, c);
  }

  // Labels
  tft.setTextColor(0x4208,0x0000); tft.setTextSize(1);
  tft.setCursor(gx, gy+gh+4); tft.print("60s ago");
  tft.setCursor(gx+gw-30, gy+gh+4); tft.print("now");

  // Task count graph
  tft.setCursor(4, gy+gh+18);
  tft.setTextColor(0x07FF,0x0000);
  tft.print("Tasks: " + String(uxTaskGetNumberOfTasks()));
  tft.setCursor(80, gy+gh+18);
  tft.setTextColor(0xFEA0,0x0000);
  tft.print("SPIFFS: " + String(SPIFFS.usedBytes()/1024) + "/" + String(SPIFFS.totalBytes()/1024) + "K");

  // Refresh button
  tft.fillRect(0, 272, 240, 48, 0x0821);
  tft.drawFastHLine(0, 272, 240, 0x07FF);
  tft.fillRoundRect(4, 278, 100, 34, 6, 0x07FF);
  tft.setTextColor(0x0000,0x07FF); tft.setCursor(14,291); tft.print("REFRESH");
  tft.fillRoundRect(120,278, 116, 34, 6, 0xF800);
  tft.setTextColor(0xFFFF,0xF800); tft.setCursor(130,291); tft.print("REBOOT ESP32");
}

// ── Draw active tab ───────────────────────────────────────────────────────────
void drawContent() {
  switch (_activeTab) {
    case 0: drawProcsTab();   break;
    case 1: drawSysinfoTab(); break;
    case 2: drawDriversTab(); break;
    case 3: drawLogsTab();    break;
    case 4: drawPerfTab();    break;
  }
}

// ── Handle touches ────────────────────────────────────────────────────────────
bool handleTouch(int tx, int ty) {
  // Tab bar
  if (ty < 22) {
    int tabW = 240 / TM_TAB_COUNT;
    int tab = tx / tabW;
    if (tab != _activeTab) {
      _activeTab = tab;
      _scrollIdx = 0;
      _selectedProc = -1;
      drawTabBar();
      drawContent();
    }
    return true;
  }

  // Proc tab: select process
  if (_activeTab == 0 && ty >= 55 && ty < 272) {
    int idx = _scrollIdx + (ty - 55) / 18;
    _selectedProc = (_selectedProc == idx) ? -1 : idx;
    // Kill button
    if (tx >= 186 && ty >= 274 && _selectedProc >= 0) {
      UBaseType_t taskCount = uxTaskGetNumberOfTasks();
      TaskStatus_t* arr = (TaskStatus_t*)pvPortMalloc(taskCount * sizeof(TaskStatus_t));
      uint32_t rt = 0;
      if (arr) {
        taskCount = uxTaskGetSystemState(arr, taskCount, &rt);
        if (_selectedProc < (int)taskCount) {
          vTaskDelete(arr[_selectedProc].xHandle);
        }
        vPortFree(arr);
      }
      _selectedProc = -1;
    }
    drawContent();
    return true;
  }

  // Driver tab: edit/reload buttons
  if (_activeTab == 2 && ty >= 44 && ty < 272) {
    auto list = DriverManager::loadManifest();
    bool hasEditable = false;
    for (auto& n : list) if (n == "editable") hasEditable = true;
    if (!hasEditable) list.insert(list.begin(), "editable");

    int rowIdx = _scrollIdx + (ty - 66) / 22;
    if (rowIdx >= 0 && rowIdx < (int)list.size()) {
      if (tx >= 160 && tx <= 192) {
        // EDIT
        NanoEditor::tftEdit(DriverManager::dvrPath(list[rowIdx]));
        tft.fillScreen(0x0000);
        drawTabBar(); drawContent();
      } else if (tx >= 196) {
        // RELOAD
        String src = DriverManager::readFile(DriverManager::dvrPath(list[rowIdx]));
        String out; LuaEngine::eval(src, out);
        drawContent();
      }
    }
    // Install button
    if (ty >= 278 && ty <= 312 && tx < 114) {
      String path = VKeyboard::open("DVR path:", "/");
      if (!path.isEmpty()) {
        DriverManager::install(path);
        tft.fillScreen(0x0000);
        drawTabBar(); drawContent();
      }
    }
    // Edit editable.dvr shortcut
    if (ty >= 278 && ty <= 312 && tx >= 120) {
      NanoEditor::tftEdit("/drivers/editable.dvr");
      tft.fillScreen(0x0000);
      drawTabBar(); drawContent();
    }
    return true;
  }

  // Logs tab: clear
  if (_activeTab == 3 && ty >= 278 && ty <= 312 && tx < 114) {
    SPIFFS.remove("/logs/crash.log");
    drawContent();
    return true;
  }

  // Perf tab: refresh / reboot
  if (_activeTab == 4 && ty >= 278 && ty <= 312) {
    if (tx < 114) { drawContent(); }
    else { delay(200); ESP.restart(); }
    return true;
  }

  // Scroll
  return false;
}

// ── Main TFT app loop ─────────────────────────────────────────────────────────
bool tftRun() {
  tft.fillScreen(0x0000);
  drawTabBar();
  drawContent();

  int swipeStartY = -1;
  unsigned long lastRefresh = millis();

  while (true) {
    samplePerf();

    // Auto-refresh perf tab every second
    if (_activeTab == 4 && millis() - lastRefresh > 2000) {
      lastRefresh = millis();
      drawContent();
    }
    // Auto-refresh procs every 3s
    if (_activeTab == 0 && millis() - lastRefresh > 3000) {
      lastRefresh = millis();
      drawContent();
    }

    if (touch.tirqTouched() && touch.touched()) {
      TS_Point p = touch.getPoint();
      int tx = map(p.x, 200, 3800, 0, 240);
      int ty = map(p.y, 200, 3800, 0, 320);
      swipeStartY = ty;
      delay(80);

      // Check swipe
      if (touch.touched()) {
        TS_Point p2 = touch.getPoint();
        int ty2 = map(p2.y, 200, 3800, 0, 320);
        int dy = swipeStartY - ty2;
        if (abs(dy) > 30) {
          _scrollIdx = max(0, _scrollIdx + (dy > 0 ? 1 : -1));
          drawContent();
          continue;
        }
      }

      if (!handleTouch(tx, ty)) {
        // Exit swipe up from very bottom
        if (ty > 300) return false;
      }
    }
    delay(20);
  }
}

// ── Shell command ─────────────────────────────────────────────────────────────
String shellCmd(const String& args) {
  if (args == "open" || args.isEmpty()) { tftRun(); return "Task Manager closed.\n"; }

  if (args == "list" || args == "ps") {
    UBaseType_t taskCount = uxTaskGetNumberOfTasks();
    TaskStatus_t* arr = (TaskStatus_t*)pvPortMalloc(taskCount * sizeof(TaskStatus_t));
    uint32_t rt = 0;
    String out = "PID  NAME             STACK  PRIO  STATE\n";
    out += "─────────────────────────────────────────\n";
    if (arr) {
      taskCount = uxTaskGetSystemState(arr, taskCount, &rt);
      for (int i = 0; i < (int)taskCount; i++) {
        String name = String(arr[i].pcTaskName);
        while (name.length() < 16) name += " ";
        const char* states[] = {"Running","Ready","Blocked","Suspended","Deleted","Invalid"};
        out += String(i) + "    " + name + " " +
               String(arr[i].usStackHighWaterMark) + "   " +
               String(arr[i].uxCurrentPriority) + "     " +
               states[min((int)arr[i].eCurrentState,5)] + "\n";
      }
      vPortFree(arr);
    }
    return out;
  }

  if (args.startsWith("kill ")) {
    int idx = args.substring(5).toInt();
    UBaseType_t taskCount = uxTaskGetNumberOfTasks();
    TaskStatus_t* arr = (TaskStatus_t*)pvPortMalloc(taskCount * sizeof(TaskStatus_t));
    uint32_t rt=0;
    if (arr) {
      taskCount = uxTaskGetSystemState(arr, taskCount, &rt);
      if (idx>=0 && idx<(int)taskCount) { vTaskDelete(arr[idx].xHandle); vPortFree(arr); return "Killed task " + String(idx) + "\n"; }
      vPortFree(arr);
    }
    return "Task not found.\n";
  }

  if (args == "sysinfo") {
    return "Chip: " + String(ESP.getChipModel()) + "\n"
           "CPU: " + String(ESP.getCpuFreqMHz()) + "MHz\n"
           "RAM: " + String(ESP.getFreeHeap()/1024) + "KB free / " + String(ESP.getHeapSize()/1024) + "KB total\n"
           "Flash: " + String(ESP.getFlashChipSize()/1024) + "KB\n"
           "SPIFFS: " + String(SPIFFS.usedBytes()/1024) + "/" + String(SPIFFS.totalBytes()/1024) + "KB\n"
           "WiFi: " + WiFi.SSID() + " (" + WiFi.localIP().toString() + ")\n"
           "Uptime: " + String(millis()/1000) + "s\n";
  }

  return "Usage: taskman [open|ps|kill <id>|sysinfo]\n";
}

} // namespace TaskManagerApp
