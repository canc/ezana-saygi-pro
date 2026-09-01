#pragma once

#include <string>

#include "logger.h"
#include "types.h"

namespace adhan {

class PrayerTimeProvider {
 public:
  virtual ~PrayerTimeProvider() {}
  virtual const char* name() const = 0;
  virtual bool fetch_daily(const Location& loc, const CalendarDate& date, int timeout_ms,
                           PrayerSchedule* out, std::string* err) = 0;
  virtual void set_logger(Logger* log) { (void)log; }
};

// Public Awqat Salah API. Primary provider. Turkish locations use Diyanet method 13.
class AladhanProvider : public PrayerTimeProvider {
 public:
  AladhanProvider(HttpClient* http, std::string endpoint);
  AladhanProvider(HttpClient* http, std::string endpoint, int calculation_method);
  const char* name() const override { return "Aladhan"; }
  void set_logger(Logger* log) override { log_ = log; }
  int calculation_method() const { return calculation_method_; }
  bool fetch_daily(const Location& loc, const CalendarDate& date, int timeout_ms,
                   PrayerSchedule* out, std::string* err) override;

 private:
  HttpClient* http_;
  std::string endpoint_;
  int calculation_method_;
  Logger* log_;

  int method_for(const Location& loc) const;
};

// Optional machine-readable IslamicFinder JSON endpoint (never the retired
// /index.php/api/prayer_times path). Empty endpoint means "not configured".
class IslamicFinderApiClient {
 public:
  IslamicFinderApiClient(HttpClient* http, std::string json_endpoint);
  bool fetch(const Location& loc, const CalendarDate& date, int timeout_ms, PrayerSchedule* out,
             std::string* err);

 private:
  HttpClient* http_;
  std::string json_endpoint_;
};

// Single targeted GET of the public city prayer-times page on islamicfinder.org.
class IslamicFinderPageClient {
 public:
  explicit IslamicFinderPageClient(HttpClient* http);
  bool fetch(const Location& loc, const CalendarDate& date, int timeout_ms, PrayerSchedule* out,
             std::string* err);

 private:
  HttpClient* http_;
  bool resolve_page_url(const Location& loc, int timeout_ms, std::string* url, std::string* err);
};

// Fallback provider: public city page, then optional JSON API.
class IslamicFinderProvider : public PrayerTimeProvider {
 public:
  IslamicFinderProvider(HttpClient* http, std::string json_endpoint);
  const char* name() const override { return "IslamicFinder"; }
  void set_logger(Logger* log) override { log_ = log; }
  bool fetch_daily(const Location& loc, const CalendarDate& date, int timeout_ms,
                   PrayerSchedule* out, std::string* err) override;

 private:
  IslamicFinderApiClient api_;
  IslamicFinderPageClient page_;
  Logger* log_;
};

class FallbackProvider : public PrayerTimeProvider {
 public:
  FallbackProvider(PrayerTimeProvider* primary, PrayerTimeProvider* secondary, Logger* log = 0);
  const char* name() const override { return "Fallback"; }
  void set_logger(Logger* log) override { log_ = log; }
  bool fetch_daily(const Location& loc, const CalendarDate& date, int timeout_ms,
                   PrayerSchedule* out, std::string* err) override;

 private:
  PrayerTimeProvider* primary_;
  PrayerTimeProvider* secondary_;
  Logger* log_;
};

std::string url_encode(const std::string& s);
int calculation_method_for_location(const Location& loc);
bool parse_prayer_timings_object(const class Json& timings, PrayerSchedule* out, std::string* err);
bool parse_islamicfinder_json_body(const std::string& body, PrayerSchedule* out, std::string* err);
bool parse_islamicfinder_page_html(const std::string& html, PrayerSchedule* out, std::string* err);
bool parse_islamicfinder_page_html(const std::string& html, const CalendarDate& date,
                                   PrayerSchedule* out, std::string* err);

}  // namespace adhan
