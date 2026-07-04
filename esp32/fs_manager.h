#pragma once
#include <LittleFS.h>

#define VROOT "/storage/esp32"

// Wraps LittleFS so the shell can present paths like "/storage/esp32/apps"
// while the real filesystem underneath just uses "/apps".
namespace FsManager {

inline bool begin() {
  return LittleFS.begin(true); // format on first-ever boot if unformatted
}

inline String toVirtual(const String& real) {
  return real == "/" ? String(VROOT) : (String(VROOT) + real);
}

// Resolves a typed path (relative or absolute, may contain . or ..) against
// the current working directory into a clean absolute real path.
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
  String resolved = "";
  for (int i = 0; i < n; i++) resolved += "/" + parts[i];
  return resolved.length() ? resolved : "/";
}

} // namespace FsManager

namespace FsManager {

inline bool isDir(const String& realPath) {
  File f = LittleFS.open(realPath);
  return f && f.isDirectory();
}

inline String ls(const String& realPath) {
  File dir = LittleFS.open(realPath);
  if (!dir || !dir.isDirectory()) return "error: not a directory";
  String out;
  File f = dir.openNextFile();
  while (f) {
    out += String(f.isDirectory() ? "d " : "f ") + f.name() + "\n";
    f = dir.openNextFile();
  }
  return out.length() ? out : "(empty)";
}

inline bool mkdir(const String& realPath) { return LittleFS.mkdir(realPath); }

inline bool remove(const String& realPath) {
  return isDir(realPath) ? LittleFS.rmdir(realPath) : LittleFS.remove(realPath);
}

inline String cat(const String& realPath) {
  File f = LittleFS.open(realPath, "r");
  if (!f) return "error: no such file";
  String out = f.readString();
  f.close();
  return out;
}

inline String df() {
  size_t total = LittleFS.totalBytes();
  size_t used  = LittleFS.usedBytes();
  size_t free  = total - used;
  float pct = total ? (100.0f * used / total) : 0.0f;
  String s;
  s += "Filesystem: LittleFS (" + String(VROOT) + ")\n";
  s += "Total: " + String(total / 1024.0f, 1) + " KB\n";
  s += "Used:  " + String(used  / 1024.0f, 1) + " KB (" + String(pct, 1) + "%)\n";
  s += "Free:  " + String(free / 1024.0f, 1) + " KB\n";
  return s;
}

} // namespace FsManager
