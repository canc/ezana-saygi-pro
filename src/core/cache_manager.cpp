#include "cache_manager.h"

#include <cstdio>

#include "fsutil.h"
#include "schedule.h"
#include "timezone.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace adhan {
namespace {

std::string sanitize_token(const std::string& s) {
  std::string out;
  for (size_t i = 0; i < s.size(); ++i) {
    char c = s[i];
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' ||
        c == '-') {
      out.push_back(c);
    } else {
      out.push_back('-');
    }
  }
  return out;
}

void default_sleep(int ms) {
#ifdef _WIN32
  Sleep(ms);
#else
  (void)ms;
  // Tests should inject a no-op sleeper. Production Linux not used.
#endif
}

}  // namespace

CacheManager::CacheManager(std::string root_dir, PrayerTimeProvider* provider, Logger* logger)
    : root_(root_dir),
      provider_(provider),
      log_(logger),
      clock_(0),
      has_memory_(false),
      next_0310_(0),
      fetch_in_progress_(false),
      max_retries_(kHttpMaxRetries),
      sleeper_(0),
      api_attempts_(0) {
  last_0310_date_.year = 0;
  last_0310_date_.month = 0;
  last_0310_date_.day = 0;
  cache_dir_ = join_path(root_, "cache");
  mkdir_p(cache_dir_);
}

std::string CacheManager::make_cache_key(const Location& loc, const CalendarDate& istanbul_date) {
  char buf[192];
  std::snprintf(buf, sizeof(buf), "%s:%.4f:%.4f:%s", loc.timezone.c_str(),
                normalize_coord(loc.latitude), normalize_coord(loc.longitude),
                istanbul_date.iso().c_str());
  return buf;
}

std::string CacheManager::make_cache_filename(const Location& loc,
                                              const CalendarDate& istanbul_date) {
  char buf[192];
  std::snprintf(buf, sizeof(buf), "%s_%.4f_%.4f_%s.json", sanitize_token(loc.timezone).c_str(),
                normalize_coord(loc.latitude), normalize_coord(loc.longitude),
                istanbul_date.iso().c_str());
  return buf;
}

void CacheManager::log_info(const std::string& m) {
  if (log_) log_->info(m);
}

bool CacheManager::matches(const PrayerSchedule& s, const Location& loc,
                           const CalendarDate& date) const {
  if (!s.valid()) return false;
  if (s.cache_date_istanbul != date) return false;
  if (s.location.timezone != loc.timezone) return false;
  if (normalize_coord(s.location.latitude) != normalize_coord(loc.latitude)) return false;
  if (normalize_coord(s.location.longitude) != normalize_coord(loc.longitude)) return false;
  return true;
}

bool CacheManager::peek_today(const Location& loc, int64_t now_unix, PrayerSchedule* out) {
  CalendarDate today = istanbul_date(now_unix);
  if (next_0310_ == 0) next_0310_ = next_istanbul_0310(now_unix);
  if (!load_key(loc, today, out)) return false;
  memory_ = *out;
  has_memory_ = true;
  return true;
}

bool CacheManager::load_key(const Location& loc, const CalendarDate& date, PrayerSchedule* out) {
  std::string path = join_path(cache_dir_, make_cache_filename(loc, date));
  std::string text;
  if (!read_file(path, &text)) return false;
  PrayerSchedule s;
  std::string err;
  if (!schedule_from_json(text, &s, &err)) {
    if (log_) log_->warn(std::string("Invalid cache file discarded: ") + err);
    return false;
  }
  if (!matches(s, loc, date)) return false;
  *out = s;
  return true;
}

bool CacheManager::save_key(const PrayerSchedule& s) {
  std::string path = join_path(cache_dir_, make_cache_filename(s.location, s.cache_date_istanbul));
  return write_file_atomic(path, schedule_to_json(s));
}

bool CacheManager::fetch_from_api(const Location& loc, const CalendarDate& date,
                                  PrayerSchedule* out, std::string* err) {
  if (!provider_) {
    if (err) *err = "no provider";
    return false;
  }
  if (fetch_in_progress_) {
    if (err) *err = "fetch already in progress";
    return false;
  }
  fetch_in_progress_ = true;
  bool ok = false;
  std::string last_err;
  int timeout = kHttpTimeoutMs;
  for (int attempt = 0; attempt < max_retries_; ++attempt) {
    ++api_attempts_;
    log_info("Requesting prayer schedule from API");
    PrayerSchedule s;
    std::string e;
    if (provider_->fetch_daily(loc, date, timeout, &s, &e)) {
      if (!s.valid() || !matches(s, loc, date)) {
        last_err = "API response failed validation";
        log_info("Prayer schedule validation failed");
      } else {
        log_info("Prayer schedule validated");
        if (save_key(s)) log_info("Prayer schedule cached");
        *out = s;
        ok = true;
        break;
      }
    } else {
      last_err = e.empty() ? "API request failed" : e;
      if (log_) log_->error(std::string("API failure: ") + last_err);
    }
    if (!ok && attempt + 1 < max_retries_) {
      int delay = 2000 * (attempt + 1);
      if (sleeper_)
        sleeper_(delay);
      else
        default_sleep(delay);
    }
  }
  fetch_in_progress_ = false;
  if (!ok && err) *err = last_err;
  return ok;
}

