#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <winnls.h>
#include <objbase.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "core/cache_manager.h"
#include "core/config.h"
#include "core/constants.h"
#include "core/fsutil.h"
#include "core/i18n.h"
#include "core/locations.h"
#include "core/logger.h"
#include "core/provider.h"
#include "core/scheduler.h"
#include "core/timezone.h"
#include "resource.h"
#include "win_platform.h"

using namespace adhan;

namespace {

std::wstring to_wide(const std::string& s) {
  if (s.empty()) return std::wstring();
  int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, 0, 0);
  std::wstring w(n ? n - 1 : 0, 0);
  if (n > 1) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
  return w;
}

std::string to_utf8(const std::wstring& w) {
  if (w.empty()) return std::string();
  int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, 0, 0, 0, 0);
  std::string s(n ? n - 1 : 0, 0);
  if (n > 1) WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], n, 0, 0);
  return s;
}

void set_text(HWND h, int id, const std::string& s) {
  SetDlgItemTextW(h, id, to_wide(s).c_str());
}

void set_text_w(HWND h, int id, const wchar_t* s) { SetDlgItemTextW(h, id, s ? s : L""); }

void set_text_w(HWND h, int id, const std::wstring& s) { SetDlgItemTextW(h, id, s.c_str()); }

HFONT g_font = 0;

HWND make_label(HWND parent, int id, const wchar_t* text, int x, int y, int w, int h, bool bold) {
  HWND hwnd = CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE, x, y, w, h, parent,
                              (HMENU)(INT_PTR)id, 0, 0);
  SendMessageW(hwnd, WM_SETFONT, (WPARAM)g_font, TRUE);
  (void)bold;
  return hwnd;
}

HWND make_combo(HWND parent, int id, int x, int y, int w, int h) {
  HWND hwnd = CreateWindowExW(WS_EX_CLIENTEDGE, L"COMBOBOX", L"",
                              WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL | CBS_HASSTRINGS,
                              x, y, w, h, parent, (HMENU)(INT_PTR)id, 0, 0);
  SendMessageW(hwnd, WM_SETFONT, (WPARAM)g_font, TRUE);
  return hwnd;
}

HWND make_btn(HWND parent, int id, const wchar_t* text, int x, int y, int w, int h) {
  HWND hwnd = CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, x, y, w, h,
                              parent, (HMENU)(INT_PTR)id, 0, 0);
  SendMessageW(hwnd, WM_SETFONT, (WPARAM)g_font, TRUE);
  return hwnd;
}

struct FetchResultMsg {
  CacheEnsureResult result;
  bool force;
};

struct App {
  HWND hwnd;
  HINSTANCE inst;
  NOTIFYICONDATAW nid;
  AppConfig cfg;
  Logger* log;
  HttpClient* http;
  VolumeController* vol;
  AladhanProvider* aladhan;
  IslamicFinderProvider* ifinder;
  FallbackProvider* provider;
  CacheManager* cache;
  Scheduler* sched;
  std::string root;
  bool fetching;
  bool exiting;
  bool shown_schedule_balloon;
  HANDLE fetch_handle;
  std::vector<const CityInfo*> cities;
};

App* g_app = 0;

void balloon(App* a, const wchar_t* title, const wchar_t* msg) {
  if (!a) return;
  a->nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_INFO;
  lstrcpynW(a->nid.szInfoTitle, title, ARRAYSIZE(a->nid.szInfoTitle));
  lstrcpynW(a->nid.szInfo, msg, ARRAYSIZE(a->nid.szInfo));
  a->nid.dwInfoFlags = NIIF_INFO;
  Shell_NotifyIconW(NIM_MODIFY, &a->nid);
  a->nid.szInfo[0] = 0;
  a->nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
}

