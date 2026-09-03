#pragma once
// ── driver_manager.h ──────────────────────────────────────────────────────────
// NoorOS Driver System (.dvr format)
//
// DVR file format (Lua source):
//   --[[dvr
//     name    = "my_driver"
//     version = "1.0"
//     author  = "Noor"
//     reboot  = "ask"        -- "auto", "ask", "manual", or "none"
//     undo    = "undo.lua"   -- optional undo script
//   --]]
//   -- driver body (Lua code, runs at boot after OS init)
//   os.hook("boot", function() ... end)
//   os.hook("sensor_read", function() ... end)
//   driver.register("my_driver", { ... })
//
// Storage: /drivers/<name>.dvr  (source)
//          /drivers/<name>.dvc  (compiled cache, Lua bytecode)
//          /drivers/<name>.undo (undo script if provided)
//          /drivers/manifest.lua (list of installed drivers)
//
// editable.dvr lives at /drivers/editable.dvr
// backup.dvr   lives at /drivers/backup.dvr  (auto-snapshot before each edit)
//
// Shell commands:
//   driver install <path>       — install a .dvr file
//   driver remove  <name>       — remove driver (runs undo if exists)
//   driver list                 — list installed drivers
//   driver info    <name>       — show driver metadata
//   driver reload  <name>       — reload without reboot
//   driver edit    <name>       — open in nano/TFT editor
//   driver backup  <name>       — manual backup
//   driver restore <name>       — restore from backup
//   os reboot [--force-reboot]  — reboot (apply pending driver changes)
// ─────────────────────────────────────────────────────────────────────────────

#include <Arduino.h>
#include <SPIFFS.h>
#include "fs_manager.h"
#include "lua_engine.h"

#include <TFT_eSPI.h>
#include "tft_manager.h"
extern TFT_eSPI tft;