CacheEnsureResult CacheManager::load_or_fetch(const Location& loc, int64_t now_unix,
                                              bool force_network, const char* reason) {
  CacheEnsureResult r;
  r.have_schedule = false;
  r.did_api_request = false;
  r.used_cache = false;

  CalendarDate today = istanbul_date(now_unix);
  log_info(std::string("Current prayer timezone: ") + kAuthoritativeTimezone);
  log_info(std::string("Current prayer date: ") + today.iso());
  std::string key = make_cache_key(loc, today);
  log_info(std::string("Checking cache: ") + key);

  if (next_0310_ == 0) next_0310_ = next_istanbul_0310(now_unix);

  PrayerSchedule cached;
  bool have_cache = load_key(loc, today, &cached);
  if (have_cache && !force_network) {
    log_info("Daily prayer cache found");
    log_info("Using cached prayer schedule");
    memory_ = cached;
    has_memory_ = true;
    r.have_schedule = true;
    r.used_cache = true;
    r.schedule = cached;
    return r;
  }
  if (!have_cache) log_info("Prayer cache missing");
  if (force_network) log_info(std::string("Manual/forced refresh (") + reason + ")");

  PrayerSchedule fetched;
  std::string err;
  r.did_api_request = true;
  if (fetch_from_api(loc, today, &fetched, &err)) {
    memory_ = fetched;
    has_memory_ = true;
    r.have_schedule = true;
    r.schedule = fetched;
    return r;
  }
  r.error = err;
  if (have_cache) {
    log_info("API failed; keeping last valid cache");
    memory_ = cached;
    has_memory_ = true;
    r.have_schedule = true;
    r.used_cache = true;
    r.schedule = cached;
    r.did_api_request = true;
    return r;
  }
  log_info("No valid cache and API unavailable; no prayer schedule");
  return r;
}

CacheEnsureResult CacheManager::ensure_today(const Location& loc, int64_t now_unix,
                                             bool force_network) {
  if (next_0310_ == 0) next_0310_ = next_istanbul_0310(now_unix);
  return load_or_fetch(loc, now_unix, force_network, force_network ? "manual" : "ensure");
}

CacheEnsureResult CacheManager::on_tick(const Location& loc, int64_t now_unix) {
  CacheEnsureResult r;
  r.have_schedule = has_memory_;
  r.did_api_request = false;
  r.used_cache = has_memory_;
  if (has_memory_) r.schedule = memory_;

  if (next_0310_ == 0) next_0310_ = next_istanbul_0310(now_unix);
  if (now_unix < next_0310_) return r;

  CalendarDate today = istanbul_date(now_unix);
  if (last_0310_date_ == today) {
    next_0310_ = next_istanbul_0310(now_unix);
    return r;
  }

  log_info("Daily cache check triggered at 03:10 Europe/Istanbul");
  last_0310_date_ = today;
  PrayerSchedule cached;
  if (load_key(loc, today, &cached)) {
    log_info("Today's prayer cache already exists");
    log_info("No API request required");
    memory_ = cached;
    has_memory_ = true;
    r.have_schedule = true;
    r.used_cache = true;
    r.schedule = cached;
  } else {
    log_info("Today's prayer cache is missing");
    r = load_or_fetch(loc, now_unix, false, "03:10");
  }
  next_0310_ = next_istanbul_0310(now_unix);
  return r;
}

CacheEnsureResult CacheManager::on_resume(const Location& loc, int64_t now_unix) {
  next_0310_ = next_istanbul_0310(now_unix);
  return load_or_fetch(loc, now_unix, false, "resume");
}

void CacheManager::cleanup(int64_t now_unix) {
  CalendarDate today = istanbul_date(now_unix);
  int64_t cutoff = istanbul_local_to_unix(today, 0, 0, 0) - kCacheRetentionDays * 86400LL;
  std::vector<std::string> files = list_files(cache_dir_);
  for (size_t i = 0; i < files.size(); ++i) {
    const std::string& name = files[i];
    // Date is the last 10 chars before .json: YYYY-MM-DD.json
    if (name.size() < 15) continue;
    std::string iso = name.substr(name.size() - 15, 10);
    int y = 0, m = 0, d = 0;
    if (std::sscanf(iso.c_str(), "%d-%d-%d", &y, &m, &d) != 3) continue;
    CalendarDate cd;
    cd.year = y;
    cd.month = m;
    cd.day = d;
    if (!cd.valid()) continue;
    int64_t file_day = istanbul_local_to_unix(cd, 0, 0, 0);
    if (file_day < cutoff) {
      remove_file(join_path(cache_dir_, name));
    }
  }
}

}  // namespace adhan
