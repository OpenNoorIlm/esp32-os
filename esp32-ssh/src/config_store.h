// config_store.h -- tiny persisted settings file for esp32-ssh.
//
// Stores the last-used host + password (so Python bridge packages like
// lua/device/oled can call esp32-ssh --command without repeating
// credentials on every single call) and the preferred Python interpreter
// version for launching CLIENT GUI apps (--app).
//
// Hand-rolled flat JSON on purpose: the schema is 3 flat string fields and
// we own both the writer and the only reader, so pulling in a JSON library
// dependency isn't worth it (matches this project's existing style of
// hand-rolling sha256.cpp rather than adding a crypto dependency).
//
// File location:
//   Windows: %APPDATA%\NoorShell\config.json
//   Linux:   ~/.config/noorshell/config.json
#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstdlib>

#ifdef _WIN32
  #include <direct.h>
  #define MKDIR(p) _mkdir(p)
#else
  #include <sys/stat.h>
  #define MKDIR(p) mkdir(p, 0700)
#endif

struct NoorConfig {
  std::string host;
  std::string password;
  std::string pythonVersion = "3.10"; // legacy field, kept for old configs
  std::string pythonCmd;              // chosen interpreter argv, "|"-joined,
                                       // e.g. "C:\...\python.exe" or "py|-3.10"
                                       // empty = not chosen yet, ask via the picker
};

namespace ConfigStore {

inline std::string configDir() {
#ifdef _WIN32
  const char* appData = std::getenv("APPDATA");
  std::string base = appData ? appData : ".";
  return base + "\\NoorShell";
#else
  const char* home = std::getenv("HOME");
  std::string base = home ? home : ".";
  return base + "/.config/noorshell";
#endif
}

inline std::string configPath() {
#ifdef _WIN32
  return configDir() + "\\config.json";
#else
  return configDir() + "/config.json";
#endif
}

// Pulls out the string value of "key": "value" from raw flat JSON text.
// Good enough since we control the writer -- always one key per line,
// always double-quoted string values, no nesting, no escaping needed for
// the values we actually store (host/IP, version numbers). Passwords in
// practice won't contain literal `"` characters either, but if one ever
// did, this parser would mis-read it -- acceptable given this is a local,
// single-user convenience cache, not a hardened parser.
inline std::string extractField(const std::string& json, const std::string& key) {
  std::string needle = "\"" + key + "\"";
  size_t pos = json.find(needle);
  if (pos == std::string::npos) return "";
  pos = json.find(':', pos);
  if (pos == std::string::npos) return "";
  size_t q1 = json.find('"', pos + 1);
  if (q1 == std::string::npos) return "";
  size_t q2 = json.find('"', q1 + 1);
  if (q2 == std::string::npos) return "";
  return json.substr(q1 + 1, q2 - q1 - 1);
}

inline NoorConfig load() {
  NoorConfig cfg;
  std::ifstream f(configPath());
  if (!f) return cfg; // no saved config yet -- defaults are fine
  std::stringstream ss;
  ss << f.rdbuf();
  std::string json = ss.str();
  std::string h = extractField(json, "host");
  std::string p = extractField(json, "password");
  std::string v = extractField(json, "python_version");
  std::string pc = extractField(json, "python_cmd");
  if (!h.empty()) cfg.host = h;
  if (!p.empty()) cfg.password = p;
  if (!v.empty()) cfg.pythonVersion = v;
  if (!pc.empty()) cfg.pythonCmd = pc;
  return cfg;
}

inline void save(const NoorConfig& cfg) {
  MKDIR(configDir().c_str()); // ignore return -- fine if it already exists
  std::ofstream f(configPath());
  if (!f) return; // best-effort; a failed save just means credentials
                   // aren't cached for next time, nothing catastrophic
  f << "{\n"
    << "  \"host\": \"" << cfg.host << "\",\n"
    << "  \"password\": \"" << cfg.password << "\",\n"
    << "  \"python_version\": \"" << cfg.pythonVersion << "\",\n"
    << "  \"python_cmd\": \"" << cfg.pythonCmd << "\"\n"
    << "}\n";
}

// pythonCmd is stored as argv joined with "|" (e.g. "py|-3.10" or a single
// full path with no "|" at all). "|" was picked because it can't appear in
// a Windows path and interpreter argv pieces here are never more than a
// path or a short "-X.Y" flag.
inline std::vector<std::string> splitPythonCmd(const std::string& joined) {
  std::vector<std::string> out;
  std::stringstream ss(joined);
  std::string part;
  while (std::getline(ss, part, '|')) if (!part.empty()) out.push_back(part);
  return out;
}

inline std::string joinPythonCmd(const std::vector<std::string>& argv) {
  std::string out;
  for (size_t i = 0; i < argv.size(); i++) {
    if (i) out += "|";
    out += argv[i];
  }
  return out;
}

} // namespace ConfigStore
