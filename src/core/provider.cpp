#include "provider.h"

#include <cstdio>
#include <sstream>

#include "json.h"
#include "schedule.h"
#include "timezone.h"

namespace adhan {

std::string url_encode(const std::string& s) {
  std::string out;
  for (size_t i = 0; i < s.size(); ++i) {
    unsigned char c = static_cast<unsigned char>(s[i]);
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' ||
        c == '_' || c == '.' || c == '~') {
      out.push_back(static_cast<char>(c));
    } else if (c == ' ') {
      out.push_back('+');
    } else {
      char buf[8];
      std::snprintf(buf, sizeof(buf), "%%%02X", c);
      out.append(buf);
    }
  }
  return out;
}

int calculation_method_for_location(const Location& loc) {
  if (loc.country == "Turkey" || loc.timezone == "Europe/Istanbul") return 13;  // Diyanet
  if (loc.country == "Saudi Arabia") return 4;
  if (loc.country == "Egypt") return 5;
  if (loc.country == "United States" || loc.country == "Canada") return 2;
  return 3;  // Muslim World League
}

static const Json* find_named(const Json& obj, const char* a, const char* b = 0) {
  if (obj.has(a) && !obj.get(a).is_null()) return &obj.get(a);
  if (b && obj.has(b) && !obj.get(b).is_null()) return &obj.get(b);
  return 0;
}

bool parse_prayer_timings_object(const Json& timings, PrayerSchedule* out, std::string* err) {
  struct Map {
    PrayerId id;
    const char* a;
    const char* b;
    bool required;
  };
  const Map maps[] = {
      {PRAYER_FAJR, "Fajr", "fajr", true},
      {PRAYER_SUNRISE, "Sunrise", "sunrise", false},
      {PRAYER_DHUHR, "Dhuhr", "Zuhr", true},
      {PRAYER_ASR, "Asr", "asr", true},
      {PRAYER_MAGHRIB, "Maghrib", "maghrib", true},
      {PRAYER_ISHA, "Isha", "isha", true},
  };
  for (size_t i = 0; i < sizeof(maps) / sizeof(maps[0]); ++i) {
    PrayerOccurrence& p = out->prayers[maps[i].id];
    p.id = maps[i].id;
    p.valid = false;
    const Json* v = find_named(timings, maps[i].a, maps[i].b);
    if (!v || !v->is_string()) {
      if (maps[i].required) {
        if (err) *err = std::string("missing prayer field: ") + maps[i].a;
        return false;
      }
      continue;
    }
    if (!parse_hhmm(v->as_string(), &p.hour, &p.minute)) {
      if (maps[i].required) {
        if (err) *err = std::string("invalid time for ") + maps[i].a;
        return false;
      }
      continue;
    }
    p.valid = true;
  }
  return true;
}

static bool finish_schedule(PrayerSchedule* s, const Location& loc, const CalendarDate& date,
                            const char* source, int64_t fetched_at, std::string* err) {
  s->version = kScheduleVersion;
  s->cache_date_istanbul = date;
  s->local_date = date;
  s->location = loc;
  s->location.latitude = normalize_coord(loc.latitude);
  s->location.longitude = normalize_coord(loc.longitude);
  s->fetched_at_unix = fetched_at;
  s->source = source;
  if (!fill_unix_times(s)) {
    if (err) *err = "validated times failed integrity checks";
    return false;
  }
  return true;
}

AladhanProvider::AladhanProvider(HttpClient* http, std::string endpoint)
    : http_(http), endpoint_(endpoint.empty() ? kDefaultAladhanEndpoint : endpoint) {}

