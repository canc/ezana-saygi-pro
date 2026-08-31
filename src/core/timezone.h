#pragma once

#include <cstdint>
#include <string>

#include "types.h"

namespace adhan {

enum DstRule { DST_NONE = 0, DST_EU, DST_US };

struct TimeZoneInfo {
  const char* iana;
  int std_offset_seconds;
  int dst_offset_seconds;
  DstRule dst;
};

const TimeZoneInfo* find_timezone(const std::string& iana);
int timezone_offset_seconds(const std::string& iana, int64_t unix_utc);
std::string timezone_display(const std::string& iana, int64_t unix_utc);

// Civil date conversion (proleptic Gregorian, UTC). Independent of libc TZ.
int64_t civil_to_unix_utc(int year, int month, int day, int hour, int minute, int second);
void unix_to_civil_utc(int64_t unix_utc, int* year, int* month, int* day, int* hour, int* minute,
                       int* second);

CalendarDate utc_date(int64_t unix_utc);
CalendarDate zoned_date(int64_t unix_utc, const std::string& iana);
CalendarDate next_date(CalendarDate d);
CalendarDate prev_date(CalendarDate d);

int64_t zoned_local_to_unix(CalendarDate date, int hour, int minute, int second,
                            const std::string& iana);

// Authoritative cache timezone: Europe/Istanbul, GMT+3, never Windows TZ.
CalendarDate istanbul_date(int64_t unix_utc);
int64_t istanbul_local_to_unix(CalendarDate date, int hour, int minute, int second);
int64_t next_istanbul_0310(int64_t now_unix);
int64_t today_istanbul_0310(int64_t now_unix);

bool parse_hhmm(const std::string& text, int* hour, int* minute);

// Wall-clock HH:MM:SS in the given IANA zone (not Windows TZ).
std::string format_zoned_hms(int64_t unix_utc, const std::string& iana);

}  // namespace adhan
