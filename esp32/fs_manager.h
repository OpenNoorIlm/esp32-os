#pragma once
#include <LittleFS.h>
#include <SD.h>
#include <SPI.h>
#include <Preferences.h>

// SD card wiring (standard ESP32 VSPI bus -- see wiring.md for full details).
// Only CS is dedicated to the SD card; MOSI/MISO/SCK are the shared VSPI bus.
#define SD_CS_PIN 5

#define VROOT "/storage/esp32"

// Wraps either LittleFS (internal flash) or SD (external card) behind one
// interface, so the shell can present paths like "/storage/esp32/apps"
// regardless of which physical storage is actually backing it.
//
// LittleFS and SD both derive from fs::FS in the ESP32 Arduino core, and
// both implement the FS-level virtual methods (open/mkdir/remove/rmdir) --
// that's what lets activeFs() be swapped polymorphically. totalBytes()/
// usedBytes() are NOT part of the common FS interface though (each class
// defines them separately), so df() below has to special-case per backend.
namespace FsManager {

inline Preferences storagePrefs;
inline bool usingSd = false; // which backend is actually mounted right now

inline String loadBackendPref() {
  storagePrefs.begin("storage", true);
  String b = storagePrefs.getString("backend", "internal");
  storagePrefs.end();
  return b;
}

inline void saveBackendPref(const String& backend) {
  storagePrefs.begin("storage", false);
  storagePrefs.putString("backend", backend);
  storagePrefs.end();
}

inline fs::FS& activeFs() { return usingSd ? (fs::FS&)SD : (fs::FS&)LittleFS; }

// Mounts whichever backend was last saved. If SD was selected but the card
// isn't actually present/working, falls back to internal flash rather than
// bricking the filesystem entirely, and says so on Serial.
inline bool begin() {
  bool ok = LittleFS.begin(true); // format on first-ever boot if unformatted
  String backend = loadBackendPref();

  if (backend == "sd") {
    SPI.begin(18, 19, 23, SD_CS_PIN); // SCK, MISO, MOSI, CS -- standard VSPI
    if (SD.begin(SD_CS_PIN)) {
      usingSd = true;
      Serial.println("Storage: SD card mounted (" + String(SD.cardSize() / (1024 * 1024)) + " MB card)");
    } else {
      usingSd = false;
      Serial.println("Storage: SD card selected but not found/working -- falling back to internal flash. Check wiring.md.");
    }
  } else {
    usingSd = false;
    Serial.println("Storage: internal flash (LittleFS)");
  }
  return ok;
}

// Validates the requested backend actually works BEFORE saving the
// preference, so a bad SD card can't lock you out of storage after restart.
inline String changeStorage(const String& choice) {
  if (choice == "1" || choice.equalsIgnoreCase("internal")) {
    saveBackendPref("internal");
    return "Storage backend set to: internal flash (LittleFS). Run 'restart' to apply.";
  }
  if (choice == "2" || choice.equalsIgnoreCase("sd")) {
    SPI.begin(18, 19, 23, SD_CS_PIN);
    if (!SD.begin(SD_CS_PIN)) {
      return "error: could not detect an SD card on CS pin " + String(SD_CS_PIN) +
             ". Check wiring.md and that a card is inserted. Storage unchanged.";
    }
    saveBackendPref("sd");
    return "Storage backend set to: SD card (" + String(SD.cardSize() / (1024 * 1024)) +
           " MB detected). Run 'restart' to apply.";
  }
  return "error: invalid choice '" + choice + "' (expected 1 or 2). Storage unchanged.";
}

} // namespace FsManager

namespace FsManager {

inline String toVirtual(const String& real) {
  return real == "/" ? String(VROOT) : (String(VROOT) + real);
}

// Resolves a typed path (relative or absolute, may contain . or ..) against
// the current working directory into a clean absolute real path.
//
// Also accepts the VROOT display prefix ("storage/esp32") as an alias for
// the real filesystem root. toVirtual() always PREPENDS that prefix when
// SHOWING a path back to the user (pwd, the install-completion message,
// the "PROMPT" line), so it's natural to type it right back into cd/ls/cat
// -- without this, doing exactly that produces a confusing "no such
// directory" even though the real path underneath is fine. Nothing on the
// actual filesystem is ever named "storage" or "esp32" at the root (this
// project doesn't nest real dirs that way), so stripping that leading pair
// is safe and makes both forms land in the same place, e.g.
// "cd storage/esp32/apps" and "cd /apps" resolve identically.
inline String toRealPath(const String& cwd, const String& input) {
  String path = input;
  if (path.length() == 0) return cwd;
  if (!path.startsWith("/")) path = cwd + "/" + path;

  String parts[32];
  int n = 0, start = 0;
  path += "/";
  for (int i = 0; i < (int)path.length(); i++) {
    if (path[i] == '/') {
      String seg = path.substring(start, i);
      start = i + 1;
      if (seg.length() == 0 || seg == ".") continue;
      if (seg == "..") { if (n > 0) n--; continue; }
      if (n < 32) parts[n++] = seg;
    }
  }

  // Strip a leading "storage"/"esp32" pair (case-insensitive) -- only as a
  // matched pair, so a genuine top-level folder someone names "storage" on
  // its own still works normally.
  int vstart = 0;
  if (n >= 2 && parts[0].equalsIgnoreCase("storage") && parts[1].equalsIgnoreCase("esp32")) vstart = 2;

  String resolved = "";
  for (int i = vstart; i < n; i++) resolved += "/" + parts[i];
  return resolved.length() ? resolved : "/";
}

} // namespace FsManager