void update_tray_tip(App* a) {
  const wchar_t* next = 0;
  PrayerId id;
  if (a->sched) {
    const SchedulerStatus& st = a->sched->status();
    if (!st.next_prayer_name.empty() && prayer_id_from_name(st.next_prayer_name, &id)) {
      next = ui::prayer(id);
    }
  }
  std::wstring tip = ui::tray_tip(next, a->cfg.enabled);
  lstrcpynW(a->nid.szTip, tip.c_str(), ARRAYSIZE(a->nid.szTip));
  a->nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
  Shell_NotifyIconW(NIM_MODIFY, &a->nid);
}

void apply_schedule(App* a, const PrayerSchedule& s, bool valid, int64_t now_ms) {
  a->sched->set_config(a->cfg);
  a->sched->set_schedule(s, valid);
  a->sched->evaluate(now_ms);
}

void refresh_ui(App* a);
void update_fade_timers(App* a);

void start_fetch(App* a, bool force);

  DWORD WINAPI fetch_thread(LPVOID param) {
  FetchResultMsg* wrap = static_cast<FetchResultMsg*>(param);
  App* a = g_app;
  bool force = wrap->force;
  delete wrap;
  if (!a || a->exiting) return 0;
  int64_t now = SystemClock().now_unix();
  CacheEnsureResult r = a->cache->ensure_today(a->cfg.location(), now, force);
  if (!a || a->exiting || !IsWindow(a->hwnd)) return 0;
  FetchResultMsg* out = new FetchResultMsg();
  out->result = r;
  out->force = force;
  PostMessageW(a->hwnd, WM_FETCH_DONE, 0, (LPARAM)out);
  return 0;
}

void start_fetch(App* a, bool force) {
  if (a->fetching) return;
  a->fetching = true;
  FetchResultMsg* msg = new FetchResultMsg();
  msg->force = force;
  if (a->fetch_handle) {
    CloseHandle(a->fetch_handle);
    a->fetch_handle = NULL;
  }
  a->fetch_handle = CreateThread(0, 0, fetch_thread, msg, 0, 0);
  if (!a->fetch_handle) {
    a->fetching = false;
    delete msg;
  }
}

std::string format_prayer_time(const App* a, int64_t unix_utc) {
  if (unix_utc <= 0) return "--:--";
  int off = timezone_offset_seconds(a->cfg.timezone, unix_utc);
  int y, m, d, h, mi, s;
  unix_to_civil_utc(unix_utc + off, &y, &m, &d, &h, &mi, &s);
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%02d:%02d", h, mi);
  return buf;
}

void fill_locations(App* a) {
  HWND combo = GetDlgItem(a->hwnd, IDC_LOCATION);
  SendMessageW(combo, CB_RESETCONTENT, 0, 0);
  a->cities.clear();
  size_t n = 0;
  const CityInfo* table = city_table(&n);
  int sel = 0;
  for (size_t i = 0; i < n; ++i) {
    a->cities.push_back(&table[i]);
    std::wstring label = ui::location_item(table[i].city, table[i].country);
    SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)label.c_str());
    if (a->cfg.city == table[i].city && a->cfg.country == table[i].country) sel = (int)i;
  }
  SendMessageW(combo, CB_SETCURSEL, sel, 0);
}

void fill_threshold(App* a) {
  HWND combo = GetDlgItem(a->hwnd, IDC_THRESHOLD);
  SendMessageW(combo, CB_RESETCONTENT, 0, 0);
  const wchar_t* labels[] = {ui::threshold_option(30), ui::threshold_option(60),
                             ui::threshold_option(180), ui::threshold_option(300)};
  int sel = 1;
  for (int i = 0; i < kThresholdOptionCount; ++i) {
    SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)labels[i]);
    if (a->cfg.threshold_seconds == kThresholdOptions[i]) sel = i;
  }
  SendMessageW(combo, CB_SETCURSEL, sel, 0);
}

void persist_config(App* a) { save_config(join_path(a->root, "config.json"), a->cfg); }

