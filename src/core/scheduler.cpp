#include "scheduler.h"

#include <cmath>
#include <cstdio>

#include "fade.h"
#include "fsutil.h"
#include "json.h"
#include "schedule.h"
#include "timezone.h"

namespace adhan {
namespace {

bool near_zero(float v) { return v < 0.005f; }

std::string event_to_json(const PrayerEvent& e) {
  Json j = Json::object();
  j["id"] = Json::string(e.id);
  j["prayer"] = Json::string(prayer_name(e.prayer));
  j["prayer_unix"] = Json::number(static_cast<double>(e.prayer_unix));
  j["threshold_seconds"] = Json::number(e.threshold_seconds);
  j["adhan_duration_seconds"] = Json::number(e.adhan_duration_seconds);
  j["fade_duration_ms"] = Json::number(e.fade_duration_ms);
  j["original_volume"] = Json::number(e.original_volume);
  j["original_mute"] = Json::boolean(e.original_mute);
  j["captured"] = Json::boolean(e.captured);
  j["state"] = Json::string(event_state_name(e.state));
  j["fade_out_start_ms"] = Json::number(static_cast<double>(e.fade_out_start_ms));
  j["fade_out_end_ms"] = Json::number(static_cast<double>(e.fade_out_end_ms));
  j["fade_in_start_ms"] = Json::number(static_cast<double>(e.fade_in_start_ms));
  j["fade_in_end_ms"] = Json::number(static_cast<double>(e.fade_in_end_ms));
  return j.stringify(true);
}

bool event_from_json(const std::string& text, PrayerEvent* e) {
  Json j;
  std::string err;
  if (!Json::parse(text, &j, &err) || !j.is_object()) return false;
  e->id = j.get("id").as_string();
  PrayerId id;
  if (!prayer_id_from_name(j.get("prayer").as_string(), &id)) return false;
  e->prayer = id;
  e->prayer_unix = j.get("prayer_unix").as_int64(0);
  e->threshold_seconds = j.get("threshold_seconds").as_int(DEFAULT_THRESHOLD_SECONDS);
  e->adhan_duration_seconds = j.get("adhan_duration_seconds").as_int(DEFAULT_ADHAN_DURATION_SECONDS);
  e->fade_duration_ms = j.get("fade_duration_ms").as_int(DEFAULT_FADE_DURATION_MS);
  e->original_volume = static_cast<float>(j.get("original_volume").as_number(0));
  e->original_mute = j.get("original_mute").as_bool(false);
  e->captured = j.get("captured").as_bool(false);
  e->fade_out_start_ms = j.get("fade_out_start_ms").as_int64(0);
  e->fade_out_end_ms = j.get("fade_out_end_ms").as_int64(0);
  e->fade_in_start_ms = j.get("fade_in_start_ms").as_int64(0);
  e->fade_in_end_ms = j.get("fade_in_end_ms").as_int64(0);
  e->state = ST_IDLE;
  return !e->id.empty();
}

}  // namespace

Scheduler::Scheduler(VolumeController* volume, Logger* logger, std::string state_dir)
    : vol_(volume),
      log_(logger),
      state_dir_(state_dir),
      has_schedule_(false),
      has_active_(false) {
  cfg_ = default_config();
  active_.state = ST_IDLE;
  mkdir_p(state_dir_);
  load_processed();
  status_.state = ST_IDLE;
  status_.enabled = cfg_.enabled;
  status_.has_schedule = false;
  status_.next_prayer_unix = 0;
  status_.original_volume = 0;
  status_.original_mute = false;
}

void Scheduler::set_config(const AppConfig& cfg) { cfg_ = cfg; }

void Scheduler::set_schedule(const PrayerSchedule& s, bool valid) {
  has_schedule_ = valid && s.valid();
  if (has_schedule_) schedule_ = s;
}

void Scheduler::set_enabled(bool on, int64_t now_ms) {
  cfg_.enabled = on;
  if (!on) abort_and_restore(now_ms, "disabled");
  evaluate(now_ms);
}

void Scheduler::on_location_changing(int64_t now_ms) {
  abort_and_restore(now_ms, "location changed");
  has_schedule_ = false;
}

void Scheduler::clear_active() {
  has_active_ = false;
  active_ = PrayerEvent();
  active_.state = ST_IDLE;
  clear_persisted_active();
}

void Scheduler::mark_processed(const std::string& id) {
  processed_.insert(id);
  save_processed();
}

void Scheduler::apply_volume(float v) {
  if (v < 0) v = 0;
  if (v > 1) v = 1;
  if (vol_) vol_->set_master_volume(v);
}

bool Scheduler::capture_volume() {
  if (active_.captured) return true;
  if (!vol_) return false;
  float v = 0;
  bool muted = false;
  if (!vol_->get_master_volume(&v)) {
    if (log_) log_->error("Failed to read master volume");
    return false;
  }
  vol_->get_mute(&muted);
  active_.original_volume = v;
  active_.original_mute = muted;
  active_.captured = true;
  if (log_) {
    char buf[80];
    std::snprintf(buf, sizeof(buf), "Original volume: %.0f%%", v * 100.0f);
    log_->info(buf);
    if (muted) log_->info("System already muted; mute state will be preserved");
  }
  persist_active();
  return true;
}

void Scheduler::abort_and_restore(int64_t now_ms, const char* reason) {
  (void)now_ms;
  if (has_active_ && active_.captured) {
    if (log_) log_->info(std::string("Aborting volume event (") + reason + "); restoring volume");
    apply_volume(active_.original_volume);
    mark_processed(active_.id);
  }
  clear_active();
}

PrayerEvent Scheduler::make_event(PrayerId id) const {
  PrayerEvent e;
  const PrayerOccurrence& p = schedule_.prayers[id];
  e.id = make_event_id(schedule_, id);
  e.prayer = id;
  e.prayer_unix = p.unix_utc;
  e.threshold_seconds = cfg_.threshold_seconds;
  e.adhan_duration_seconds = cfg_.adhan_duration_seconds;
  e.fade_duration_ms = cfg_.fade_duration_ms;
  e.original_volume = 0;
  e.original_mute = false;
  e.captured = false;
  e.state = ST_WAITING_FOR_THRESHOLD;
  e.fade_out_start_ms = (p.unix_utc - cfg_.threshold_seconds) * 1000LL;
  e.fade_out_end_ms = e.fade_out_start_ms + cfg_.fade_duration_ms;
  e.fade_in_start_ms = (p.unix_utc + cfg_.adhan_duration_seconds) * 1000LL;
  e.fade_in_end_ms = e.fade_in_start_ms + cfg_.fade_duration_ms;
  return e;
}

void Scheduler::update_active(int64_t now_ms) {
  EventState next = ST_WAITING_FOR_THRESHOLD;
  if (now_ms >= active_.fade_in_end_ms)
    next = ST_RESTORED;
  else if (now_ms >= active_.fade_in_start_ms)
    next = ST_FADING_IN;
  else if (now_ms >= active_.fade_out_end_ms)
    next = ST_MUTED;
  else if (now_ms >= active_.fade_out_start_ms)
    next = ST_FADING_OUT;
  else
    next = ST_WAITING_FOR_THRESHOLD;

  if (next == ST_WAITING_FOR_THRESHOLD) {
    active_.state = ST_WAITING_FOR_THRESHOLD;
    return;
  }

  if (!active_.captured) {
    if (!capture_volume()) return;
  }

  if (near_zero(active_.original_volume) && next != ST_RESTORED) {
    apply_volume(0);
    active_.state = (next == ST_FADING_IN) ? ST_FADING_IN : ST_MUTED;
    if (next == ST_FADING_OUT || next == ST_MUTED) persist_active();
    return;
  }

  if (next == ST_FADING_OUT) {
    if (active_.state != ST_FADING_OUT) {
      if (log_) log_->info(std::string("Fade-out started (") + prayer_name(active_.prayer) + ")");
    }
    bool done = false;
    float v = fade_volume(active_.original_volume, 0.0f,
                          static_cast<double>(now_ms - active_.fade_out_start_ms),
                          static_cast<double>(active_.fade_duration_ms), &done);
    apply_volume(v);
    active_.state = ST_FADING_OUT;
    if (done) {
      apply_volume(0);
      active_.state = ST_MUTED;
      if (log_) log_->info("Volume reached 0%");
    }
    persist_active();
    return;
  }

  if (next == ST_MUTED) {
    if (active_.state != ST_MUTED) {
      apply_volume(0);
      if (log_) log_->info("Volume held at 0% during Adhan window");
    } else {
      float cur = 0;
      if (vol_ && vol_->get_master_volume(&cur) && !near_zero(cur)) {
        apply_volume(0);  // user raised volume during muted period
      }
    }
    active_.state = ST_MUTED;
    persist_active();
    return;
  }

  if (next == ST_FADING_IN) {
    if (active_.state != ST_FADING_IN) {
      if (log_) log_->info("Fade-in started");
    }
    bool done = false;
    float v = fade_volume(0.0f, active_.original_volume,
                          static_cast<double>(now_ms - active_.fade_in_start_ms),
                          static_cast<double>(active_.fade_duration_ms), &done);
    apply_volume(v);
    active_.state = ST_FADING_IN;
    if (done) next = ST_RESTORED;
    else {
      persist_active();
      return;
    }
  }

  if (next == ST_RESTORED) {
    apply_volume(active_.original_volume);
    if (log_) {
      char buf[80];
      std::snprintf(buf, sizeof(buf), "Volume restored: %.0f%%", active_.original_volume * 100.0f);
      log_->info(buf);
    }
    mark_processed(active_.id);
    active_.state = ST_RESTORED;
    clear_active();
  }
}

void Scheduler::consider_new_event(int64_t now_ms) {
  if (!has_schedule_) return;
  const PrayerId order[] = {PRAYER_FAJR, PRAYER_DHUHR, PRAYER_ASR, PRAYER_MAGHRIB, PRAYER_ISHA};
  PrayerEvent in_window;
  bool found_window = false;
  for (int i = 0; i < 5; ++i) {
    PrayerId id = order[i];
    if (!schedule_.prayers[id].valid) continue;
    if (processed_.count(make_event_id(schedule_, id))) continue;
    PrayerEvent e = make_event(id);
    if (now_ms >= e.fade_in_end_ms) {
      mark_processed(e.id);  // fully passed; never replay
      continue;
    }
    if (now_ms >= e.fade_out_start_ms) {
      in_window = e;
      found_window = true;
      break;
    }
  }
  if (found_window) {
    active_ = in_window;
    has_active_ = true;
    update_active(now_ms);
  }
}

void Scheduler::evaluate(int64_t now_ms) {
  if (!cfg_.enabled) {
    refresh_status(now_ms);
    return;
  }
  if (!has_schedule_) {
    refresh_status(now_ms);
    return;
  }
  if (has_active_) {
    update_active(now_ms);
  }
  if (!has_active_) {
    consider_new_event(now_ms);
  }
  refresh_status(now_ms);
}

void Scheduler::refresh_status(int64_t now_ms) {
  status_.enabled = cfg_.enabled;
  status_.has_schedule = has_schedule_;
  status_.state = has_active_ ? active_.state : ST_IDLE;
  status_.active_event_id = has_active_ ? active_.id : "";
  status_.original_volume = has_active_ ? active_.original_volume : 0;
  status_.original_mute = has_active_ ? active_.original_mute : false;
  status_.next_prayer_name.clear();
  status_.next_prayer_unix = 0;
  status_.message.clear();

  if (!cfg_.enabled) {
    status_.message = "Disabled";
    return;
  }
  if (!has_schedule_) {
    status_.message = "No prayer schedule";
    return;
  }

  int64_t now_unix = now_ms / 1000;
  const PrayerId order[] = {PRAYER_FAJR, PRAYER_DHUHR, PRAYER_ASR, PRAYER_MAGHRIB, PRAYER_ISHA};
  if (has_active_) {
    status_.next_prayer_name = prayer_name(active_.prayer);
    status_.next_prayer_unix = active_.prayer_unix;
    status_.message = event_state_name(active_.state);
    return;
  }
  for (int i = 0; i < 5; ++i) {
    const PrayerOccurrence& p = schedule_.prayers[order[i]];
    if (!p.valid) continue;
    if (p.unix_utc >= now_unix) {
      status_.next_prayer_name = prayer_name(order[i]);
      status_.next_prayer_unix = p.unix_utc;
      break;
    }
  }
  if (status_.next_prayer_name.empty())
    status_.message = "No remaining prayers today";
  else
    status_.message = "Waiting";
}

void Scheduler::persist_active() {
  if (!has_active_ || !active_.captured) return;
  write_file_atomic(join_path(state_dir_, "active_event.json"), event_to_json(active_));
}

void Scheduler::clear_persisted_active() {
  remove_file(join_path(state_dir_, "active_event.json"));
}

void Scheduler::load_processed() {
  std::string text;
  if (!read_file(join_path(state_dir_, "processed.json"), &text)) return;
  Json j;
  std::string err;
  if (!Json::parse(text, &j, &err)) return;
  const Json* arr = 0;
  if (j.type() == Json::ARRAY)
    arr = &j;
  else if (j.get("ids").type() == Json::ARRAY)
    arr = &j.get("ids");
  if (!arr) return;
  for (size_t i = 0; i < arr->size(); ++i) {
    std::string id = arr->at(i).as_string();
    if (!id.empty()) processed_.insert(id);
  }
}

void Scheduler::save_processed() {
  Json j = Json::object();
  Json ids = Json::array();
  for (std::set<std::string>::const_iterator it = processed_.begin(); it != processed_.end(); ++it) {
    ids.push(Json::string(*it));
  }
  j["ids"] = ids;
  write_file_atomic(join_path(state_dir_, "processed.json"), j.stringify(true));
}

void Scheduler::recover_on_startup(int64_t now_ms) {
  std::string text;
  std::string path = join_path(state_dir_, "active_event.json");
  if (!read_file(path, &text)) return;
  PrayerEvent e;
  if (!event_from_json(text, &e) || !e.captured) {
    remove_file(path);
    return;
  }
  if (now_ms >= e.fade_in_end_ms) {
    float cur = 0;
    bool got = vol_ && vol_->get_master_volume(&cur);
    if (got && near_zero(cur) && !near_zero(e.original_volume)) {
      if (log_) log_->info("Recovering unfinished event; restoring original volume");
      apply_volume(e.original_volume);
    } else {
      if (log_) log_->info("Unfinished event expired; leaving current volume unchanged");
    }
    mark_processed(e.id);
    remove_file(path);
    return;
  }
  // Still inside window: resume without recapturing.
  active_ = e;
  has_active_ = true;
  if (log_) log_->info("Resuming unfinished volume event");
  update_active(now_ms);
}

}  // namespace adhan
