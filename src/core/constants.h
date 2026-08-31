#pragma once

namespace adhan {

constexpr const char* kAppName = "Adhan Volume";
constexpr const char* kAppFolderName = "AdhanVolume";
constexpr const char* kVersion = "1.0.3";
constexpr const char* kMutexName = "Local\\AdhanVolumeSingleton";

constexpr int kConfigVersion = 2;
constexpr int kCacheVersion = 1;
constexpr int kScheduleVersion = 1;

constexpr int DEFAULT_ADHAN_DURATION_SECONDS = 180;
constexpr int DEFAULT_ADHAN_DURATION_FAJR_SECONDS = 300;
constexpr int DEFAULT_ADHAN_DURATION_DHUHR_SECONDS = 300;
constexpr int DEFAULT_ADHAN_DURATION_ASR_SECONDS = 180;
constexpr int DEFAULT_ADHAN_DURATION_MAGHRIB_SECONDS = 180;
constexpr int DEFAULT_ADHAN_DURATION_ISHA_SECONDS = 420;
constexpr int kAdhanDurationMinMinutes = 1;
constexpr int kAdhanDurationMaxMinutes = 30;
constexpr int kAdhanDurationMinSeconds = 60;
constexpr int kAdhanDurationMaxSeconds = 1800;
constexpr int DEFAULT_FADE_DURATION_MS = 4000;
constexpr int DEFAULT_THRESHOLD_SECONDS = 60;
constexpr int kSchedulerIdleIntervalMs = 15000;
constexpr int kSchedulerSoonIntervalMs = 5000;
constexpr int kSchedulerNearIntervalMs = 1000;
constexpr int kSchedulerSoonWindowMs = 600000;
constexpr int kSchedulerNearWindowMs = 120000;
constexpr int kClockJumpThresholdMs = 45000;
constexpr int kFadeTickMs = 100;
constexpr int kMuteHoldTickMs = 1000;
constexpr float kVolumeVerifyEpsilon = 0.03f;
constexpr int kCacheRetentionDays = 14;
constexpr int kHttpTimeoutMs = 15000;
constexpr int kHttpMaxRetries = 3;
constexpr int kLogMaxBytes = 512 * 1024;
constexpr int kLogMaxFiles = 3;

constexpr const char* kAuthoritativeTimezone = "Europe/Istanbul";
constexpr int kIstanbulOffsetSeconds = 3 * 3600;
constexpr int kDailyCacheCheckHour = 3;
constexpr int kDailyCacheCheckMinute = 10;

constexpr const char* kDefaultCountry = "Turkey";
constexpr const char* kDefaultCity = "Antalya";
constexpr const char* kDefaultTimezone = "Europe/Istanbul";
constexpr double kDefaultLatitude = 36.8969;
constexpr double kDefaultLongitude = 30.6966;

constexpr const char* kDefaultAladhanEndpoint = "https://api.aladhan.com/v1/timings";
constexpr const char* kDefaultIslamicFinderEndpoint =
    "https://www.islamicfinder.us/index.php/api/prayer_times";

constexpr int kThresholdOptions[] = {30, 60, 120};
constexpr int kThresholdOptionCount = 3;

}  // namespace adhan
