#include "config.h"

#include "fsutil.h"
#include "json.h"
#include "locations.h"

namespace adhan {
namespace {

int read_duration_field(const Json& durs, const char* key, const char* alt, int fallback) {
  int raw = 0;
  bool present = false;
  if (durs.has(key) && durs.get(key).is_number()) {
    raw = durs.get(key).as_int(0);
    present = true;
  } else if (alt && durs.has(alt) && durs.get(alt).is_number()) {
    raw = durs.get(alt).as_int(0);
    present = true;
  }
  if (!present) return fallback;
  if (!valid_adhan_duration_seconds(raw)) {
    int clamped = clamp_adhan_duration_seconds(raw);
    if (valid_adhan_duration_seconds(clamped)) return clamped;
    return fallback;
  }
  return raw;
}

}  // namespace

std::string config_to_json(const AppConfig& cfg) {
  Json j = Json::object();
  j["version"] = Json::number(kConfigVersion);
  j["enabled"] = Json::boolean(cfg.enabled);
  j["country"] = Json::string(cfg.country);
  j["city"] = Json::string(cfg.city);
  j["latitude"] = Json::number(cfg.latitude);
  j["longitude"] = Json::number(cfg.longitude);
  j["timezone"] = Json::string(cfg.timezone);
  j["threshold_seconds"] = Json::number(cfg.threshold_seconds);
  Json durs = Json::object();
  durs["imsak"] = Json::number(cfg.adhan_duration_for(PRAYER_FAJR));
  durs["dhuhr"] = Json::number(cfg.adhan_duration_for(PRAYER_DHUHR));
  durs["asr"] = Json::number(cfg.adhan_duration_for(PRAYER_ASR));
  durs["maghrib"] = Json::number(cfg.adhan_duration_for(PRAYER_MAGHRIB));
  durs["isha"] = Json::number(cfg.adhan_duration_for(PRAYER_ISHA));
  j["adhan_durations"] = durs;
  j["fade_duration_ms"] = Json::number(cfg.fade_duration_ms);
  j["aladhan_endpoint"] = Json::string(cfg.aladhan_endpoint);
  j["islamicfinder_endpoint"] = Json::string(cfg.islamicfinder_endpoint);
  j["islamicfinder_city_id"] = Json::number(cfg.islamicfinder_city_id);
  j["http_timeout_ms"] = Json::number(cfg.http_timeout_ms);
  return j.stringify(true);
}

bool config_from_json(const std::string& text, AppConfig* out, std::string* err) {
  Json j;
  if (!Json::parse(text, &j, err)) return false;
  if (!j.is_object()) {
    if (err) *err = "config is not an object";
    return false;
  }
  AppConfig c = default_config();
  if (j.has("version")) c.version = j.get("version").as_int(kConfigVersion);
  if (j.has("enabled")) c.enabled = j.get("enabled").as_bool(true);
  if (j.has("country")) c.country = j.get("country").as_string(kDefaultCountry);
  if (j.has("city")) c.city = j.get("city").as_string(kDefaultCity);
  if (j.has("latitude")) c.latitude = j.get("latitude").as_number(kDefaultLatitude);
  if (j.has("longitude")) c.longitude = j.get("longitude").as_number(kDefaultLongitude);
  if (j.has("timezone")) c.timezone = j.get("timezone").as_string(kDefaultTimezone);
  if (j.has("threshold_seconds")) c.threshold_seconds = j.get("threshold_seconds").as_int(60);
  if (j.has("fade_duration_ms"))
    c.fade_duration_ms = j.get("fade_duration_ms").as_int(DEFAULT_FADE_DURATION_MS);
  if (j.has("aladhan_endpoint"))
    c.aladhan_endpoint = j.get("aladhan_endpoint").as_string(kDefaultAladhanEndpoint);
  if (j.has("islamicfinder_endpoint"))
    c.islamicfinder_endpoint =
        j.get("islamicfinder_endpoint").as_string(kDefaultIslamicFinderEndpoint);
  c.islamicfinder_endpoint = canonical_islamicfinder_endpoint(c.islamicfinder_endpoint);
  if (j.has("islamicfinder_city_id"))
    c.islamicfinder_city_id = j.get("islamicfinder_city_id").as_int(0);
  apply_islamicfinder_place(&c);
  if (j.has("http_timeout_ms")) c.http_timeout_ms = j.get("http_timeout_ms").as_int(kHttpTimeoutMs);

  int legacy = 0;
  bool has_legacy = false;
  if (j.has("adhan_duration_seconds") && j.get("adhan_duration_seconds").is_number()) {
    legacy = j.get("adhan_duration_seconds").as_int(0);
    has_legacy = valid_adhan_duration_seconds(legacy) ||
                 valid_adhan_duration_seconds(clamp_adhan_duration_seconds(legacy));
    if (has_legacy && !valid_adhan_duration_seconds(legacy))
      legacy = clamp_adhan_duration_seconds(legacy);
    c.adhan_duration_seconds = has_legacy ? legacy : DEFAULT_ADHAN_DURATION_SECONDS;
  }

  const bool has_durs = j.has("adhan_durations") && j.get("adhan_durations").is_object();
  if (has_durs) {
    const Json& d = j.get("adhan_durations");
    c.adhan_durations[PRAYER_FAJR] =
        read_duration_field(d, "imsak", "fajr", default_adhan_duration_seconds(PRAYER_FAJR));
    c.adhan_durations[PRAYER_DHUHR] =
        read_duration_field(d, "dhuhr", "zuhr", default_adhan_duration_seconds(PRAYER_DHUHR));
    c.adhan_durations[PRAYER_ASR] =
        read_duration_field(d, "asr", 0, default_adhan_duration_seconds(PRAYER_ASR));
    c.adhan_durations[PRAYER_MAGHRIB] =
        read_duration_field(d, "maghrib", 0, default_adhan_duration_seconds(PRAYER_MAGHRIB));
    c.adhan_durations[PRAYER_ISHA] =
        read_duration_field(d, "isha", 0, default_adhan_duration_seconds(PRAYER_ISHA));
  } else if (has_legacy) {
    c.adhan_durations[PRAYER_FAJR] = legacy;
    c.adhan_durations[PRAYER_DHUHR] = legacy;
    c.adhan_durations[PRAYER_ASR] = legacy;
    c.adhan_durations[PRAYER_MAGHRIB] = legacy;
    c.adhan_durations[PRAYER_ISHA] = legacy;
  }

  bool ok_threshold = false;
  for (int i = 0; i < kThresholdOptionCount; ++i) {
    if (c.threshold_seconds == kThresholdOptions[i]) ok_threshold = true;
  }
  if (!ok_threshold) c.threshold_seconds = DEFAULT_THRESHOLD_SECONDS;
  if (c.fade_duration_ms < 500) c.fade_duration_ms = DEFAULT_FADE_DURATION_MS;
  if (c.timezone.empty()) c.timezone = kDefaultTimezone;
  if (!c.location().valid()) {
    if (err) *err = "invalid location in config";
    return false;
  }
  *out = c;
  return true;
}

bool load_config(const std::string& path, AppConfig* out, std::string* err) {
  std::string text;
  if (!read_file(path, &text)) {
    *out = default_config();
    return true;  // missing file -> defaults
  }
  return config_from_json(text, out, err);
}

bool save_config(const std::string& path, const AppConfig& cfg) {
  AppConfig stored = cfg;
  apply_islamicfinder_place(&stored);
  std::string parent = parent_path(path);
  if (!parent.empty()) mkdir_p(parent);
  return write_file_atomic(path, config_to_json(stored));
}

}  // namespace adhan
