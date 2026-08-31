#include "fsutil.h"

#include <cstdio>
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace adhan {

#ifdef _WIN32
static std::wstring to_wide(const std::string& s) {
  if (s.empty()) return std::wstring();
  int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, 0, 0);
  std::wstring w(n ? n - 1 : 0, 0);
  if (n > 1) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
  return w;
}
#endif

std::string join_path(const std::string& a, const std::string& b) {
  if (a.empty()) return b;
  char sep =
#ifdef _WIN32
      '\\';
#else
      '/';
#endif
  if (a[a.size() - 1] == '/' || a[a.size() - 1] == '\\') return a + b;
  return a + sep + b;
}

std::string parent_path(const std::string& path) {
  size_t pos = path.find_last_of("/\\");
  if (pos == std::string::npos) return "";
  return path.substr(0, pos);
}

bool read_file(const std::string& path, std::string* out) {
#ifdef _WIN32
  FILE* f = _wfopen(to_wide(path).c_str(), L"rb");
#else
  FILE* f = std::fopen(path.c_str(), "rb");
#endif
  if (!f) return false;
  std::string data;
  char buf[4096];
  size_t n;
  while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) data.append(buf, n);
  std::fclose(f);
  *out = data;
  return true;
}

bool write_file_atomic(const std::string& path, const std::string& data) {
  std::string tmp = path + ".tmp";
#ifdef _WIN32
  FILE* f = _wfopen(to_wide(tmp).c_str(), L"wb");
#else
  FILE* f = std::fopen(tmp.c_str(), "wb");
#endif
  if (!f) return false;
  size_t w = std::fwrite(data.data(), 1, data.size(), f);
  std::fclose(f);
  if (w != data.size()) {
    remove_file(tmp);
    return false;
  }
#ifdef _WIN32
  std::wstring wpath = to_wide(path);
  std::wstring wtmp = to_wide(tmp);
  MoveFileExW(wtmp.c_str(), wpath.c_str(), MOVEFILE_REPLACE_EXISTING);
  return file_exists(path);
#else
  if (rename(tmp.c_str(), path.c_str()) != 0) {
    remove_file(tmp);
    return false;
  }
  return true;
#endif
}

bool file_exists(const std::string& path) {
#ifdef _WIN32
  DWORD a = GetFileAttributesW(to_wide(path).c_str());
  return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) == 0;
#else
  struct stat st;
  return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
#endif
}

bool mkdir_p(const std::string& path) {
  if (path.empty()) return false;
#ifdef _WIN32
  std::wstring w = to_wide(path);
  for (size_t i = 0; i < w.size(); ++i) {
    if (w[i] == L'/' || w[i] == L'\\') {
      if (i == 0) continue;
      wchar_t c = w[i];
      w[i] = 0;
      CreateDirectoryW(w.c_str(), 0);
      w[i] = c;
    }
  }
  CreateDirectoryW(w.c_str(), 0);
  DWORD a = GetFileAttributesW(to_wide(path).c_str());
  return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
  std::string cur;
  for (size_t i = 0; i < path.size(); ++i) {
    if (path[i] == '/') {
      if (!cur.empty()) mkdir(cur.c_str(), 0755);
    }
    cur.push_back(path[i]);
  }
  if (!cur.empty()) mkdir(cur.c_str(), 0755);
  struct stat st;
  return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
#endif
}

bool remove_file(const std::string& path) {
#ifdef _WIN32
  return DeleteFileW(to_wide(path).c_str()) != 0;
#else
  return unlink(path.c_str()) == 0;
#endif
}

std::vector<std::string> list_files(const std::string& dir) {
  std::vector<std::string> out;
#ifdef _WIN32
  std::wstring pattern = to_wide(join_path(dir, "*"));
  WIN32_FIND_DATAW fd;
  HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
  if (h == INVALID_HANDLE_VALUE) return out;
  do {
    if (fd.cFileName[0] == L'.') continue;
    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
    int n = WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, 0, 0, 0, 0);
    std::string name(n ? n - 1 : 0, 0);
    if (n > 1) WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, &name[0], n, 0, 0);
    out.push_back(name);
  } while (FindNextFileW(h, &fd));
  FindClose(h);
#else
  DIR* d = opendir(dir.c_str());
  if (!d) return out;
  struct dirent* e;
  while ((e = readdir(d))) {
    if (e->d_name[0] == '.') continue;
    out.push_back(e->d_name);
  }
  closedir(d);
#endif
  return out;
}

}  // namespace adhan
