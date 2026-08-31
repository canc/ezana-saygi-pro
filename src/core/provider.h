#pragma once

#include <string>

#include "types.h"

namespace adhan {

class PrayerTimeProvider {
 public:
  virtual ~PrayerTimeProvider() {}
  virtual const char* name() const = 0;
  virtual bool fetch_daily(const Location& loc, const CalendarDate& date, int timeout_ms,
                           PrayerSchedule* out, std::string* err) = 0;
};

// Compatible public Awqat Salah API (Aladhan). No API key required.
class AladhanProvider : public PrayerTimeProvider {
 public:
  AladhanProvider(HttpClient* http, std::string endpoint);
  const char* name() const override { return "Aladhan"; }
  bool fetch_daily(const Location& loc, const CalendarDate& date, int timeout_ms,
                   PrayerSchedule* out, std::string* err) override;

 private:
  HttpClient* http_;
  std::string endpoint_;
};

// Optional Islamic Finder compatible endpoint. Used as fallback.
class IslamicFinderProvider : public PrayerTimeProvider {
 public:
  IslamicFinderProvider(HttpClient* http, std::string endpoint);
  const char* name() const override { return "IslamicFinder"; }
  bool fetch_daily(const Location& loc, const CalendarDate& date, int timeout_ms,
                   PrayerSchedule* out, std::string* err) override;

 private:
  HttpClient* http_;
  std::string endpoint_;
};

class FallbackProvider : public PrayerTimeProvider {
 public:
  FallbackProvider(PrayerTimeProvider* primary, PrayerTimeProvider* secondary);
  const char* name() const override { return "Fallback"; }
  bool fetch_daily(const Location& loc, const CalendarDate& date, int timeout_ms,
                   PrayerSchedule* out, std::string* err) override;

 private:
  PrayerTimeProvider* primary_;
  PrayerTimeProvider* secondary_;
};

std::string url_encode(const std::string& s);
int calculation_method_for_location(const Location& loc);
bool parse_prayer_timings_object(const class Json& timings, PrayerSchedule* out, std::string* err);

}  // namespace adhan