bool AladhanProvider::fetch_daily(const Location& loc, const CalendarDate& date, int timeout_ms,
                                  PrayerSchedule* out, std::string* err) {
  if (!http_) {
    if (err) *err = "no http client";
    return false;
  }
  if (!loc.valid() || !date.valid()) {
    if (err) *err = "invalid location or date";
    return false;
  }
  char dbuf[32];
  std::snprintf(dbuf, sizeof(dbuf), "%02d-%02d-%04d", date.day, date.month, date.year);
  char q[512];
  std::snprintf(q, sizeof(q), "/%s?latitude=%.4f&longitude=%.4f&method=%d&timezonestring=%s", dbuf,
                loc.latitude, loc.longitude, calculation_method_for_location(loc),
                url_encode(loc.timezone).c_str());
  std::string url = endpoint_;
  if (!url.empty() && url[url.size() - 1] == '/') url.erase(url.size() - 1);
  url += q;

  HttpResult r = http_->get(url, timeout_ms);
  if (!r.ok) {
    if (err) *err = r.error.empty() ? "http request failed" : r.error;
    return false;
  }
  if (r.status != 200) {
    if (err) {
      char b[64];
      std::snprintf(b, sizeof(b), "http status %d", r.status);
      *err = b;
    }
    return false;
  }
  Json root;
  std::string perr;
  if (!Json::parse(r.body, &root, &perr)) {
    if (err) *err = "invalid JSON: " + perr;
    return false;
  }
  if (root.get("code").as_int(0) != 200 && !root.has("data")) {
    if (err) *err = "API returned error";
    return false;
  }
  const Json& data = root.get("data");
  const Json& timings = data.get("timings");
  if (!timings.is_object()) {
    if (err) *err = "missing timings";
    return false;
  }
  PrayerSchedule s;
  if (!parse_prayer_timings_object(timings, &s, err)) return false;

  const Json& greg = data.get("date").get("gregorian");
  if (greg.is_object() && greg.has("date")) {
    std::string gd = greg.get("date").as_string();
    int dd = 0, mm = 0, yy = 0;
    if (std::sscanf(gd.c_str(), "%d-%d-%d", &dd, &mm, &yy) == 3) {
      if (yy != date.year || mm != date.month || dd != date.day) {
        if (err) *err = "API date mismatch";
        return false;
      }
    }
  }
  const Json& meta = data.get("meta");
  if (meta.is_object()) {
    if (meta.has("timezone")) {
      std::string tz = meta.get("timezone").as_string();
      if (!tz.empty() && tz != loc.timezone) {
        // Keep configured timezone as authoritative for scheduling.
      }
    }
  }
  return finish_schedule(&s, loc, date, name(), SystemClock().now_unix(), err) && (*out = s, true);
}

IslamicFinderProvider::IslamicFinderProvider(HttpClient* http, std::string endpoint)
    : http_(http), endpoint_(endpoint) {}

bool IslamicFinderProvider::fetch_daily(const Location& loc, const CalendarDate& date,
                                        int timeout_ms, PrayerSchedule* out, std::string* err) {
  if (!http_ || endpoint_.empty()) {
    if (err) *err = "IslamicFinder endpoint not configured";
    return false;
  }
  if (!loc.valid() || !date.valid()) {
    if (err) *err = "invalid location or date";
    return false;
  }
  std::string url = endpoint_;
  char sep = endpoint_.find('?') == std::string::npos ? '?' : '&';
  char q2[640];
  std::snprintf(q2, sizeof(q2),
                "%clatitude=%.4f&longitude=%.4f&timezone=%s&time_format=0&date=%04d-%02d-%02d", sep,
                loc.latitude, loc.longitude, url_encode(loc.timezone).c_str(), date.year, date.month,
                date.day);
  url += q2;

  HttpResult r = http_->get(url, timeout_ms);
  if (!r.ok) {
    if (err) *err = r.error.empty() ? "http request failed" : r.error;
    return false;
  }
  if (r.status != 200) {
    if (err) *err = "http status not 200";
    return false;
  }
  Json root;
  std::string perr;
  if (!Json::parse(r.body, &root, &perr)) {
    if (err) *err = "invalid JSON: " + perr;
    return false;
  }
  const Json* timings = 0;
  if (root.get("results").is_object())
    timings = &root.get("results");
  else if (root.get("times").is_object())
    timings = &root.get("times");
  else if (root.get("timings").is_object())
    timings = &root.get("timings");
  else if (root.get("data").get("timings").is_object())
    timings = &root.get("data").get("timings");
  if (!timings) {
    if (err) *err = "missing timings in IslamicFinder response";
    return false;
  }
  PrayerSchedule s;
  if (!parse_prayer_timings_object(*timings, &s, err)) return false;
  return finish_schedule(&s, loc, date, name(), SystemClock().now_unix(), err) && (*out = s, true);
}

FallbackProvider::FallbackProvider(PrayerTimeProvider* primary, PrayerTimeProvider* secondary)
    : primary_(primary), secondary_(secondary) {}

bool FallbackProvider::fetch_daily(const Location& loc, const CalendarDate& date, int timeout_ms,
                                   PrayerSchedule* out, std::string* err) {
  std::string e1, e2;
  if (primary_ && primary_->fetch_daily(loc, date, timeout_ms, out, &e1)) return true;
  if (secondary_ && secondary_->fetch_daily(loc, date, timeout_ms, out, &e2)) return true;
  if (err) {
    *err = "all providers failed";
    if (!e1.empty()) *err += " [primary: " + e1 + "]";
    if (!e2.empty()) *err += " [secondary: " + e2 + "]";
  }
  return false;
}

}  // namespace adhan
