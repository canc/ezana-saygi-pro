#include "schedule.h"

#include <cmath>
#include <cstdio>

#include "json.h"
#include "timezone.h"

#include <cstring>

namespace adhan {

double normalize_coord(double v) {
  // Four decimal places; avoid -0.0000 vs 0.0000 duplicates.
  double s = std::floor(v * 10000.0 + 0.5) / 10000.0;
  if (s == 0.0) s = 0.0;
  return s;
}

std::string make_event_id(const PrayerSchedule& s, PrayerId id) {
  char buf[192];
  std::snprintf(buf, sizeof(buf), "%s:%s:%.4f:%.4f:%s", s.cache_date_istanbul.iso().c_str(),
                prayer_name(id), normalize_coord(s.location.latitude),
                normalize_coord(s.location.longitude), s.location.timezone.c_str());
  return buf;
}

bool fill_unix_times(PrayerSchedule* s) {
  if (!s) return false;
  if (s->version == 0) s->version = kScheduleVersion;
  for (int i = 0; i < PRAYER_COUNT; ++i) {
    PrayerOccurrence& p = s->prayers[i];
    if (!p.valid) continue;
    CalendarDate d = s->local_date.valid() ? s->local_date : s->cache_date_istanbul;
    p.unix_utc = zoned_local_to_unix(d, p.hour, p.minute, 0, s->location.timezone);
    p.id = static_cast<PrayerId>(i);
  }
  return s->valid();
}

static Json occ_to_json(const PrayerOccurrence& p) {
  Json j = Json::object();
  j["name"] = Json::string(prayer_name(p.id));
  j["valid"] = Json::boolean(p.valid);
  j["hour"] = Json::number(p.hour);
  j["minute"] = Json::number(p.minute);
  j["unix_utc"] = Json::number(static_cast<double>(p.unix_utc));
  return j;
}

static bool occ_from_json(const Json& j, PrayerId expected, PrayerOccurrence* p, std::string* err) {
  p->id = expected;
  p->valid = j.get("valid").as_bool(false);
  p->hour = j.get("hour").as_int(-1);
  p->minute = j.get("minute").as_int(-1);
  p->unix_utc = j.get("unix_utc").as_int64(0);
  if (j.has("name")) {
    PrayerId parsed;
    if (prayer_id_from_name(j.get("name").as_string(), &parsed) && parsed != expected) {
      if (err) *err = "prayer name mismatch";
      return false;
    }
  }
  return true;
}

std::string schedule_to_json(const PrayerSchedule& s) {
  Json j = Json::object();
  j["version"] = Json::number(kScheduleVersion);
  j["date"] = Json::string(s.cache_date_istanbul.iso());
  j["local_date"] = Json::string(s.local_date.iso());
  j["country"] = Json::string(s.location.country);
  j["city"] = Json::string(s.location.city);
  j["timezone"] = Json::string(s.location.timezone);
  j["latitude"] = Json::number(normalize_coord(s.location.latitude));
  j["longitude"] = Json::number(normalize_coord(s.location.longitude));
  j["fetchedAt"] = Json::number(static_cast<double>(s.fetched_at_unix));
  j["source"] = Json::string(s.source);
  Json prayers = Json::object();
  prayers["fajr"] = occ_to_json(s.prayers[PRAYER_FAJR]);
  prayers["sunrise"] = occ_to_json(s.prayers[PRAYER_SUNRISE]);
  prayers["dhuhr"] = occ_to_json(s.prayers[PRAYER_DHUHR]);
  prayers["asr"] = occ_to_json(s.prayers[PRAYER_ASR]);
  prayers["maghrib"] = occ_to_json(s.prayers[PRAYER_MAGHRIB]);
  prayers["isha"] = occ_to_json(s.prayers[PRAYER_ISHA]);
  j["prayers"] = prayers;
  return j.stringify(true);
}

static bool parse_iso_date(const std::string& iso, CalendarDate* d) {
  int y = 0, m = 0, day = 0;
  if (std::sscanf(iso.c_str(), "%d-%d-%d", &y, &m, &day) != 3) return false;
  d->year = y;
  d->month = m;
  d->day = day;
  return d->valid();
}

bool schedule_from_json(const std::string& text, PrayerSchedule* out, std::string* err) {
  Json j;
  if (!Json::parse(text, &j, err)) return false;
  if (!j.is_object()) {
    if (err) *err = "schedule is not an object";
    return false;
  }
  PrayerSchedule s;
  s.version = j.get("version").as_int(0);
  if (s.version != kScheduleVersion && s.version != kCacheVersion && s.version != 1) {
    if (err) *err = "unsupported cache version";
    return false;
  }
  if (!parse_iso_date(j.get("date").as_string(), &s.cache_date_istanbul)) {
    if (err) *err = "missing/invalid date";
    return false;
  }
  if (j.has("local_date")) {
    parse_iso_date(j.get("local_date").as_string(), &s.local_date);
  } else {
    s.local_date = s.cache_date_istanbul;
  }
  s.location.country = j.get("country").as_string();
  s.location.city = j.get("city").as_string();
  s.location.timezone = j.get("timezone").as_string();
  s.location.latitude = j.get("latitude").as_number();
  s.location.longitude = j.get("longitude").as_number();
  s.fetched_at_unix = j.get("fetchedAt").as_int64(0);
  s.source = j.get("source").as_string("unknown");
  const Json& prayers = j.get("prayers");
  if (!prayers.is_object()) {
    if (err) *err = "missing prayers";
    return false;
  }
  if (!occ_from_json(prayers.get("fajr"), PRAYER_FAJR, &s.prayers[PRAYER_FAJR], err)) return false;
  occ_from_json(prayers.get("sunrise"), PRAYER_SUNRISE, &s.prayers[PRAYER_SUNRISE], err);
  if (!occ_from_json(prayers.get("dhuhr"), PRAYER_DHUHR, &s.prayers[PRAYER_DHUHR], err)) return false;
  if (!occ_from_json(prayers.get("asr"), PRAYER_ASR, &s.prayers[PRAYER_ASR], err)) return false;
  if (!occ_from_json(prayers.get("maghrib"), PRAYER_MAGHRIB, &s.prayers[PRAYER_MAGHRIB], err))
    return false;
  if (!occ_from_json(prayers.get("isha"), PRAYER_ISHA, &s.prayers[PRAYER_ISHA], err)) return false;

  if (s.prayers[PRAYER_FAJR].unix_utc <= 0) {
    if (!fill_unix_times(&s)) {
      if (err) *err = "invalid prayer times";
      return false;
    }
  }
  if (!s.valid()) {
    if (err) *err = "schedule failed validation";
    return false;
  }
  *out = s;
  return true;
}

}  // namespace adhan
