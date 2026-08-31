#pragma once

#include <string>

#include "logger.h"
#include "provider.h"
#include "types.h"

namespace adhan {

struct CacheEnsureResult {
  bool have_schedule;
  bool did_api_request;
  bool used_cache;
  std::string error;
  PrayerSchedule schedule;
};

class CacheManager {
 public:
  CacheManager(std::string root_dir, PrayerTimeProvider* provider, Logger* logger);

  void set_max_retries(int n) { max_retries_ = n < 1 ? 1 : n; }
  void set_sleeper(void (*fn)(int)) { sleeper_ = fn; }
  void set_clock(Clock* clock) { clock_ = clock; }

  // Startup / location change / resume / manual refresh.
  // force_network: manual refresh (still keeps old cache if the request fails).
  CacheEnsureResult ensure_today(const Location& loc, int64_t now_unix, bool force_network);

  // Periodic tick. Performs the 03:10 Europe/Istanbul check when due.
  // Never used by the volume scheduler as an API poll.
  CacheEnsureResult on_tick(const Location& loc, int64_t now_unix);

  CacheEnsureResult on_resume(const Location& loc, int64_t now_unix);

  void cleanup(int64_t now_unix);

  int64_t next_0310_unix() const { return next_0310_; }
  bool has_schedule() const { return has_memory_; }
  const PrayerSchedule& schedule() const { return memory_; }

  static std::string make_cache_key(const Location& loc, const CalendarDate& istanbul_date);
  static std::string make_cache_filename(const Location& loc, const CalendarDate& istanbul_date);

  int api_attempts() const { return api_attempts_; }
  void reset_api_attempts() { api_attempts_ = 0; }

  // Disk only; never contacts the API.
  bool peek_today(const Location& loc, int64_t now_unix, PrayerSchedule* out);

 private:
  std::string root_;
  std::string cache_dir_;
  PrayerTimeProvider* provider_;
  Logger* log_;
  Clock* clock_;
  PrayerSchedule memory_;
  bool has_memory_;
  int64_t next_0310_;
  CalendarDate last_0310_date_;
  bool fetch_in_progress_;
  int max_retries_;
  void (*sleeper_)(int);
  int api_attempts_;

  bool load_key(const Location& loc, const CalendarDate& date, PrayerSchedule* out);
  bool save_key(const PrayerSchedule& s);
  bool matches(const PrayerSchedule& s, const Location& loc, const CalendarDate& date) const;
  CacheEnsureResult load_or_fetch(const Location& loc, int64_t now_unix, bool force_network,
                                  const char* reason);
  bool fetch_from_api(const Location& loc, const CalendarDate& date, PrayerSchedule* out,
                      std::string* err);
  void log_info(const std::string& m);
};

}  // namespace adhan
