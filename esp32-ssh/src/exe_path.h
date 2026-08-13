// exe_path.h -- resolves the directory the running esp32-ssh binary lives
// in, so --app can find packages/ relative to the EXE, not the caller's
// current working directory (which could be anywhere).
#pragma once
#include <string>
#include <vector>

#ifdef _WIN32
  #include <windows.h>
#else
  #include <unistd.h>
  #include <limits.h>
  #include <dirent.h>
#endif

namespace ExePath {

inline std::string exeDir() {
#ifdef _WIN32
  char buf[MAX_PATH];
  DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
  std::string full(buf, n);
  size_t pos = full.find_last_of("\\/");
  return pos == std::string::npos ? "." : full.substr(0, pos);
#else
  char buf[PATH_MAX];
  ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
  if (n <= 0) return ".";
  std::string full(buf, n);
  size_t pos = full.find_last_of('/');
  return pos == std::string::npos ? "." : full.substr(0, pos);
#endif
}

// Lists immediate subdirectory names of `dir` (not full paths, just names)
// -- e.g. for packages/ containing lua/, device/, oled/, returns those
// three names. Used to add each package's own folder to PYTHONPATH
// individually, since these are bare-module packages (lua.py directly
// inside lua/, no __init__.py), not namespace packages.
inline std::vector<std::string> listSubdirs(const std::string& dir) {
  std::vector<std::string> out;
#ifdef _WIN32
  WIN32_FIND_DATAA fd;
  HANDLE h = FindFirstFileA((dir + "\\*").c_str(), &fd);
  if (h == INVALID_HANDLE_VALUE) return out;
  do {
    std::string name = fd.cFileName;
    if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
        name != "." && name != "..") {
      out.push_back(name);
    }
  } while (FindNextFileA(h, &fd));
  FindClose(h);
#else
  DIR* d = opendir(dir.c_str());
  if (!d) return out;
  struct dirent* entry;
  while ((entry = readdir(d)) != nullptr) {
    std::string name = entry->d_name;
    if (name == "." || name == "..") continue;
    if (entry->d_type == DT_DIR) out.push_back(name);
  }
  closedir(d);
#endif
  return out;
}

} // namespace ExePath
