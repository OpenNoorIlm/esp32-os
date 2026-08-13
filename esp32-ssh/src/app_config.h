// app_config.h -- reads the "requirements" list out of a CLIENT GUI app's
// config.json (e.g. esp32-ssh/apps/esp32-app/config.json), so --app can
// pip-install whatever the app declares before launching it.
//
// Hand-rolled on purpose, same rationale as config_store.h: every
// config.json here is OUR OWN generated format (name/version/description/
// entry/python/uses_packages/requirements, all flat strings or a flat
// string array), and we own every file that will ever be read this way.
// Pulling in a full JSON library for one known key would be overkill.
#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

namespace AppConfig {

// Returns the app's declared pip requirements, e.g. ["PyQt5"] for
// esp32-app. Empty if config.json is missing or has no "requirements" key
// (apps with no extra dependencies, like robot-face-demo, just omit it).
inline std::vector<std::string> readRequirements(const std::string& appDir) {
  std::vector<std::string> reqs;
  char sepc =
#ifdef _WIN32
    '\\';
#else
    '/';
#endif
  std::string path = appDir;
  if (!path.empty() && path.back() != sepc) path += sepc;
  path += "config.json";

  std::ifstream f(path);
  if (!f.good()) return reqs; // no config.json next to main.py -- fine

  std::stringstream buf;
  buf << f.rdbuf();
  std::string text = buf.str();

  size_t keyPos = text.find("\"requirements\"");
  if (keyPos == std::string::npos) return reqs;

  size_t open = text.find('[', keyPos);
  size_t close = text.find(']', keyPos);
  if (open == std::string::npos || close == std::string::npos || close < open)
    return reqs; // malformed -- treat as "nothing declared" rather than crash

  std::string arr = text.substr(open + 1, close - open - 1);
  size_t i = 0;
  while (i < arr.size()) {
    size_t q1 = arr.find('"', i);
    if (q1 == std::string::npos) break;
    size_t q2 = arr.find('"', q1 + 1);
    if (q2 == std::string::npos) break;
    std::string item = arr.substr(q1 + 1, q2 - q1 - 1);
    if (!item.empty()) reqs.push_back(item);
    i = q2 + 1;
  }
  return reqs;
}

} // namespace AppConfig
