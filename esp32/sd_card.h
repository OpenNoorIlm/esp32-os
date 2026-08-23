#pragma once
// ── sd_card.h ─────────────────────────────────────────────────────────────────
// SD card manager for NoorRobot
// Uses the TFT board's onboard SD slot (shares VSPI with TFT)
// Pins: SCK=18, MISO=19, MOSI=23, CS=21
//
// Library: SD (built into Arduino ESP32 core)
// ─────────────────────────────────────────────────────────────────────────────

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>

// SD shares VSPI bus with TFT (SCK=18, MISO=19, MOSI=23)
// CS on GPIO21 (free pin — Hall sensor moved to GPIO34 if needed)
#define SD_CS_PIN   21
#define SD_SCK_PIN  18
#define SD_MISO_PIN 19
#define SD_MOSI_PIN 23

namespace SdCard {

bool _mounted = false;

// ── Init ──────────────────────────────────────────────────────────────────────
bool begin() {
  // SPI already started by TFT_eSPI — just init SD with same bus
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("[SD] Mount failed — no card or wiring issue");
    _mounted = false;
    return false;
  }
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("[SD] Mounted. Size: %lluMB  Type: %d\n", cardSize, SD.cardType());
  _mounted = true;
  return true;
}

bool mounted() { return _mounted; }

// ── File helpers ──────────────────────────────────────────────────────────────

bool exists(const char* path) {
  if (!_mounted) return false;
  return SD.exists(path);
}

File open(const char* path, const char* mode = FILE_READ) {
  return SD.open(path, mode);
}

bool remove(const char* path) {
  if (!_mounted) return false;
  return SD.remove(path);
}

bool mkdir(const char* path) {
  if (!_mounted) return false;
  return SD.mkdir(path);
}

uint64_t totalBytes() { return _mounted ? SD.totalBytes() : 0; }
uint64_t usedBytes()  { return _mounted ? SD.usedBytes()  : 0; }
uint64_t freeBytes()  { return _mounted ? (SD.totalBytes() - SD.usedBytes()) : 0; }

// ── List directory ────────────────────────────────────────────────────────────
String listDir(const char* path, bool recursive = false) {
  if (!_mounted) return "SD not mounted\n";
  File root = SD.open(path);
  if (!root || !root.isDirectory()) return String("Not a directory: ") + path + "\n";
  String out = "";
  File f = root.openNextFile();
  while (f) {
    if (f.isDirectory()) {
      out += String("[DIR]  ") + f.name() + "\n";
      if (recursive) {
        out += listDir(f.name(), true);
      }
    } else {
      out += String("       ") + f.name() + "  (" + String(f.size()) + " bytes)\n";
    }
    f = root.openNextFile();
  }
  return out.length() ? out : "(empty)\n";
}

// ── Shell command handler ─────────────────────────────────────────────────────
// Usage: sd [status|ls [path]|rm <path>|mkdir <path>|free]
String handleCommand(const String& args) {
  if (!_mounted && args != "status") {
    // Try remounting
    begin();
    if (!_mounted) return "SD not mounted. Check wiring (CS=GPIO21, shares VSPI).\n";
  }

  String cmd = args;
  cmd.trim();

  if (cmd == "" || cmd == "status") {
    if (!_mounted) return "SD: not mounted\n";
    return String("SD: mounted\n") +
           "Total: " + String(totalBytes() / (1024*1024)) + " MB\n" +
           "Used:  " + String(usedBytes()  / (1024*1024)) + " MB\n" +
           "Free:  " + String(freeBytes()  / (1024*1024)) + " MB\n" +
           "Type:  " + String(SD.cardType()) + "\n";
  }

  if (cmd.startsWith("ls")) {
    String path = cmd.length() > 3 ? cmd.substring(3) : "/";
    path.trim();
    return listDir(path.c_str());
  }

  if (cmd.startsWith("rm ")) {
    String path = cmd.substring(3); path.trim();
    return remove(path.c_str()) ? "Removed: " + path + "\n" : "Failed to remove: " + path + "\n";
  }

  if (cmd.startsWith("mkdir ")) {
    String path = cmd.substring(6); path.trim();
    return mkdir(path.c_str()) ? "Created: " + path + "\n" : "Failed to create: " + path + "\n";
  }

  if (cmd == "free") {
    return "Free: " + String(freeBytes() / (1024*1024)) + " MB\n";
  }

  return "Usage: sd [status|ls [path]|rm <path>|mkdir <path>|free]\n";
}

} // namespace SdCard