namespace DriverManager {

// ── DVR metadata ──────────────────────────────────────────────────────────────
struct DvrMeta {
  String name;
  String version;
  String author;
  String reboot;   // "auto" | "ask" | "manual" | "none"
  String undo;     // undo script filename or empty
  bool   valid;
};

// ── Parse DVR header comment ──────────────────────────────────────────────────
DvrMeta parseMeta(const String& src) {
  DvrMeta m;
  m.valid   = false;
  m.reboot  = "ask";

  int start = src.indexOf("--[[dvr");
  int end   = src.indexOf("--]]", start);
  if (start < 0 || end < 0) return m;

  String header = src.substring(start + 7, end);

  auto field = [&](const char* key) -> String {
    int ki = header.indexOf(key);
    if (ki < 0) return "";
    int eq = header.indexOf('=', ki);
    int q1 = header.indexOf('"', eq);
    int q2 = header.indexOf('"', q1 + 1);
    if (q1 < 0 || q2 < 0) return "";
    return header.substring(q1 + 1, q2);
  };

  m.name    = field("name");
  m.version = field("version");
  m.author  = field("author");
  m.reboot  = field("reboot");
  m.undo    = field("undo");
  if (m.reboot.isEmpty()) m.reboot = "ask";
  m.valid   = !m.name.isEmpty();
  return m;
}

// ── Storage helpers ───────────────────────────────────────────────────────────
String dvrPath(const String& name)    { return "/drivers/" + name + ".dvr"; }
String cachePath(const String& name)  { return "/drivers/" + name + ".dvc"; }
String undoPath(const String& name)   { return "/drivers/" + name + ".undo"; }
String backupPath(const String& name) { return "/drivers/" + name + ".bak"; }
String manifestPath()                 { return "/drivers/manifest.lua"; }

bool fileExists(const String& path) {
  return SPIFFS.exists(path);
}

String readFile(const String& path) {
  if (!SPIFFS.exists(path)) return "";
  File f = SPIFFS.open(path, "r");
  if (!f) return "";
  String s = f.readString();
  f.close();
  return s;
}

bool writeFile(const String& path, const String& content) {
  File f = SPIFFS.open(path, "w");
  if (!f) return false;
  f.print(content);
  f.close();
  return true;
}

bool deleteFile(const String& path) {
  if (!SPIFFS.exists(path)) return true;
  return SPIFFS.remove(path);
}

// ── Manifest ──────────────────────────────────────────────────────────────────
// Format: Lua table  { "driver1", "driver2", ... }
std::vector<String> loadManifest() {
  std::vector<String> list;
  String m = readFile(manifestPath());
  if (m.isEmpty()) return list;
  int i = 0;
  while (true) {
    int q1 = m.indexOf('"', i);
    if (q1 < 0) break;
    int q2 = m.indexOf('"', q1 + 1);
    if (q2 < 0) break;
    list.push_back(m.substring(q1 + 1, q2));
    i = q2 + 1;
  }
  return list;
}

void saveManifest(const std::vector<String>& list) {
  String m = "return {";
  for (int i = 0; i < (int)list.size(); i++) {
    if (i > 0) m += ",";
    m += "\"" + list[i] + "\"";
  }
  m += "}";
  writeFile(manifestPath(), m);
}

void addToManifest(const String& name) {
  auto list = loadManifest();
  for (auto& n : list) if (n == name) return; // already there
  list.push_back(name);
  saveManifest(list);
}

void removeFromManifest(const String& name) {
  auto list = loadManifest();
  list.erase(std::remove(list.begin(), list.end(), name), list.end());
  saveManifest(list);
}

// ── Backup ────────────────────────────────────────────────────────────────────
bool backup(const String& name) {
  String src = readFile(dvrPath(name));
  if (src.isEmpty()) return false;
  return writeFile(backupPath(name), src);
}

bool restore(const String& name) {
  String bak = readFile(backupPath(name));
  if (bak.isEmpty()) return false;
  return writeFile(dvrPath(name), bak);
}

// ── Run undo script ───────────────────────────────────────────────────────────
String runUndo(const String& name) {
  String undo = readFile(undoPath(name));
  if (undo.isEmpty()) return "No undo script for " + name + "\n";
  StringPrint _sp_undo; LuaEngine::eval(undo, _sp_undo);
  return "Undo ran for " + name + ":\n" + _sp_undo.buf;
}

// ── TFT reboot prompt ─────────────────────────────────────────────────────────
bool tftAskReboot(const String& driverName) {
  tft.fillRect(20, 80, 200, 160, 0x0821);
  tft.drawRoundRect(20, 80, 200, 160, 8, 0x07FF);
  tft.setTextColor(0xFEA0, 0x0821);
  tft.setTextSize(2);
  tft.setCursor(30, 95);
  tft.print("Reboot?");
  tft.setTextColor(0xFFFF, 0x0821);
  tft.setTextSize(1);
  tft.setCursor(28, 120);
  tft.print("Driver installed:");
  tft.setCursor(28, 134);
  tft.print(driverName);
  tft.setCursor(28, 148);
  tft.print("Reboot to apply?");
  // YES button
  tft.fillRoundRect(30, 180, 70, 40, 6, 0x07E0);
  tft.setTextColor(0x0000, 0x07E0);
  tft.setTextSize(2);
  tft.setCursor(44, 192);
  tft.print("YES");
  // NO button
  tft.fillRoundRect(140, 180, 70, 40, 6, 0xF800);
  tft.setTextColor(0xFFFF, 0xF800);
  tft.setCursor(157, 192);
  tft.print("NO");

  // Wait for touch
  unsigned long timeout = millis() + 30000; // 30s timeout = auto YES
  while (millis() < timeout) {
    if (touch.tirqTouched() && touch.touched()) {
      TS_Point p = touch.getPoint();
      int tx = map(p.x, 200, 3800, 0, 240);
      int ty = map(p.y, 200, 3800, 0, 320);
      if (tx >= 30 && tx <= 100 && ty >= 180 && ty <= 220) return true;
      if (tx >= 140 && tx <= 210 && ty >= 180 && ty <= 220) return false;
    }
    delay(50);
  }
  return true; // timeout = reboot
}

// ── TFT crash/error screen ────────────────────────────────────────────────────
void tftErrorScreen(const String& driverName, const String& error) {
  tft.fillScreen(0x0000);
  tft.fillRect(0, 0, 240, 40, 0xF800);
  tft.setTextColor(0xFFFF, 0xF800);
  tft.setTextSize(2);
  tft.setCursor(8, 12);
  tft.print("DVR ERROR");
  tft.setTextColor(0xFEA0, 0x0000);
  tft.setTextSize(1);
  tft.setCursor(4, 50);
  tft.print("Driver: " + driverName);
  tft.setTextColor(0xFFFF, 0x0000);
  tft.setCursor(4, 66);
  // Word-wrap error
  int y = 66;
  for (int i = 0; i < (int)error.length() && y < 260; i += 36) {
    tft.setCursor(4, y);
    tft.print(error.substring(i, min((int)error.length(), i + 36)));
    y += 14;
  }
  // Restore button
  tft.fillRoundRect(20, 270, 90, 36, 6, 0x07E0);
  tft.setTextColor(0x0000, 0x07E0);
  tft.setTextSize(1);
  tft.setCursor(28, 284);
  tft.print("RESTORE BAK");
  // Continue button
  tft.fillRoundRect(130, 270, 90, 36, 6, 0x4208);
  tft.setTextColor(0xFFFF, 0x4208);
  tft.setCursor(138, 284);
  tft.print("IGNORE");

  unsigned long timeout = millis() + 20000;
  while (millis() < timeout) {
    if (touch.tirqTouched() && touch.touched()) {
      TS_Point p = touch.getPoint();
      int tx = map(p.x, 200, 3800, 0, 240);
      int ty = map(p.y, 200, 3800, 0, 320);
      if (tx >= 20 && tx <= 110 && ty >= 270 && ty <= 306) {
        restore(driverName);
        ESP.restart();
      }
      if (tx >= 130 && tx <= 220 && ty >= 270 && ty <= 306) return;
    }
    delay(50);
  }
}

// ── Install driver ────────────────────────────────────────────────────────────
String install(const String& srcPath, bool forceReboot = false) {
  String src = readFile(srcPath);
  if (src.isEmpty()) return "Error: cannot read " + srcPath + "\n";

  DvrMeta meta = parseMeta(src);
  if (!meta.valid) return "Error: invalid DVR file (missing --[[dvr header)\n";

  // Backup existing if present
  if (fileExists(dvrPath(meta.name))) backup(meta.name);

  // Write driver source
  if (!writeFile(dvrPath(meta.name), src))
    return "Error: cannot write to /drivers/" + meta.name + ".dvr\n";

  // Write undo script if embedded
  int undoStart = src.indexOf("--[[undo");
  int undoEnd   = src.indexOf("--]]", undoStart + 1);
  if (undoStart >= 0 && undoEnd >= 0) {
    String undoSrc = src.substring(undoStart + 8, undoEnd);
    writeFile(undoPath(meta.name), undoSrc);
  }

  // Validate: try running in Lua sandbox
  StringPrint _sp_testOut;
  bool ok = LuaEngine::eval("-- DVR validation\n" + src, _sp_testOut);
  if (!ok) {
    // Restore backup, show error
    String errMsg = "Validation failed:\n" + _sp_testOut.buf;
    tftErrorScreen(meta.name, errMsg);
    restore(meta.name);
    return errMsg;
  }

  addToManifest(meta.name);

  String result = "Driver '" + meta.name + "' v" + meta.version + " installed.\n";

  // Log it
  String logEntry = "[" + String(millis()) + "] installed " + meta.name + " v" + meta.version + "\n";
  File logF = SPIFFS.open("/logs/driver.log", "a");
  if (logF) { logF.print(logEntry); logF.close(); }

  // Reboot logic
  String rb = forceReboot ? "auto" : meta.reboot;
  if (rb == "auto") {
    result += "Rebooting...\n";
    delay(500);
    ESP.restart();
  } else if (rb == "ask") {
    if (tftAskReboot(meta.name)) {
      result += "Rebooting...\n";
      delay(300);
      ESP.restart();
    } else {
      result += "Reboot skipped. Run 'os reboot' to apply.\n";
    }
  } else {
    result += "Run 'os reboot' to apply changes.\n";
  }

  return result;
}

// ── Remove driver ─────────────────────────────────────────────────────────────
String remove(const String& name) {
  if (!fileExists(dvrPath(name))) return "Driver '" + name + "' not found.\n";
  String undoResult = runUndo(name);
  backup(name); // keep backup of removed driver
  deleteFile(dvrPath(name));
  deleteFile(cachePath(name));
  removeFromManifest(name);
  return "Driver '" + name + "' removed.\n" + undoResult;
}

// ── Load all drivers at boot ──────────────────────────────────────────────────
void loadAll() {
  // Ensure directories exist
  if (!SPIFFS.exists("/drivers")) {
    // SPIFFS doesn't have real dirs but files with / in path work
  }
  if (!SPIFFS.exists("/logs")) {}

  // Create editable.dvr if not present
  if (!fileExists("/drivers/editable.dvr")) {
    writeFile("/drivers/editable.dvr",
      "--[[dvr\n"
      "  name    = \"editable\"\n"
      "  version = \"1.0\"\n"
      "  author  = \"NoorOS\"\n"
      "  reboot  = \"ask\"\n"
      "--]]\n"
      "-- NoorOS editable.dvr\n"
      "-- Edit ANYTHING here: pins, speeds, colors, shell commands, boot behavior.\n"
      "-- Changes apply after reboot. Backup is auto-saved before each edit.\n"
      "-- This file runs as Lua after all OS modules are loaded.\n\n"
      "-- ── Motor speeds ────────────────────────────────────────────────────\n"
      "os.set('motor.speed.default', 200)   -- 0-255\n"
      "os.set('motor.speed.turn',    180)\n"
      "os.set('motor.speed.slow',    120)\n\n"
      "-- ── Eye default ─────────────────────────────────────────────────────\n"
      "os.set('eye.default', 'Normal')\n\n"
      "-- ── Sensor intervals (ms) ───────────────────────────────────────────\n"
      "os.set('sensor.dht.interval',  2000)\n"
      "os.set('sensor.dist.interval', 500)\n"
      "os.set('sensor.fast.interval', 50)\n\n"
      "-- ── Shell custom commands ───────────────────────────────────────────\n"
      "-- shell.add('hi', function(args) return 'Hello, ' .. args .. '!' end)\n\n"
      "-- ── Boot hook ───────────────────────────────────────────────────────\n"
      "os.hook('boot', function()\n"
      "  -- Runs after full OS boot\n"
      "  -- screen.eyes('Happy')\n"
      "end)\n\n"
      "-- ── Pin overrides ───────────────────────────────────────────────────\n"
      "-- os.set('pin.servo',    12)\n"
      "-- os.set('pin.laser',    26)\n"
      "-- os.set('pin.dht',      32)\n\n"
      "-- ── WiFi ────────────────────────────────────────────────────────────\n"
      "-- os.set('wifi.ssid',     'MyNetwork')\n"
      "-- os.set('wifi.password', 'MyPassword')\n\n"
      "-- ── TFT theme ───────────────────────────────────────────────────────\n"
      "-- os.set('theme.bg',      '0x0000')\n"
      "-- os.set('theme.accent',  'cyan')\n"
      "-- os.set('theme.text',    'white')\n"
    );
    // Backup immediately
    backup("editable");
  }

  // Load all installed drivers
  auto list = loadManifest();
  for (auto& name : list) {
    String src = readFile(dvrPath(name));
    if (src.isEmpty()) continue;
    StringPrint _sp_out;
    bool ok = LuaEngine::eval(src, _sp_out);
    if (!ok) {
      // Log error
      File logF = SPIFFS.open("/logs/crash.log", "a");
      if (logF) {
        logF.print("[boot] driver '" + name + "' error: " + _sp_out.buf + "\n");
        logF.close();
      }
      // Show TFT error screen
      tftErrorScreen(name, _sp_out.buf);
      // If editable.dvr failed restore backup
      if (name == "editable") restore("editable");
    }
  }
}

// ── Shell command handler ─────────────────────────────────────────────────────
String shellCmd(const String& args) {
  int sp   = args.indexOf(' ');
  String sub  = sp < 0 ? args : args.substring(0, sp);
  String rest = sp < 0 ? "" : args.substring(sp + 1);
  rest.trim();

  if (sub == "install") {
    bool force = rest.endsWith("--force-reboot");
    String path = force ? rest.substring(0, rest.lastIndexOf(' ')) : rest;
    path.trim();
    return install(path, force);
  }
  if (sub == "remove")  return remove(rest);
  if (sub == "restore") return restore(rest) ? "Restored " + rest + " from backup.\n" : "No backup found.\n";
  if (sub == "backup")  return backup(rest)  ? "Backed up " + rest + ".\n" : "Failed.\n";
  if (sub == "reload") {
    String src = readFile(dvrPath(rest));
    if (src.isEmpty()) return "Driver '" + rest + "' not found.\n";
    StringPrint _sp_out;
    LuaEngine::eval(src, _sp_out);
    return "Reloaded " + rest + ":\n" + _sp_out.buf;
  }
  if (sub == "list") {
    auto list = loadManifest();
    if (list.empty()) return "No drivers installed.\n";
    String out = "Installed drivers:\n";
    for (auto& n : list) {
      String src = readFile(dvrPath(n));
      DvrMeta m  = parseMeta(src);
      out += "  " + n + " v" + m.version + " (" + m.author + ")\n";
    }
    return out;
  }
  if (sub == "info") {
    String src = readFile(dvrPath(rest));
    if (src.isEmpty()) return "Driver '" + rest + "' not found.\n";
    DvrMeta m = parseMeta(src);
    return "Name:    " + m.name    + "\n"
           "Version: " + m.version + "\n"
           "Author:  " + m.author  + "\n"
           "Reboot:  " + m.reboot  + "\n"
           "Undo:    " + (m.undo.isEmpty() ? "none" : m.undo) + "\n";
  }
  if (sub == "log") {
    return readFile("/logs/driver.log");
  }

  return "Usage: driver [install <path>|remove <name>|list|info <name>|reload <name>|backup <name>|restore <name>|log]\n";
}

// ── os reboot shell command ───────────────────────────────────────────────────
String osCmd(const String& args) {
  if (args == "reboot" || args == "reboot --force-reboot") {
    delay(200);
    ESP.restart();
    return ""; // never reached
  }
  if (args.startsWith("set ")) {
    // os set key value
    int sp2 = args.indexOf(' ', 4);
    if (sp2 < 0) return "Usage: os set <key> <value>\n";
    String key = args.substring(4, sp2);
    String val = args.substring(sp2 + 1);
    // Store in SPIFFS
    writeFile("/os/config/" + key, val);
    return "Set " + key + " = " + val + "\n";
  }
  if (args.startsWith("get ")) {
    String key = args.substring(4);
    key.trim();
    String val = readFile("/os/config/" + key);
    return val.isEmpty() ? "Not set\n" : val + "\n";
  }
  if (args == "version") {
    return "NoorOS 1.0 | Built on esp32-os\n";
  }
  return "Usage: os [reboot|set <key> <value>|get <key>|version]\n";
}

} // namespace DriverManager