void on_location_sel(App* a) {
  HWND combo = GetDlgItem(a->hwnd, IDC_LOCATION);
  int sel = (int)SendMessageW(combo, CB_GETCURSEL, 0, 0);
  if (sel < 0 || sel >= (int)a->cities.size()) return;
  const CityInfo* c = a->cities[sel];
  if (a->cfg.city == c->city && a->cfg.country == c->country) return;
  int64_t now_ms = SystemClock().now_ms();
  a->sched->on_location_changing(now_ms);
  a->cfg.country = c->country;
  a->cfg.city = c->city;
  a->cfg.latitude = c->latitude;
  a->cfg.longitude = c->longitude;
  a->cfg.timezone = c->timezone;
  persist_config(a);
  if (a->log) {
    a->log->info(std::string("Location: ") + a->cfg.city + ", " + a->cfg.country);
    a->log->info(std::string("Timezone: ") + a->cfg.timezone);
  }
  int64_t now = SystemClock().now_unix();
  PrayerSchedule s;
  if (a->cache->peek_today(a->cfg.location(), now, &s)) {
    apply_schedule(a, s, true, now * 1000);
  } else {
    a->sched->set_schedule(PrayerSchedule(), false);
    start_fetch(a, false);
  }
  refresh_ui(a);
}

void on_threshold_sel(App* a) {
  HWND combo = GetDlgItem(a->hwnd, IDC_THRESHOLD);
  int sel = (int)SendMessageW(combo, CB_GETCURSEL, 0, 0);
  if (sel < 0 || sel >= kThresholdOptionCount) return;
  a->cfg.threshold_seconds = kThresholdOptions[sel];
  persist_config(a);
  a->sched->set_config(a->cfg);
  a->sched->evaluate(SystemClock().now_ms());
  refresh_ui(a);
}

void toggle_enabled(App* a) {
  a->cfg.enabled = !a->cfg.enabled;
  persist_config(a);
  a->sched->set_enabled(a->cfg.enabled, SystemClock().now_ms());
  if (a->log) a->log->info(a->cfg.enabled ? "Enabled" : "Disabled");
  refresh_ui(a);
  update_tray_tip(a);
}

void detect_location(App* a) {
  GEOID id = GetUserGeoID(GEOCLASS_NATION);
  if (id == GEOID_NOT_AVAILABLE) {
    MessageBoxW(a->hwnd, ui::err_geo_unavailable(), ui::app_title(), MB_OK | MB_ICONINFORMATION);
    return;
  }
  wchar_t iso[8];
  iso[0] = 0;
  GetGeoInfoW(id, GEO_ISO2, iso, 8, 0);
  std::string country = iso2_to_country(to_utf8(iso));
  if (country.empty()) {
    MessageBoxW(a->hwnd, ui::err_geo_unmapped(), ui::app_title(), MB_OK | MB_ICONINFORMATION);
    return;
  }
  std::vector<const CityInfo*> list = cities_in_country(country);
  if (list.empty()) {
    MessageBoxW(a->hwnd, ui::err_geo_no_cities(), ui::app_title(), MB_OK | MB_ICONINFORMATION);
    return;
  }
  const CityInfo* pick = list[0];
  if (country == a->cfg.country) {
    fill_locations(a);
    balloon(a, ui::app_title(), ui::note_geo_same_country());
    return;
  }
  // Prefer a well-known default city when switching country.
  for (size_t i = 0; i < list.size(); ++i) {
    if (country == "Turkey" && std::string(list[i]->city) == "Antalya") pick = list[i];
    if (country == "Germany" && std::string(list[i]->city) == "Berlin") pick = list[i];
    if (country == "United Kingdom" && std::string(list[i]->city) == "London") pick = list[i];
  }
  a->cfg.country = pick->country;
  a->cfg.city = pick->city;
  a->cfg.latitude = pick->latitude;
  a->cfg.longitude = pick->longitude;
  a->cfg.timezone = pick->timezone;
  persist_config(a);
  fill_locations(a);
  int64_t now_ms = SystemClock().now_ms();
  a->sched->on_location_changing(now_ms);
  int64_t now = now_ms / 1000;
  PrayerSchedule s;
  if (a->cache->peek_today(a->cfg.location(), now, &s)) {
    apply_schedule(a, s, true, now_ms);
  } else {
    a->sched->set_schedule(PrayerSchedule(), false);
    start_fetch(a, false);
  }
  refresh_ui(a);
}

