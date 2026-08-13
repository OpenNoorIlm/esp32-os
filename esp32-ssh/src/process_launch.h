// process_launch.h -- spawns the chosen Python interpreter to run a
// CLIENT GUI app's main.py, with PYTHONPATH pointed at esp32-ssh's
// packages/ folder (so "import lua" / "import device" / "import oled"
// resolve) and the working directory set to the app's own folder (so the
// app can load its own local assets/config.json by relative path).
//
// Blocks until the GUI app exits and returns its exit code -- this is a
// foreground launch (the person is sitting at the GUI), not a background
// service.
#pragma once
#include <string>
#include <vector>
#include <cstdlib>

#ifdef _WIN32
  #include <windows.h>
#else
  #include <unistd.h>
  #include <sys/wait.h>
#endif

namespace ProcessLaunch {

inline void setPythonPath(const std::vector<std::string>& packageDirs) {
  std::string joined;
#ifdef _WIN32
  const char sep = ';';
#else
  const char sep = ':';
#endif
  for (size_t i = 0; i < packageDirs.size(); i++) {
    if (i) joined += sep;
    joined += packageDirs[i];
  }
  const char* existing = std::getenv("PYTHONPATH");
  if (existing && *existing) joined += sep + std::string(existing);
#ifdef _WIN32
  _putenv_s("PYTHONPATH", joined.c_str());
#else
  setenv("PYTHONPATH", joined.c_str(), 1);
#endif
}

// pythonCmd: e.g. {"py", "-3.10"} or {"/usr/bin/python3.10"}
// scriptPath: full path to main.py
// cwd: the app's own folder (script runs as if launched from there)
// packageDirs: each individual package folder (packages/lua, packages/device,
//              packages/oled, ...) -- see setPythonPath's comment for why
//              these are added individually rather than as one parent dir.
inline int run(const std::vector<std::string>& pythonCmd,
                const std::string& scriptPath,
                const std::string& cwd,
                const std::vector<std::string>& packageDirs) {
  setPythonPath(packageDirs);

#ifdef _WIN32
  std::string cmdLine;
  for (auto& part : pythonCmd) cmdLine += "\"" + part + "\" ";
  cmdLine += "\"" + scriptPath + "\"";

  STARTUPINFOA si{};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi{};
  std::vector<char> mutableCmd(cmdLine.begin(), cmdLine.end());
  mutableCmd.push_back('\0');

  BOOL ok = CreateProcessA(
      nullptr, mutableCmd.data(), nullptr, nullptr, FALSE, 0,
      nullptr, cwd.c_str(), &si, &pi);
  if (!ok) return -1;
  WaitForSingleObject(pi.hProcess, INFINITE);
  DWORD exitCode = 0;
  GetExitCodeProcess(pi.hProcess, &exitCode);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
  return (int)exitCode;
#else
  pid_t pid = fork();
  if (pid < 0) return -1;
  if (pid == 0) {
    if (chdir(cwd.c_str()) != 0) { _exit(127); }
    std::vector<char*> argv;
    for (auto& part : pythonCmd) argv.push_back(const_cast<char*>(part.c_str()));
    std::string script = scriptPath;
    argv.push_back(const_cast<char*>(script.c_str()));
    argv.push_back(nullptr);
    execvp(argv[0], argv.data());
    _exit(127); // execvp only returns on failure
  }
  int status = 0;
  waitpid(pid, &status, 0);
  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
}

} // namespace ProcessLaunch
