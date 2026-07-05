// dependency_installer.h -- makes esp32-ssh --app's dependency handling
// actually automatic: for every package an app's config.json declares in
// "requirements" (see app_config.h), check whether it's importable under
// the SAME interpreter about to launch the app, and `python -m pip
// install` it if not. Without this, esp32-app (PyQt5) would just crash on
// import the first time someone runs it without having manually
// pip-installed the GUI stack themselves.
//
// Deliberately uses "<python> -m pip install", never a bare "pip", so the
// install always lands in whichever interpreter/environment was actually
// resolved by python_finder.h -- not whatever "pip" happens to resolve to
// on PATH, which could be a totally different Python.
#pragma once
#include <string>
#include <vector>
#include <iostream>
#include <cstdlib>
#include <map>

namespace DependencyInstaller {

// Handful of pip-name -> import-name mismatches worth knowing about.
// Anything not listed here is assumed identical (true for PyQt5, numpy,
// requests, flask, etc.) -- good enough for the small, known set of
// requirements our own apps will ever declare.
inline std::string importNameFor(const std::string& pipName) {
  static const std::map<std::string, std::string> known = {
    {"pyserial", "serial"},
    {"Pillow", "PIL"},
    {"PyYAML", "yaml"},
    {"opencv-python", "cv2"},
    {"beautifulsoup4", "bs4"},
  };
  auto it = known.find(pipName);
  return it == known.end() ? pipName : it->second;
}

inline std::string quoteArg(const std::string& s) {
#ifdef _WIN32
  return "\"" + s + "\"";
#else
  return "'" + s + "'";
#endif
}

inline std::string joinCmd(const std::vector<std::string>& pythonCmd) {
  std::string out;
  for (auto& p : pythonCmd) out += quoteArg(p) + " ";
  return out;
}

// Windows' "cmd /c <command>" has a well-known quoting gotcha: when
// <command> contains MORE THAN ONE quoted segment (e.g. a quoted exe path
// AND a quoted "-c ..." argument, as every command built here does), cmd's
// quote-stripping heuristic corrupts the line -- it strips the very first
// and very last quote characters as if they were one wrapping pair,
// mangling everything in between (verified: this reliably breaks
// `"C:\...\python.exe" -c "import X"` and silently no-ops or errors).
// The standard fix is to wrap the ENTIRE command in one extra pair of
// quotes, which makes cmd strip only that outer layer and leave the real
// quoting inside untouched. Not needed on Linux (std::system there just
// hands the string to /bin/sh -c, no such heuristic).
inline std::string systemQuote(const std::string& fullCmd) {
#ifdef _WIN32
  return "\"" + fullCmd + "\"";
#else
  return fullCmd;
#endif
}

inline bool canImport(const std::vector<std::string>& pythonCmd, const std::string& modName) {
  // IMPORTANT: the outer systemQuote() wrap must go around the core
  // exe+args only. Redirection (">NUL 2>NUL") has to stay OUTSIDE that
  // wrap -- if it ends up inside the quotes too, cmd.exe treats ">NUL"
  // as a literal, quoted argument instead of a redirection operator,
  // which fails the whole invocation before Python even starts.
  std::string core = systemQuote(joinCmd(pythonCmd) + "-c \"import " + modName + "\"");
#ifdef _WIN32
  core += " >NUL 2>NUL";
#else
  core += " >/dev/null 2>&1";
#endif
  return std::system(core.c_str()) == 0;
}

// Ensures every declared requirement is importable, installing whatever's
// missing. pip's own output is left un-redirected so a slow/first-time
// install (e.g. PyQt5's wheel) shows real progress instead of a silent
// multi-second hang. Returns false only if something is still missing
// after attempting install -- caller should abort the launch in that case
// rather than let the app crash on its own import line.
inline bool ensureInstalled(const std::vector<std::string>& pythonCmd,
                             const std::vector<std::string>& requirements) {
  bool allOk = true;
  for (auto& req : requirements) {
    std::string modName = importNameFor(req);
    if (canImport(pythonCmd, modName)) continue;

    std::cout << "Installing missing dependency '" << req << "' ...\n";
    std::string cmd = joinCmd(pythonCmd) + "-m pip install " + quoteArg(req);
    int rc = std::system(systemQuote(cmd).c_str());
    if (rc != 0 || !canImport(pythonCmd, modName)) {
      std::cerr << "error: could not install/import required package '" << req
                << "'. Try installing it manually: " << joinCmd(pythonCmd)
                << "-m pip install " << req << "\n";
      allOk = false;
    } else {
      std::cout << "'" << req << "' installed.\n";
    }
  }
  return allOk;
}

} // namespace DependencyInstaller
