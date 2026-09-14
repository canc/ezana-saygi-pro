#pragma once

#include <string>

#include "types.h"

namespace adhan {

std::string schedule_to_json(const PrayerSchedule& s);
bool schedule_from_json(const std::string& text, PrayerSchedule* out, std::string* err);
bool fill_unix_times(PrayerSchedule* s);
std::string make_event_id(const PrayerSchedule& s, PrayerId id);
double normalize_coord(double v);

// Date + location + unix prayer times. Used to decide whether a refresh must
// rebuild scheduler/timer state.
bool schedules_same_identity(const PrayerSchedule& a, const PrayerSchedule& b);

// True when `candidate` must not replace `installed` as the active schedule
// (older calendar date than today's installed schedule, or older fetch of the
// same day). Location changes are never treated as stale.
bool is_stale_schedule(const PrayerSchedule& candidate, const PrayerSchedule& installed,
                       const CalendarDate& today);

}  // namespace adhan
