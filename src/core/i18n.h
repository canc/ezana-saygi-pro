#pragma once

#include <string>

#include "types.h"

namespace adhan {
namespace ui {

// Default UI language is Turkish. English can be added later by extending this module.
enum Language { LANG_TR = 0, LANG_EN = 1 };

Language language();

const wchar_t* app_title();
const wchar_t* status_label();
const wchar_t* location_label();
const wchar_t* city_label();
const wchar_t* timezone_label();
const wchar_t* threshold_label();
const wchar_t* next_prayer_label();
const wchar_t* detect_location();
const wchar_t* refresh_times();
const wchar_t* close_hint();
const wchar_t* active();
const wchar_t* inactive();
const wchar_t* enable_action();
const wchar_t* disable_action();
const wchar_t* toggle_action(bool currently_enabled);
std::wstring app_version();
const wchar_t* tray_show();
const wchar_t* tray_exit();
const wchar_t* threshold_option(int seconds);
const wchar_t* prayer(PrayerId id);
const wchar_t* country(const char* english_name);
const wchar_t* err_geo_unavailable();
const wchar_t* err_geo_unmapped();
const wchar_t* err_geo_no_cities();
const wchar_t* note_geo_same_country();
const wchar_t* note_schedule_updated();
const wchar_t* err_schedule_unavailable();
const wchar_t* status_updating();
const wchar_t* status_no_schedule();
const wchar_t* status_fading_out();
const wchar_t* status_muted();
const wchar_t* status_fading_in();
const wchar_t* status_no_remaining();
const wchar_t* em_dash();
const wchar_t* duration_settings();
const wchar_t* duration_save();
const wchar_t* duration_cancel();
const wchar_t* duration_minutes();
const wchar_t* duration_invalid();

std::wstring location_item(const char* city, const char* country_en);
std::wstring timezone_text(const std::string& iana, int64_t unix_utc);
std::wstring tray_tip(const wchar_t* next_prayer, bool enabled);
std::wstring source_text(const std::string& source);

}  // namespace ui
}  // namespace adhan
