#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>

#include <cstring>
#include <string>

#include "core/constants.h"
#include "core/fsutil.h"

namespace adhan {

std::string win_appdata_root() {
  wchar_t wbuf[MAX_PATH];
  if (FAILED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, wbuf))) {
    return std::string();
  }
  int n = WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, 0, 0, 0, 0);
  std::string path(n ? n - 1 : 0, 0);
  if (n > 1) WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, &path[0], n, 0, 0);
  std::string root = join_path(path, kAppFolderName);
  mkdir_p(root);
  mkdir_p(join_path(root, "cache"));
  mkdir_p(join_path(root, "logs"));
  return root;
}

void win_enable_dpi_aware() {
  HMODULE user32 = GetModuleHandleW(L"user32.dll");
  if (!user32) return;
  typedef BOOL(WINAPI * SetProcessDPIAwareFn)(void);
  FARPROC proc = GetProcAddress(user32, "SetProcessDPIAware");
  if (proc) {
    SetProcessDPIAwareFn fn;
    memcpy(&fn, &proc, sizeof(fn));
    fn();
  }
}

}  // namespace adhan