void refresh_ui(App* a) {
  if (!a->hwnd) return;
  const SchedulerStatus& st = a->sched->status();
  std::wstring status = a->cfg.enabled ? ui::active() : ui::inactive();
  if (a->fetching) {
    status += L"  (";
    status += ui::status_updating();
    status += L")";
  } else if (!st.has_schedule) {
    status += L"  (";
    status += ui::status_no_schedule();
    status += L")";
  } else if (st.state == ST_FADING_OUT) {
    status += L"  (";
    status += ui::status_fading_out();
    status += L")";
  } else if (st.state == ST_MUTED) {
    status += L"  (";
    status += ui::status_muted();
    status += L")";
  } else if (st.state == ST_FADING_IN) {
    status += L"  (";
    status += ui::status_fading_in();
    status += L")";
  }
  std::wstring status_line = a->cfg.enabled ? L"\u25CF " : L"\u25CB ";
  status_line += status;
  set_text_w(a->hwnd, IDC_STATUS, status_line);

  int64_t now = SystemClock().now_unix();
  set_text_w(a->hwnd, IDC_TIMEZONE, ui::timezone_text(a->cfg.timezone, now));

  char coords[80];
  std::snprintf(coords, sizeof(coords), "%.4f, %.4f", a->cfg.latitude, a->cfg.longitude);
  set_text(a->hwnd, IDC_COORDS, coords);

  PrayerId pid;
  if (!st.next_prayer_name.empty() && prayer_id_from_name(st.next_prayer_name, &pid)) {
    set_text_w(a->hwnd, IDC_NEXT_NAME, ui::prayer(pid));
    set_text(a->hwnd, IDC_NEXT_TIME, format_prayer_time(a, st.next_prayer_unix));
  } else if (!st.has_schedule) {
    set_text_w(a->hwnd, IDC_NEXT_NAME, ui::status_no_schedule());
    set_text_w(a->hwnd, IDC_NEXT_TIME, L"");
  } else {
    set_text_w(a->hwnd, IDC_NEXT_NAME, ui::status_no_remaining());
    set_text_w(a->hwnd, IDC_NEXT_TIME, L"");
  }
  SetDlgItemTextW(a->hwnd, IDC_TOGGLE, a->cfg.enabled ? ui::active() : ui::inactive());
  update_tray_tip(a);
  update_fade_timers(a);
}

void update_fade_timers(App* a) {
  if (a->sched->is_fading()) {
    SetTimer(a->hwnd, IDT_FADE, kFadeTickMs, 0);
    KillTimer(a->hwnd, IDT_MUTE);
  } else if (a->sched->is_holding_mute()) {
    KillTimer(a->hwnd, IDT_FADE);
    SetTimer(a->hwnd, IDT_MUTE, kMuteHoldTickMs, 0);
  } else {
    KillTimer(a->hwnd, IDT_FADE);
    KillTimer(a->hwnd, IDT_MUTE);
  }
}

void tick_scheduler(App* a) {
  int64_t now = SystemClock().now_unix();
  int64_t now_ms = SystemClock().now_ms();
  CacheEnsureResult cr = a->cache->on_tick(a->cfg.location(), now);
  if (cr.did_api_request && cr.have_schedule) {
    apply_schedule(a, cr.schedule, true, now_ms);
  } else if (cr.have_schedule) {
    a->sched->set_schedule(cr.schedule, true);
  }
  a->sched->evaluate(now_ms);
  refresh_ui(a);
}

