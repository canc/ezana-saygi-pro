#pragma once

#include <set>
#include <string>

#include "logger.h"
#include "types.h"

namespace adhan {

struct SchedulerStatus {
  EventState state;
  bool enabled;
  bool has_schedule;
  std::string next_prayer_name;
  int64_t next_prayer_unix;
  std::string active_event_id;
  std::string message;
  float original_volume;
  bool original_mute;
};

class Scheduler {
 public:
  Scheduler(VolumeController* volume, Logger* logger, std::string state_dir);

  void set_config(const AppConfig& cfg);
  void set_schedule(const PrayerSchedule& s, bool valid);
  void set_enabled(bool on, int64_t now_ms);
  void set_debug(bool on) { debug_ = on; }
  void evaluate(int64_t now_ms);
  void on_location_changing(int64_t now_ms);
  void recover_on_startup(int64_t now_ms);

  const SchedulerStatus& status() const { return status_; }
  const PrayerEvent& active() const { return active_; }
  bool is_fading() const {
    return active_.state == ST_FADING_OUT || active_.state == ST_FADING_IN;
  }
  bool is_holding_mute() const { return active_.state == ST_MUTED; }

  // Adaptive poll: ~1s near an event, ~5s when soon, idle otherwise.
  int recommended_poll_interval_ms(int64_t now_ms) const;

 private:
  VolumeController* vol_;
  Logger* log_;
  std::string state_dir_;
  AppConfig cfg_;
  PrayerSchedule schedule_;
  bool has_schedule_;
  PrayerEvent active_;
  bool has_active_;
  std::set<std::string> processed_;
  SchedulerStatus status_;
  int64_t last_now_ms_;
  int64_t last_diag_log_ms_;
  EventState last_logged_state_;
  int last_logged_vol_pct_;
  int64_t last_capture_fail_log_ms_;
  bool debug_;
  bool logged_threshold_;
  bool logged_fade_start_;

  void clear_active();
  void abort_and_restore(int64_t now_ms, const char* reason);
  bool capture_volume();
  bool apply_volume(float v);
  void update_active(int64_t now_ms);
  void consider_new_event(int64_t now_ms);
  void refresh_status(int64_t now_ms);
  void persist_active();
  void clear_persisted_active();
  void load_processed();
  void save_processed();
  void mark_processed(const std::string& id);
  void allow_replay_if_window_open(int64_t now_ms);
  void handle_clock_discontinuity(int64_t now_ms);
  void log_eval(int64_t now_ms, const char* extra);
  std::string zone_name() const;
  std::string fmt_ms(int64_t epoch_ms) const;
  PrayerEvent make_event(PrayerId id) const;
  int64_t peek_next_fade_out_start_ms(int64_t now_ms) const;
};

}  // namespace adhan
