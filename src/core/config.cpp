#include "config.h"

#include "fsutil.h"
#include "json.h"
#include "locations.h"

namespace adhan {

std::string config_to_json(const AppConfig& cfg) {
  Json j = Json::object();
  j["version"] = Json::number(cfg.version);
  j["enabled"] = Json::boolean(cfg.enabled);
  j["country"] = Json::string(cfg.country);
  j["city"] = Json::string(cfg.city);
  j["latitude"] = Json::number(cfg.latitude);
  j["longitude"] = Json::number(cfg.longitude);
  j["timezone"] = Json::string(cfg.timezone);
  j["threshold_seconds"] = Json::number(cfg.threshold_seconds);
  j["adhan_duration_seconds"] = Json::number(cfg.adhan_duration_seconds);
  j["fade_duration_ms"] = Json::number(cfg.fade_duration_ms);
  j["aladhan_endpoint"] = Json::string(cfg.aladhan_endpoint);
  j["islamicfinder_endpoint"] = Json::string(cfg.islamicfinder_endpoint);
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
  if (j.has("adhan_duration_seconds"))
    c.adhan_duration_seconds = j.get("adhan_duration_seconds").as_int(DEFAULT_ADHAN_DURATION_SECONDS);
  if (j.has("fade_duration_ms"))
    c.fade_duration_ms = j.get("fade_duration_ms").as_int(DEFAULT_FADE_DURATION_MS);
  if (j.has("aladhan_endpoint"))
    c.aladhan_endpoint = j.get("aladhan_endpoint").as_string(kDefaultAladhanEndpoint);
  if (j.has("islamicfinder_endpoint"))
    c.islamicfinder_endpoint =
        j.get("islamicfinder_endpoint").as_string(kDefaultIslamicFinderEndpoint);
  if (j.has("http_timeout_ms")) c.http_timeout_ms = j.get("http_timeout_ms").as_int(kHttpTimeoutMs);

  bool ok_threshold = false;
  for (int i = 0; i < kThresholdOptionCount; ++i) {
    if (c.threshold_seconds == kThresholdOptions[i]) ok_threshold = true;
  }
  if (!ok_threshold) c.threshold_seconds = DEFAULT_THRESHOLD_SECONDS;
  if (c.adhan_duration_seconds < 30) c.adhan_duration_seconds = DEFAULT_ADHAN_DURATION_SECONDS;
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
  std::string parent = parent_path(path);
  if (!parent.empty()) mkdir_p(parent);
  return write_file_atomic(path, config_to_json(cfg));
}

}  // namespace adhan
