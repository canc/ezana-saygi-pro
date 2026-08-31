#include "volume_diag.h"

#include <cmath>
#include <cstdio>

#include "constants.h"

namespace adhan {
namespace {

void append_line(std::string* out, Logger* log, const std::string& line) {
  if (out) {
    *out += line;
    *out += '\n';
  }
  if (log) log->info(line);
}

bool step_set_verify(VolumeController* vol, Logger* log, std::string* out, float target) {
  char buf[160];
  std::snprintf(buf, sizeof(buf), "Setting master volume: %.0f%%", target * 100.0f);
  append_line(out, log, buf);
  if (!vol->set_master_volume(target)) {
    std::snprintf(buf, sizeof(buf), "Set master volume FAILED (%s): %s", vol->backend_name(),
                  vol->last_error());
    append_line(out, log, buf);
    return false;
  }
  float got = -1.0f;
  if (!vol->get_master_volume(&got)) {
    std::snprintf(buf, sizeof(buf), "Verify get FAILED (%s): %s", vol->backend_name(),
                  vol->last_error());
    append_line(out, log, buf);
    return false;
  }
  std::snprintf(buf, sizeof(buf), "Verified volume: %.0f%%", got * 100.0f);
  append_line(out, log, buf);
  if (std::fabs(got - target) > kVolumeVerifyEpsilon) {
    std::snprintf(buf, sizeof(buf), "Volume mismatch: wanted %.0f%% got %.0f%%", target * 100.0f,
                  got * 100.0f);
    append_line(out, log, buf);
    return false;
  }
  return true;
}

}  // namespace

bool run_volume_self_test(VolumeController* vol, Logger* log, std::string* report) {
  std::string local;
  std::string* out = report ? report : &local;
  if (report) report->clear();

  if (!vol) {
    append_line(out, log, "Volume self-test FAILED: no VolumeController");
    return false;
  }

  char buf[160];
  std::snprintf(buf, sizeof(buf), "Volume self-test starting (backend: %s)", vol->backend_name());
  append_line(out, log, buf);

  vol->refresh_endpoint();

  float original = 0;
  if (!vol->get_master_volume(&original)) {
    std::snprintf(buf, sizeof(buf), "Get current volume FAILED (%s): %s", vol->backend_name(),
                  vol->last_error());
    append_line(out, log, buf);
    return false;
  }
  std::snprintf(buf, sizeof(buf), "Current volume: %.0f%%", original * 100.0f);
  append_line(out, log, buf);

  bool muted = false;
  if (vol->get_mute(&muted)) {
    append_line(out, log, muted ? "Mute state: muted" : "Mute state: unmuted");
  }

  bool ok = step_set_verify(vol, log, out, 0.50f) && step_set_verify(vol, log, out, 0.25f) &&
            step_set_verify(vol, log, out, 0.00f);

  std::snprintf(buf, sizeof(buf), "Restoring original volume: %.0f%%", original * 100.0f);
  append_line(out, log, buf);
  if (!vol->set_master_volume(original)) {
    std::snprintf(buf, sizeof(buf), "Restore FAILED (%s): %s", vol->backend_name(), vol->last_error());
    append_line(out, log, buf);
    ok = false;
  } else {
    float got = -1.0f;
    if (vol->get_master_volume(&got)) {
      std::snprintf(buf, sizeof(buf), "Restored volume: %.0f%%", got * 100.0f);
      append_line(out, log, buf);
    }
  }

  append_line(out, log, ok ? "Volume self-test PASSED" : "Volume self-test FAILED");
  return ok;
}

}  // namespace adhan
