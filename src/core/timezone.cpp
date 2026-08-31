#include "timezone.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace adhan {
namespace {

// Howard Hinnant civil calendar algorithms (public domain).
int64_t days_from_civil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097LL + static_cast<int64_t>(doe) - 719468;
}

void civil_from_days(int64_t z, int* y, int* m, int* d) {
  z += 719468;
  const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const unsigned doe = static_cast<unsigned>(z - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const int yy = static_cast<int>(yoe) + static_cast<int>(era) * 400;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  const unsigned dd = doy - (153 * mp + 2) / 5 + 1;
  const unsigned mm = mp < 10 ? mp + 3 : mp - 9;
  *y = yy + (mm <= 2);
  *m = static_cast<int>(mm);
  *d = static_cast<int>(dd);
}

int weekday_from_days(int64_t z) {
  // 1970-01-01 was Thursday = 4. Return 0=Sun .. 6=Sat.
  int64_t w = (z + 4) % 7;
  if (w < 0) w += 7;
  return static_cast<int>(w);
}

int last_weekday_of_month(int year, int month, int weekday /*0=Sun*/) {
  int ny, nm, nd;
  CalendarDate next = next_date(CalendarDate{year, month, 1});
  // last day of month = day before next month's 1st
  int64_t first_next = days_from_civil(next.year, next.month, 1);
  int64_t last = first_next - 1;
  civil_from_days(last, &ny, &nm, &nd);
  int w = weekday_from_days(last);
  int delta = (w - weekday + 7) % 7;
  return nd - delta;
}

int nth_weekday_of_month(int year, int month, int n, int weekday) {
  int w = weekday_from_days(days_from_civil(year, month, 1));
  int day = 1 + (weekday - w + 7) % 7 + (n - 1) * 7;
  return day;
}

const TimeZoneInfo kTimezones[] = {
    {"Europe/Istanbul", kIstanbulOffsetSeconds, kIstanbulOffsetSeconds, DST_NONE},
    {"Asia/Riyadh", 3 * 3600, 3 * 3600, DST_NONE},
    {"Asia/Dubai", 4 * 3600, 4 * 3600, DST_NONE},
    {"Asia/Qatar", 3 * 3600, 3 * 3600, DST_NONE},
    {"Asia/Kuwait", 3 * 3600, 3 * 3600, DST_NONE},
    {"Asia/Baghdad", 3 * 3600, 3 * 3600, DST_NONE},
    {"Asia/Amman", 3 * 3600, 3 * 3600, DST_NONE},
    {"Asia/Beirut", 2 * 3600, 3 * 3600, DST_EU},
    {"Asia/Damascus", 3 * 3600, 3 * 3600, DST_NONE},
    {"Asia/Jakarta", 7 * 3600, 7 * 3600, DST_NONE},
    {"Asia/Kuala_Lumpur", 8 * 3600, 8 * 3600, DST_NONE},
    {"Africa/Cairo", 2 * 3600, 3 * 3600, DST_EU},
    {"Africa/Casablanca", 1 * 3600, 1 * 3600, DST_NONE},
    {"Europe/London", 0, 3600, DST_EU},
    {"Europe/Berlin", 3600, 2 * 3600, DST_EU},
    {"Europe/Paris", 3600, 2 * 3600, DST_EU},
    {"Europe/Amsterdam", 3600, 2 * 3600, DST_EU},
    {"Europe/Brussels", 3600, 2 * 3600, DST_EU},
    {"Europe/Vienna", 3600, 2 * 3600, DST_EU},
    {"Europe/Zurich", 3600, 2 * 3600, DST_EU},
    {"Europe/Rome", 3600, 2 * 3600, DST_EU},
    {"Europe/Madrid", 3600, 2 * 3600, DST_EU},
    {"Europe/Stockholm", 3600, 2 * 3600, DST_EU},
    {"Europe/Sarajevo", 3600, 2 * 3600, DST_EU},
    {"Europe/Athens", 2 * 3600, 3 * 3600, DST_EU},
    {"Europe/Moscow", 3 * 3600, 3 * 3600, DST_NONE},
    {"America/New_York", -5 * 3600, -4 * 3600, DST_US},
    {"America/Chicago", -6 * 3600, -5 * 3600, DST_US},
    {"America/Denver", -7 * 3600, -6 * 3600, DST_US},
    {"America/Los_Angeles", -8 * 3600, -7 * 3600, DST_US},
    {"America/Toronto", -5 * 3600, -4 * 3600, DST_US},
    {"America/Vancouver", -8 * 3600, -7 * 3600, DST_US},
    {"UTC", 0, 0, DST_NONE},
    {"Etc/UTC", 0, 0, DST_NONE},
    {"Etc/GMT", 0, 0, DST_NONE},
};

bool eu_dst_active(int64_t unix_utc) {
  int y, m, d, h, mi, s;
  unix_to_civil_utc(unix_utc, &y, &m, &d, &h, &mi, &s);
  int last_sun_mar = last_weekday_of_month(y, 3, 0);
  int last_sun_oct = last_weekday_of_month(y, 10, 0);
  int64_t start = civil_to_unix_utc(y, 3, last_sun_mar, 1, 0, 0);  // 01:00 UTC
  int64_t end = civil_to_unix_utc(y, 10, last_sun_oct, 1, 0, 0);
  return unix_utc >= start && unix_utc < end;
}

bool us_dst_active(int64_t unix_utc, int std_off) {
  int y, m, d, h, mi, s;
  unix_to_civil_utc(unix_utc, &y, &m, &d, &h, &mi, &s);
  int mar_day = nth_weekday_of_month(y, 3, 2, 0);  // 2nd Sunday
  int nov_day = nth_weekday_of_month(y, 11, 1, 0);  // 1st Sunday
  // 02:00 local standard time
  int64_t start = civil_to_unix_utc(y, 3, mar_day, 2, 0, 0) - std_off;
  // 02:00 local DST = 01:00 standard
  int64_t end = civil_to_unix_utc(y, 11, nov_day, 2, 0, 0) - (std_off + 3600);
  return unix_utc >= start && unix_utc < end;
}

}  // namespace