void show_window(App* a) {
  if (!a || !a->hwnd) return;
  if (IsIconic(a->hwnd))
    ShowWindow(a->hwnd, SW_RESTORE);
  else
    ShowWindow(a->hwnd, SW_SHOW);
  SetWindowPos(a->hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
  SetForegroundWindow(a->hwnd);
  SetActiveWindow(a->hwnd);
  SetFocus(a->hwnd);
}

void tray_menu(App* a) {
  POINT pt;
  GetCursorPos(&pt);
  HMENU m = CreatePopupMenu();
  AppendMenuW(m, MF_STRING, ID_TRAY_OPEN, ui::tray_show());
  AppendMenuW(m, MF_STRING, ID_TRAY_EXIT, ui::tray_exit());
  SetForegroundWindow(a->hwnd);
  TrackPopupMenu(m, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0, a->hwnd, 0);
  DestroyMenu(m);
}

void add_tray(App* a) {
  ZeroMemory(&a->nid, sizeof(a->nid));
  a->nid.cbSize = sizeof(NOTIFYICONDATAW);
  a->nid.hWnd = a->hwnd;
  a->nid.uID = 1;
  a->nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
  a->nid.uCallbackMessage = WM_TRAYICON;
  a->nid.hIcon = LoadIconW(a->inst, MAKEINTRESOURCEW(IDI_APPICON));
  if (!a->nid.hIcon) a->nid.hIcon = LoadIconW(0, IDI_APPLICATION);
  lstrcpynW(a->nid.szTip, ui::app_title(), ARRAYSIZE(a->nid.szTip));
  Shell_NotifyIconW(NIM_ADD, &a->nid);
  a->nid.uVersion = NOTIFYICON_VERSION;
  Shell_NotifyIconW(NIM_SETVERSION, &a->nid);
}

void remove_tray(App* a) { Shell_NotifyIconW(NIM_DELETE, &a->nid); }

void layout_window(HWND hwnd) {
  make_label(hwnd, 0, ui::app_title(), 20, 16, 400, 24, true);
  make_label(hwnd, 0, ui::status_label(), 20, 52, 120, 18, false);
  make_label(hwnd, IDC_STATUS, ui::active(), 20, 72, 400, 22, false);

  make_label(hwnd, 0, ui::location_label(), 20, 108, 200, 18, false);
  make_combo(hwnd, IDC_LOCATION, 20, 128, 300, 220);
  make_btn(hwnd, IDC_DETECT, ui::detect_location(), 330, 127, 120, 26);
  make_label(hwnd, IDC_COORDS, L"", 20, 160, 400, 18, false);

  make_label(hwnd, 0, ui::timezone_label(), 20, 188, 200, 18, false);
  make_label(hwnd, IDC_TIMEZONE, L"", 20, 208, 400, 20, false);

  make_label(hwnd, 0, ui::threshold_label(), 20, 240, 280, 18, false);
  make_combo(hwnd, IDC_THRESHOLD, 20, 260, 200, 120);

  make_label(hwnd, 0, ui::next_prayer_label(), 20, 300, 200, 18, false);
  make_label(hwnd, IDC_NEXT_NAME, ui::em_dash(), 20, 322, 400, 22, false);
  make_label(hwnd, IDC_NEXT_TIME, L"", 20, 346, 400, 22, false);

  make_btn(hwnd, IDC_TOGGLE, ui::active(), 20, 390, 120, 36);
  make_btn(hwnd, IDC_REFRESH, ui::refresh_times(), 150, 390, 160, 36);
  make_label(hwnd, IDC_HINT, ui::close_hint(), 20, 440, 430, 36, false);
}

void on_create(HWND hwnd, App* a) {
  a->hwnd = hwnd;
  layout_window(hwnd);
  fill_locations(a);
  fill_threshold(a);
  add_tray(a);
  SetTimer(hwnd, IDT_SCHEDULER, kSchedulerIdleIntervalMs, 0);

  int64_t now = SystemClock().now_unix();
  a->log->info("Application started");
  a->log->info(std::string("Location: ") + a->cfg.city + ", " + a->cfg.country);
  a->log->info(std::string("Timezone: ") + a->cfg.timezone);
  a->cache->cleanup(now);
  a->sched->set_config(a->cfg);
  a->sched->recover_on_startup(now * 1000);

  PrayerSchedule s;
  if (a->cache->peek_today(a->cfg.location(), now, &s)) {
    a->log->info("Daily prayer cache found");
    a->log->info("Using cached prayer schedule");
    apply_schedule(a, s, true, now * 1000);
  } else {
    a->log->info("Prayer cache missing");
    start_fetch(a, false);
  }
  refresh_ui(a);
}

void on_fetch_done(App* a, FetchResultMsg* msg) {
  a->fetching = false;
  if (!msg) return;
  if (msg->result.have_schedule) {
    apply_schedule(a, msg->result.schedule, true, SystemClock().now_ms());
    if (a->log) a->log->info("Prayer schedule updated");
    if (msg->force || (!msg->result.used_cache && msg->result.did_api_request)) {
      if (!a->shown_schedule_balloon || msg->force) {
        balloon(a, ui::app_title(), ui::note_schedule_updated());
        a->shown_schedule_balloon = true;
      }
    }
  } else {
    if (a->log) a->log->error(std::string("No schedule: ") + msg->result.error);
    set_text_w(a->hwnd, IDC_HINT, ui::err_schedule_unavailable());
  }
  delete msg;
  refresh_ui(a);
}

void exit_app(App* a) {
  a->exiting = true;
  if (a->fetch_handle) {
    WaitForSingleObject(a->fetch_handle, 20000);
    CloseHandle(a->fetch_handle);
    a->fetch_handle = NULL;
  }
  a->sched->set_enabled(false, SystemClock().now_ms());
  // Re-disable after restore but keep user's enabled preference in config.
  // set_enabled(false) would persist if we saved — we only want runtime restore.
  KillTimer(a->hwnd, IDT_SCHEDULER);
  KillTimer(a->hwnd, IDT_FADE);
  KillTimer(a->hwnd, IDT_MUTE);
  remove_tray(a);
  DestroyWindow(a->hwnd);
}

LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  App* a = g_app;
  switch (msg) {
    case WM_CREATE:
      on_create(hwnd, a);
      return 0;
    case WM_COMMAND: {
      int id = LOWORD(wParam);
      int code = HIWORD(wParam);
      if (id == IDC_LOCATION && code == CBN_SELCHANGE) on_location_sel(a);
      if (id == IDC_THRESHOLD && code == CBN_SELCHANGE) on_threshold_sel(a);
      if (id == IDC_TOGGLE) toggle_enabled(a);
      if (id == IDC_DETECT) detect_location(a);
      if (id == IDC_REFRESH) start_fetch(a, true);
      if (id == ID_TRAY_OPEN) show_window(a);
      if (id == ID_TRAY_EXIT) exit_app(a);
      return 0;
    }
    case WM_TIMER:
      if (wParam == IDT_SCHEDULER || wParam == IDT_FADE || wParam == IDT_MUTE) tick_scheduler(a);
      return 0;
    case WM_FETCH_DONE:
      on_fetch_done(a, reinterpret_cast<FetchResultMsg*>(lParam));
      return 0;
    case WM_SHOW_MAIN:
      show_window(a);
      return 0;
    case WM_TRAYICON:
      if (lParam == WM_LBUTTONDBLCLK) show_window(a);
      if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) tray_menu(a);
      return 0;
    case WM_CLOSE:
      ShowWindow(hwnd, SW_HIDE);
      return 0;
    case WM_SYSCOMMAND:
      if ((wParam & 0xFFF0) == SC_MINIMIZE || (wParam & 0xFFF0) == SC_CLOSE) {
        ShowWindow(hwnd, SW_HIDE);
        return 0;
      }
      break;
    case WM_POWERBROADCAST:
      if (wParam == PBT_APMRESUMEAUTOMATIC || wParam == PBT_APMRESUMESUSPEND ||
          wParam == PBT_APMRESUMECRITICAL) {
        PostMessageW(hwnd, WM_RESUME_WORK, 0, 0);
      }
      return TRUE;
    case WM_TIMECHANGE:
    case WM_RESUME_WORK: {
      int64_t now = SystemClock().now_unix();
      if (a->log) a->log->info("System time/resume: recalculating schedule");
      CacheEnsureResult r = a->cache->on_resume(a->cfg.location(), now);
      if (r.have_schedule) apply_schedule(a, r.schedule, true, now * 1000);
      else if (!a->fetching) start_fetch(a, false);
      a->sched->evaluate(now * 1000);
      refresh_ui(a);
      return 0;
    }
    case WM_CTLCOLORSTATIC: {
      HDC dc = (HDC)wParam;
      SetBkMode(dc, TRANSPARENT);
      int id = GetDlgCtrlID((HWND)lParam);
      if (id == IDC_STATUS && a && a->cfg.enabled) SetTextColor(dc, RGB(16, 120, 48));
      return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    }
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int run(HINSTANCE inst, int show) {
  INITCOMMONCONTROLSEX icc;
  icc.dwSize = sizeof(icc);
  icc.dwICC = ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES;
  InitCommonControlsEx(&icc);

  WNDCLASSEXW wc;
  ZeroMemory(&wc, sizeof(wc));
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = wnd_proc;
  wc.hInstance = inst;
  wc.hCursor = LoadCursor(0, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wc.lpszClassName = L"AdhanVolumeMainWnd";
  wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(IDI_APPICON));
  wc.hIconSm = wc.hIcon;
  RegisterClassExW(&wc);

  g_font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

  App app;
  g_app = &app;
  app.hwnd = 0;
  app.inst = inst;
  app.log = 0;
  app.http = 0;
  app.vol = 0;
  app.aladhan = 0;
  app.ifinder = 0;
  app.provider = 0;
  app.cache = 0;
  app.sched = 0;
  app.fetching = false;
  app.exiting = false;
  app.shown_schedule_balloon = false;
  app.fetch_handle = NULL;
  app.root = win_appdata_root();
  app.cfg = default_config();

  std::string err;
  if (!load_config(join_path(app.root, "config.json"), &app.cfg, &err)) {
    app.cfg = default_config();
  }
  persist_config(&app);

  app.log = new Logger(join_path(app.root, "logs"));
  app.http = create_win_http_client();
  app.vol = create_win_volume_controller();
  app.aladhan = new AladhanProvider(app.http, app.cfg.aladhan_endpoint);
  app.ifinder = new IslamicFinderProvider(app.http, app.cfg.islamicfinder_endpoint);
  app.provider = new FallbackProvider(app.aladhan, app.ifinder);
  app.cache = new CacheManager(app.root, app.provider, app.log);
  app.cache->set_max_retries(kHttpMaxRetries);
  app.sched = new Scheduler(app.vol, app.log, app.root);

  HWND hwnd = CreateWindowExW(0, L"AdhanVolumeMainWnd", ui::app_title(),
                              WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                              CW_USEDEFAULT, CW_USEDEFAULT, 480, 530, 0, 0, inst, 0);
  if (!hwnd) return 1;
  ShowWindow(hwnd, show);
  UpdateWindow(hwnd);

  MSG msg;
  while (GetMessageW(&msg, 0, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }

  delete app.sched;
  delete app.cache;
  delete app.provider;
  delete app.ifinder;
  delete app.aladhan;
  delete app.vol;
  delete app.http;
  delete app.log;
  g_app = 0;
  return (int)msg.wParam;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int show) {
  win_enable_dpi_aware();
  HANDLE mutex = CreateMutexW(NULL, TRUE, L"Local\\AdhanVolumeSingleton");
  if (mutex && GetLastError() == ERROR_ALREADY_EXISTS) {
    HWND existing = FindWindowW(L"AdhanVolumeMainWnd", 0);
    if (existing) {
      PostMessageW(existing, WM_SHOW_MAIN, 0, 0);
      AllowSetForegroundWindow(ASFW_ANY);
      ShowWindow(existing, SW_SHOW);
      SetForegroundWindow(existing);
    }
    return 0;
  }
  HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
  int rc = run(inst, show);
  if (SUCCEEDED(hr)) CoUninitialize();
  if (mutex) CloseHandle(mutex);
  return rc;
}
