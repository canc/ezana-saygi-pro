#include "types.h"

#include <cmath>
#include <cstdio>
#include <ctime>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/time.h>
#endif

namespace adhan {

std::string CalendarDate::iso() const {
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", year, month, day);
  return buf;
}

bool Location::valid() const {
  if (city.empty() || timezone.empty()) return false;
  if (latitude < -90.0 || latitude > 90.0) return false;
  if (longitude < -180.0 || longitude > 180.0) return false;
  return true;
}

std::string Location::display_name() const {
  if (country.empty()) return city;
  return city + ", " + country;
}

std::string Location::id() const {
  return country + "/" + city;
}

const char* prayer_name(PrayerId id) {
  switch (id) {
    case PRAYER_FAJR:
      return "Fajr";
    case PRAYER_SUNRISE:
      return "Sunrise";
    case PRAYER_DHUHR:
      return "Dhuhr";
    case PRAYER_ASR:
      return "Asr";
    case PRAYER_MAGHRIB:
      return "Maghrib";
    case PRAYER_ISHA:
      return "Isha";
    default:
      return "Unknown";
  }
}

bool prayer_id_from_name(const std::string& name, PrayerId* out) {
  if (name == "Fajr") {
    *out = PRAYER_FAJR;
    return true;
  }
  if (name == "Sunrise") {
    *out = PRAYER_SUNRISE;
    return true;
  }
  if (name == "Dhuhr" || name == "Zuhr") {
    *out = PRAYER_DHUHR;
    return true;
  }
  if (name == "Asr") {
    *out = PRAYER_ASR;
    return true;
  }
  if (name == "Maghrib") {
    *out = PRAYER_MAGHRIB;
    return true;
  }
  if (name == "Isha") {
    *out = PRAYER_ISHA;
    return true;
  }
  return false;
}

bool PrayerSchedule::valid() const {
  if (version != kScheduleVersion && version != kCacheVersion) {
    if (version < 1) return false;
  }
  if (!cache_date_istanbul.valid() || !location.valid()) return false;
  // Required trigger prayers must be present and ordered.
  const PrayerId required[] = {PRAYER_FAJR, PRAYER_DHUHR, PRAYER_ASR, PRAYER_MAGHRIB, PRAYER_ISHA};
  int64_t prev = 0;
  for (int i = 0; i < 5; ++i) {
    const PrayerOccurrence& p = prayers[required[i]];
    if (!p.valid) return false;
    if (p.hour < 0 || p.hour > 23 || p.minute < 0 || p.minute > 59) return false;
    if (p.unix_utc <= 0) return false;
    if (prev != 0 && p.unix_utc <= prev) return false;
    prev = p.unix_utc;
  }
  return true;
}

const PrayerOccurrence* PrayerSchedule::get(PrayerId id) const {
  if (id < 0 || id >= PRAYER_COUNT) return 0;
  return &prayers[id];
}

const char* event_state_name(EventState s) {
  switch (s) {
    case ST_IDLE:
      return "IDLE";
    case ST_WAITING_FOR_THRESHOLD:
      return "WAITING_FOR_THRESHOLD";
    case ST_FADING_OUT:
      return "FADING_OUT";
    case ST_MUTED:
      return "MUTED";
    case ST_FADING_IN:
      return "FADING_IN";
    case ST_RESTORED:
      return "RESTORED";
    default:
      return "UNKNOWN";
  }
}

AppConfig default_config() {
  AppConfig c;
  c.version = kConfigVersion;
  c.enabled = true;
  c.country = kDefaultCountry;
  c.city = kDefaultCity;
  c.latitude = kDefaultLatitude;
  c.longitude = kDefaultLongitude;
  c.timezone = kDefaultTimezone;
  c.threshold_seconds = DEFAULT_THRESHOLD_SECONDS;
  c.adhan_duration_seconds = DEFAULT_ADHAN_DURATION_SECONDS;
  for (int i = 0; i < PRAYER_COUNT; ++i) c.adhan_durations[i] = 0;
  c.adhan_durations[PRAYER_FAJR] = DEFAULT_ADHAN_DURATION_FAJR_SECONDS;
  c.adhan_durations[PRAYER_DHUHR] = DEFAULT_ADHAN_DURATION_DHUHR_SECONDS;
  c.adhan_durations[PRAYER_ASR] = DEFAULT_ADHAN_DURATION_ASR_SECONDS;
  c.adhan_durations[PRAYER_MAGHRIB] = DEFAULT_ADHAN_DURATION_MAGHRIB_SECONDS;
  c.adhan_durations[PRAYER_ISHA] = DEFAULT_ADHAN_DURATION_ISHA_SECONDS;
  c.fade_duration_ms = DEFAULT_FADE_DURATION_MS;
  c.aladhan_endpoint = kDefaultAladhanEndpoint;
  c.islamicfinder_endpoint = kDefaultIslamicFinderEndpoint;
  c.http_timeout_ms = kHttpTimeoutMs;
  return c;
}

int default_adhan_duration_seconds(PrayerId id) {
  switch (id) {
    case PRAYER_FAJR:
      return DEFAULT_ADHAN_DURATION_FAJR_SECONDS;
    case PRAYER_DHUHR:
      return DEFAULT_ADHAN_DURATION_DHUHR_SECONDS;
    case PRAYER_ASR:
      return DEFAULT_ADHAN_DURATION_ASR_SECONDS;
    case PRAYER_MAGHRIB:
      return DEFAULT_ADHAN_DURATION_MAGHRIB_SECONDS;
    case PRAYER_ISHA:
      return DEFAULT_ADHAN_DURATION_ISHA_SECONDS;
    default:
      return DEFAULT_ADHAN_DURATION_SECONDS;
  }
}

bool valid_adhan_duration_seconds(int seconds) {
  if (seconds < kAdhanDurationMinSeconds || seconds > kAdhanDurationMaxSeconds) return false;
  return (seconds % 60) == 0;
}

int clamp_adhan_duration_seconds(int seconds) {
  int minutes = seconds / 60;
  if (seconds <= 0 || minutes < kAdhanDurationMinMinutes) return kAdhanDurationMinSeconds;
  if (minutes > kAdhanDurationMaxMinutes) return kAdhanDurationMaxSeconds;
  return minutes * 60;
}

int AppConfig::adhan_duration_for(PrayerId id) const {
  if (id < 0 || id >= PRAYER_COUNT) return default_adhan_duration_seconds(PRAYER_MAGHRIB);
  int v = adhan_durations[id];
  if (!valid_adhan_duration_seconds(v)) return default_adhan_duration_seconds(id);
  return v;
}

Location AppConfig::location() const {
  Location loc;
  loc.country = country;
  loc.city = city;
  loc.timezone = timezone;
  loc.latitude = latitude;
  loc.longitude = longitude;
  return loc;
}

int64_t SystemClock::now_unix() {
  return now_ms() / 1000;
}

int64_t SystemClock::now_ms() {
#ifdef _WIN32
  FILETIME ft;
  GetSystemTimeAsFileTime(&ft);
  ULARGE_INTEGER uli;
  uli.LowPart = ft.dwLowDateTime;
  uli.HighPart = ft.dwHighDateTime;
  return static_cast<int64_t>(uli.QuadPart / 10000ULL - 11644473600000ULL);
#else
  struct timeval tv;
  gettimeofday(&tv, 0);
  return static_cast<int64_t>(tv.tv_sec) * 1000 + tv.tv_usec / 1000;
#endif
}

}  // namespace adhan