const TimeZoneInfo* find_timezone(const std::string& iana) {
  for (size_t i = 0; i < sizeof(kTimezones) / sizeof(kTimezones[0]); ++i) {
    if (iana == kTimezones[i].iana) return &kTimezones[i];
  }
  return 0;
}

int timezone_offset_seconds(const std::string& iana, int64_t unix_utc) {
  // Europe/Istanbul is hardcoded GMT+3 regardless of Windows TZ or historical DST.
  if (iana.empty() || iana == kAuthoritativeTimezone || iana == "Turkey") {
    return kIstanbulOffsetSeconds;
  }
  const TimeZoneInfo* tz = find_timezone(iana);
  if (!tz) {
    // Unknown zone: default to Istanbul offset so cache lifecycle stays defined.
    return kIstanbulOffsetSeconds;
  }
  if (tz->dst == DST_NONE) return tz->std_offset_seconds;
  if (tz->dst == DST_EU && eu_dst_active(unix_utc)) return tz->dst_offset_seconds;
  if (tz->dst == DST_US && us_dst_active(unix_utc, tz->std_offset_seconds)) {
    return tz->dst_offset_seconds;
  }
  return tz->std_offset_seconds;
}

std::string timezone_display(const std::string& iana, int64_t unix_utc) {
  int off = timezone_offset_seconds(iana, unix_utc);
  char sign = off < 0 ? '-' : '+';
  int abs_off = off < 0 ? -off : off;
  int hh = abs_off / 3600;
  int mm = (abs_off % 3600) / 60;
  char buf[80];
  std::snprintf(buf, sizeof(buf), "%s (GMT%c%d%s)", iana.c_str(), sign, hh,
                mm ? (hh >= 0 ? "" : "") : "");
  // Always show minutes if needed.
  if (mm != 0) {
    std::snprintf(buf, sizeof(buf), "%s (GMT%c%d:%02d)", iana.c_str(), sign, hh, mm);
  } else {
    std::snprintf(buf, sizeof(buf), "%s (GMT%c%d)", iana.c_str(), sign, hh);
  }
  return buf;
}