namespace FsManager {

inline bool isDir(const String& realPath) {
  File f = activeFs().open(realPath);
  return f && f.isDirectory();
}

inline String ls(const String& realPath) {
  File dir = activeFs().open(realPath);
  if (!dir || !dir.isDirectory()) return "error: not a directory";
  String out;
  File f = dir.openNextFile();
  while (f) {
    out += String(f.isDirectory() ? "d " : "f ") + f.name() + "\n";
    f = dir.openNextFile();
  }
  return out.length() ? out : "(empty)";
}

// "ls -l" variant: adds file size (in bytes) as a second column, "-" for dirs.
inline String lsLong(const String& realPath) {
  File dir = activeFs().open(realPath);
  if (!dir || !dir.isDirectory()) return "error: not a directory";
  String out;
  File f = dir.openNextFile();
  while (f) {
    out += String(f.isDirectory() ? "d " : "f ");
    out += (f.isDirectory() ? String("-") : String(f.size())) + "\t";
    out += String(f.name()) + "\n";
    f = dir.openNextFile();
  }
  return out.length() ? out : "(empty)";
}

inline bool mkdir(const String& realPath) { return activeFs().mkdir(realPath); }

// "mkdir -p": creates parent directories as needed, like the real thing.
inline bool mkdirP(const String& realPath) {
  if (realPath.length() == 0 || realPath == "/") return true;
  if (isDir(realPath)) return true;
  int lastSlash = realPath.lastIndexOf('/');
  if (lastSlash > 0) {
    String parent = realPath.substring(0, lastSlash);
    if (!isDir(parent)) mkdirP(parent);
  }
  return activeFs().mkdir(realPath);
}

inline bool remove(const String& realPath) {
  return isDir(realPath) ? activeFs().rmdir(realPath) : activeFs().remove(realPath);
}

// "rm -r": recursively deletes a directory tree (or just a file, same as
// plain remove() in that case). Returns false if anything along the way
// failed to delete, but still attempts to clean up as much as it can.
inline bool removeRecursive(const String& realPath) {
  if (!isDir(realPath)) return activeFs().remove(realPath);
  File dir = activeFs().open(realPath);
  if (!dir) return false;
  bool allOk = true;
  File f = dir.openNextFile();
  while (f) {
    String childName = String(f.name());
    bool childIsDir = f.isDirectory();
    f.close();
    String childPath = realPath + "/" + childName;
    if (childIsDir) { if (!removeRecursive(childPath)) allOk = false; }
    else { if (!activeFs().remove(childPath)) allOk = false; }
    f = dir.openNextFile();
  }
  dir.close();
  if (!activeFs().rmdir(realPath)) allOk = false;
  return allOk;
}

inline String cat(const String& realPath) {
  File f = activeFs().open(realPath, "r");
  if (!f) return "error: no such file";
  String out = f.readString();
  f.close();
  return out;
}

// Exposed so other modules (e.g. package_manager.h) can write files onto
// whichever backend is currently active, instead of hardcoding LittleFS.
inline File openFile(const String& realPath, const char* mode) {
  return activeFs().open(realPath, mode);
}

inline String df() {
  size_t total, used;
  String label;
  if (usingSd) {
    total = SD.totalBytes();
    used  = SD.usedBytes();
    label = "SD card";
  } else {
    total = LittleFS.totalBytes();
    used  = LittleFS.usedBytes();
    label = "LittleFS (internal flash)";
  }
  size_t freeB = total - used;
  float pct = total ? (100.0f * used / total) : 0.0f;
  String s;
  s += "Filesystem: " + label + " (" + String(VROOT) + ")\n";
  s += "Total: " + String(total / 1024.0f, 1) + " KB\n";
  s += "Used:  " + String(used  / 1024.0f, 1) + " KB (" + String(pct, 1) + "%)\n";
  s += "Free:  " + String(freeB / 1024.0f, 1) + " KB\n";
  return s;
}

} // namespace FsManager
