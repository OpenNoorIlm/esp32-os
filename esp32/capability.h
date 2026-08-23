#pragma once
#include <Arduino.h>

// Detects what this specific board can actually do, so later OS features
// (Lua, TLS, app installer, etc.) can check before assuming resources exist.
struct DeviceCapabilities {
  String   chipModel;
  int      chipRevision;
  int      chipCores;
  uint32_t flashSizeBytes;
  bool     hasPsram;
  uint32_t psramSizeBytes;
  uint32_t freeHeap;
};

inline DeviceCapabilities detectCapabilities() {
  DeviceCapabilities c;
  c.chipModel      = ESP.getChipModel();
  c.chipRevision   = ESP.getChipRevision();
  c.chipCores      = ESP.getChipCores();
  c.flashSizeBytes = ESP.getFlashChipSize();
  c.hasPsram       = psramFound();
  c.psramSizeBytes = c.hasPsram ? ESP.getPsramSize() : 0;
  c.freeHeap       = ESP.getFreeHeap();
  return c;
}

inline String capabilitiesReport() {
  DeviceCapabilities c = detectCapabilities();
  String s;
  s += "Chip:     " + c.chipModel + " rev" + String(c.chipRevision) +
       " (" + String(c.chipCores) + " core)\n";
  s += "Flash:    " + String(c.flashSizeBytes / (1024 * 1024)) + " MB\n";
  s += "PSRAM:    " + (c.hasPsram
                        ? (String(c.psramSizeBytes / (1024 * 1024)) + " MB")
                        : String("not present")) + "\n";
  s += "Free RAM: " + String(c.freeHeap) + " bytes\n";
  return s;
}