int64_t civil_to_unix_utc(int year, int month, int day, int hour, int minute, int second) {
  int64_t days = days_from_civil(year, static_cast<unsigned>(month), static_cast<unsigned>(day));
  return days * 86400 + hour * 3600 + minute * 60 + second;
}

void unix_to_civil_utc(int64_t unix_utc, int* year, int* month, int* day, int* hour, int* minute,
                       int* second) {
  int64_t days = unix_utc / 86400;
  int64_t rem = unix_utc % 86400;
  if (rem < 0) {
    rem += 86400;
    days -= 1;
  }
  civil_from_days(days, year, month, day);
  *hour = static_cast<int>(rem / 3600);
  rem %= 3600;
  *minute = static_cast<int>(rem / 60);
  *second = static_cast<int>(rem % 60);
}

CalendarDate utc_date(int64_t unix_utc) {
  int y, m, d, h, mi, s;
  unix_to_civil_utc(unix_utc, &y, &m, &d, &h, &mi, &s);
  CalendarDate dt;
  dt.year = y;
  dt.month = m;
  dt.day = d;
  return dt;
}

CalendarDate zoned_date(int64_t unix_utc, const std::string& iana) {
  int off = timezone_offset_seconds(iana, unix_utc);
  return utc_date(unix_utc + off);
}

CalendarDate next_date(CalendarDate d) {
  int64_t unix = civil_to_unix_utc(d.year, d.month, d.day, 0, 0, 0) + 86400;
  return utc_date(unix);
}

CalendarDate prev_date(CalendarDate d) {
  int64_t unix = civil_to_unix_utc(d.year, d.month, d.day, 0, 0, 0) - 86400;
  return utc_date(unix);
}

int64_t zoned_local_to_unix(CalendarDate date, int hour, int minute, int second,
                            const std::string& iana) {
  // Treat the wall clock as UTC, then subtract the zone offset at that instant.
  int64_t as_utc = civil_to_unix_utc(date.year, date.month, date.day, hour, minute, second);
  const TimeZoneInfo* tz = find_timezone(iana);
  int std_off = tz ? tz->std_offset_seconds : kIstanbulOffsetSeconds;
  if (iana.empty() || iana == kAuthoritativeTimezone) std_off = kIstanbulOffsetSeconds;
  int64_t unix = as_utc - std_off;
  int off = timezone_offset_seconds(iana.empty() ? kAuthoritativeTimezone : iana, unix);
  return as_utc - off;
}

CalendarDate istanbul_date(int64_t unix_utc) {
  return utc_date(unix_utc + kIstanbulOffsetSeconds);
}

int64_t istanbul_local_to_unix(CalendarDate date, int hour, int minute, int second) {
  int64_t as_utc = civil_to_unix_utc(date.year, date.month, date.day, hour, minute, second);
  return as_utc - kIstanbulOffsetSeconds;
}

int64_t today_istanbul_0310(int64_t now_unix) {
  CalendarDate d = istanbul_date(now_unix);
  return istanbul_local_to_unix(d, kDailyCacheCheckHour, kDailyCacheCheckMinute, 0);
}

int64_t next_istanbul_0310(int64_t now_unix) {
  int64_t today = today_istanbul_0310(now_unix);
  if (now_unix < today) return today;
  CalendarDate d = next_date(istanbul_date(now_unix));
  return istanbul_local_to_unix(d, kDailyCacheCheckHour, kDailyCacheCheckMinute, 0);
}

bool parse_hhmm(const std::string& text, int* hour, int* minute) {
  // Accept "HH:MM", "H:MM", optionally followed by a suffix such as " (EEST)".
  int h = -1, m = -1;
  int n = 0;
  const char* p = text.c_str();
  while (*p && (*p < '0' || *p > '9')) ++p;
  if (std::sscanf(p, "%d:%d%n", &h, &m, &n) < 2) return false;
  if (h == 24 && m == 0) {
    h = 0;
  }
  if (h < 0 || h > 23 || m < 0 || m > 59) return false;
  *hour = h;
  *minute = m;
  return true;
}

}  // namespace adhan
