#pragma once

#include <cstdint>
#include <string>

#include "constants.h"

namespace adhan {

struct CalendarDate {
  int year;
  int month;
  int day;

  bool valid() const { return year >= 1970 && month >= 1 && month <= 12 && day >= 1 && day <= 31; }
  bool operator==(const CalendarDate& o) const {
    return year == o.year && month == o.month && day == o.day;
  }
  bool operator!=(const CalendarDate& o) const { return !(*this == o); }
  std::string iso() const;
};

struct Location {
  std::string country;
  std::string city;
  std::string timezone;
  double latitude;
  double longitude;
  int islamicfinder_city_id = 0;

  bool valid() const;
  std::string display_name() const;
  std::string id() const;
};

enum PrayerId {
  PRAYER_FAJR = 0,
  PRAYER_SUNRISE = 1,
  PRAYER_DHUHR = 2,
  PRAYER_ASR = 3,
  PRAYER_MAGHRIB = 4,
  PRAYER_ISHA = 5,
  PRAYER_COUNT = 6
};

inline bool prayer_triggers_volume(PrayerId id) {
  return id == PRAYER_FAJR || id == PRAYER_DHUHR || id == PRAYER_ASR || id == PRAYER_MAGHRIB ||
         id == PRAYER_ISHA;
}

const char* prayer_name(PrayerId id);
bool prayer_id_from_name(const std::string& name, PrayerId* out);

struct PrayerOccurrence {
  PrayerId id;
  bool valid;
  int hour;
  int minute;
  int64_t unix_utc;
  PrayerOccurrence() : id(PRAYER_FAJR), valid(false), hour(0), minute(0), unix_utc(0) {}
};

struct PrayerSchedule {
  int version;
  CalendarDate cache_date_istanbul;
  CalendarDate local_date;
  Location location;
  int64_t fetched_at_unix;
  std::string source;
  int provider_config_version;
  int calculation_method;
  PrayerOccurrence prayers[PRAYER_COUNT];

  PrayerSchedule()
      : version(0), fetched_at_unix(0), provider_config_version(0), calculation_method(0) {
    cache_date_istanbul.year = cache_date_istanbul.month = cache_date_istanbul.day = 0;
    local_date.year = local_date.month = local_date.day = 0;
    for (int i = 0; i < PRAYER_COUNT; ++i) {
      prayers[i].id = static_cast<PrayerId>(i);
    }
  }

  bool valid() const;
  const PrayerOccurrence* get(PrayerId id) const;
};

enum EventState {
  ST_IDLE = 0,
  ST_WAITING_FOR_THRESHOLD,
  ST_FADING_OUT,
  ST_MUTED,
  ST_FADING_IN,
  ST_RESTORED
};

const char* event_state_name(EventState s);

struct PrayerEvent {
  std::string id;
  PrayerId prayer;
  int64_t prayer_unix;
  int threshold_seconds;
  int adhan_duration_seconds;
  int fade_duration_ms;
  float original_volume;
  bool original_mute;
  bool captured;
  EventState state;
  int64_t fade_out_start_ms;
  int64_t fade_out_end_ms;
  int64_t fade_in_start_ms;
  int64_t fade_in_end_ms;
};

struct AppConfig {
  int version;
  bool enabled;
  std::string country;
  std::string city;
  double latitude;
  double longitude;
  std::string timezone;
  int threshold_seconds;
  int adhan_duration_seconds;  // legacy global; not used for scheduling after migration
  int adhan_durations[PRAYER_COUNT];
  int fade_duration_ms;
  std::string aladhan_endpoint;
  std::string islamicfinder_endpoint;
  int islamicfinder_city_id = 0;
  int http_timeout_ms;

  Location location() const;
  int adhan_duration_for(PrayerId id) const;
};

int default_adhan_duration_seconds(PrayerId id);
bool valid_adhan_duration_seconds(int seconds);
int clamp_adhan_duration_seconds(int seconds);

AppConfig default_config();
// Empty, retired /index.php/api/prayer_times, and .us hosts are not used as a JSON API.
std::string canonical_islamicfinder_endpoint(const std::string& endpoint);
bool islamicfinder_json_api_configured(const std::string& endpoint);
// Historical /v1/timings coordinate endpoint is rewritten to timingsByCity.
std::string canonical_aladhan_endpoint(const std::string& endpoint);

struct HttpResult {
  bool ok;
  int status;
  std::string body;
  std::string error;
};

class HttpClient {
 public:
  virtual ~HttpClient() {}
  virtual HttpResult get(const std::string& url, int timeout_ms) = 0;
};

class VolumeController {
 public:
  virtual ~VolumeController() {}
  virtual bool get_master_volume(float* volume) = 0;
  virtual bool set_master_volume(float volume) = 0;
  virtual bool get_mute(bool* muted) = 0;
  virtual bool set_mute(bool muted) {
    (void)muted;
    return false;
  }
  // Re-resolve the current default render endpoint (device hotplug / default change).
  virtual void refresh_endpoint() {}
  virtual const char* backend_name() const { return "unknown"; }
  virtual const char* last_error() const { return ""; }
};

class Clock {
 public:
  virtual ~Clock() {}
  virtual int64_t now_unix() = 0;
  virtual int64_t now_ms() { return now_unix() * 1000; }
};

class SystemClock : public Clock {
 public:
  int64_t now_unix() override;
  int64_t now_ms() override;
};

}  // namespace adhan
