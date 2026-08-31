#pragma once

#include <string>

#include "types.h"

namespace adhan {

std::string schedule_to_json(const PrayerSchedule& s);
bool schedule_from_json(const std::string& text, PrayerSchedule* out, std::string* err);
bool fill_unix_times(PrayerSchedule* s);
std::string make_event_id(const PrayerSchedule& s, PrayerId id);
double normalize_coord(double v);

}  // namespace adhan
