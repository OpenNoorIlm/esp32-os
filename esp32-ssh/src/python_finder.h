// python_finder.h -- locates Python interpreters for launching CLIENT GUI
// apps (--app). Rather than silently preferring one hardcoded version,
// this lists every interpreter it can actually find on the system and
// lets the user pick which one to use (see choose()) -- that choice is
// then saved (see config_store.h's pythonCmd field) so it's only asked
// once, not on every single --app launch.
#pragma once
#include <string>
#include <vector>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>

#ifdef _WIN32
  #include <windows.h>
#else
  #include <dirent.h>
  #include <unistd.h>
#endif

namespace PythonFinder {

struct Candidate {
  std::string label;             // shown to the user, e.g. "3.10 -- C:\...\python.exe"
  std::vector<std::string> cmd;  // argv to exec, e.g. {"C:\...\python.exe"} or {"py","-3.10"}
};

inline bool fileExists(const std::string& path) {
  std::ifstream f(path);
  return f.good();
}

inline std::string trim(const std::string& s) {
  size_t a = s.find_first_not_of(" \t\r\n*");
  size_t b = s.find_last_not_of(" \t\r\n*");
  if (a == std::string::npos) return "";
  return s.substr(a, b - a + 1);
}

// Runs `cmd` and returns its stdout as text (stderr discarded). Used to
// read `py -0p`'s listing rather than just checking an exit code.
inline std::string runCaptured(const std::string& cmd) {
  std::string out;
#ifdef _WIN32
  std::string full = cmd + " 2>NUL";
  FILE* pipe = _popen(full.c_str(), "r");
#else
  std::string full = cmd + " 2>/dev/null";
  FILE* pipe = popen(full.c_str(), "r");
#endif
  if (!pipe) return out;
  char buf[512];
  while (fgets(buf, sizeof(buf), pipe)) out += buf;
#ifdef _WIN32
  _pclose(pipe);
#else
  pclose(pipe);
#endif
  return out;
}

#ifdef _WIN32

// Parses `py -0p` output. Typical lines look like:
//   -V:3.13          C:\Users\bismi\AppData\Local\Programs\Python\Python313\python.exe
//   -V:3.10 *        C:\Users\bismi\AppData\Local\Programs\Python\Python310\python.exe
// Columns are separated by runs of 2+ spaces, which is what we split on
// rather than trying to hand-parse the "-V:X.Y" tag format exactly (the
// launcher's tag format has changed across versions, e.g. "-3.13-64" vs
// "-V:3.13"; splitting on whitespace runs is robust to that).
inline std::vector<Candidate> fromPyLauncher() {
  std::vector<Candidate> out;
  std::string raw = runCaptured("py -0p");
  std::istringstream iss(raw);
  std::string line;
  while (std::getline(iss, line)) {
    size_t colon = line.find(":\\");
    if (colon == std::string::npos || colon == 0) continue; // not a "X:\..." path line
    size_t pathStart = colon - 1;
    while (pathStart > 0 && !isspace((unsigned char)line[pathStart - 1])) pathStart--;
    std::string path = line.substr(pathStart);
    while (!path.empty() && isspace((unsigned char)path.back())) path.pop_back();
    if (!fileExists(path)) continue;
    std::string tag = trim(line.substr(0, pathStart));
    out.push_back({tag + "  -- " + path, {path}});
  }
  return out;
}

// Globs "PythonXYZ" directories under common install roots, catching
// installs the py launcher doesn't know about (e.g. a portable/manual
// install, or the launcher itself not being installed at all).
inline void globPythonDirs(const std::string& root, std::vector<Candidate>& out) {
  std::string pattern = root + "\\Python*";
  WIN32_FIND_DATAA fd;
  HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
  if (h == INVALID_HANDLE_VALUE) return;
  do {
    if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
    std::string dirName = fd.cFileName;
    if (dirName == "." || dirName == "..") continue;
    std::string exe = root + "\\" + dirName + "\\python.exe";
    if (fileExists(exe)) {
      // "Python310" -> "3.10" for display only.
      std::string digits = dirName.substr(6); // strip "Python"
      std::string verLabel = digits.size() >= 2
        ? digits.substr(0, digits.size() - 1) + "." + digits.substr(digits.size() - 1)
        : digits;
      out.push_back({verLabel + "  -- " + exe, {exe}});
    }
  } while (FindNextFileA(h, &fd));
  FindClose(h);
}

#else

// Linux: scan every directory on $PATH for "python3*" executables, plus
// a couple of common fallback locations in case PATH is minimal.
inline bool isExecutable(const std::string& path) {
  return access(path.c_str(), X_OK) == 0;
}

inline void scanDirForPython(const std::string& dir, std::vector<Candidate>& out,
                              std::vector<std::string>& seen) {
  DIR* d = opendir(dir.c_str());
  if (!d) return;
  dirent* entry;
  while ((entry = readdir(d)) != nullptr) {
    std::string name = entry->d_name;
    if (name.rfind("python3", 0) != 0) continue; // must start with "python3"
    // Skip generic "python3"/"python" symlinks -- keep specific versions
    // like "python3.10", "python3.11" so the picker shows real choices.
    bool hasVersionSuffix = name.size() > 7 && name[7] == '.';
    if (!hasVersionSuffix) continue;
    std::string full = dir + "/" + name;
    if (!isExecutable(full)) continue;
    if (std::find(seen.begin(), seen.end(), name) != seen.end()) continue;
    seen.push_back(name);
    out.push_back({name + "  -- " + full, {full}});
  }
  closedir(d);
}

#endif

// Every Python interpreter this machine seems to have, deduplicated by
// resolved path. This is the full picker list -- nothing here is
// "preferred" or hidden; the user decides in choose().
inline std::vector<Candidate> listAll() {
  std::vector<Candidate> all;
#ifdef _WIN32
  all = fromPyLauncher();
  const char* localAppData = std::getenv("LOCALAPPDATA");
  globPythonDirs("C:", all);
  if (localAppData) globPythonDirs(std::string(localAppData) + "\\Programs\\Python", all);
#else
  std::vector<std::string> seen;
  const char* pathEnv = std::getenv("PATH");
  std::string pathStr = pathEnv ? pathEnv : "";
  std::stringstream ss(pathStr);
  std::string dir;
  while (std::getline(ss, dir, ':')) scanDirForPython(dir, all, seen);
  scanDirForPython("/usr/bin", all, seen);
  scanDirForPython("/usr/local/bin", all, seen);
#endif
  // Dedup by the resolved path (last element of cmd), case-insensitive on
  // Windows since paths there aren't case-sensitive.
  std::vector<Candidate> out;
  std::vector<std::string> seenPaths;
  for (auto& c : all) {
    std::string key = c.cmd.back();
#ifdef _WIN32
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);
#endif
    if (std::find(seenPaths.begin(), seenPaths.end(), key) != seenPaths.end()) continue;
    seenPaths.push_back(key);
    out.push_back(c);
  }
  return out;
}

// Lists every interpreter found and prompts the user to pick one by
// number. Returns {} if nothing was found or the user gave up.
inline std::vector<std::string> choose() {
  std::vector<Candidate> candidates = listAll();
  if (candidates.empty()) {
    std::cerr << "error: no Python interpreters found on this system "
                 "(checked the 'py' launcher, common install folders, and PATH).\n";
    return {};
  }
  std::cout << "Python interpreters found on this system:\n";
  for (size_t i = 0; i < candidates.size(); i++) {
    std::cout << "  [" << (i + 1) << "] " << candidates[i].label << "\n";
  }
  std::cout << "Select one to use for CLIENT GUI apps [1-" << candidates.size() << "]: ";
  std::cout.flush();
  std::string line;
  if (!std::getline(std::cin, line)) return {};
  int idx = 0;
  try { idx = std::stoi(line); } catch (...) { idx = 0; }
  if (idx < 1 || idx > (int)candidates.size()) {
    std::cerr << "Invalid selection.\n";
    return {};
  }
  return candidates[idx - 1].cmd;
}

} // namespace PythonFinder
