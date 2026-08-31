#pragma once

namespace adhan {

constexpr const char* kAppName = "Adhan Volume";
constexpr const char* kAppFolderName = "AdhanVolume";
constexpr const char* kVersion = "1.0.0";
constexpr const char* kMutexName = "Local\\AdhanVolumeSingleton";

constexpr int kConfigVersion = 1;
constexpr int kCacheVersion = 1;
constexpr int kScheduleVersion = 1;

constexpr int DEFAULT_ADHAN_DURATION_SECONDS = 180;
constexpr int DEFAULT_FADE_DURATION_MS = 4000;
constexpr int DEFAULT_THRESHOLD_SECONDS = 60;
constexpr int kSchedulerIdleIntervalMs = 20000;
constexpr int kFadeTickMs = 100;
constexpr int kMuteHoldTickMs = 1000;
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

constexpr int kThresholdOptions[] = {30, 60, 180, 300};
constexpr int kThresholdOptionCount = 4;

}  // namespace adhan
