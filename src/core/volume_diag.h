#pragma once

#include <string>

#include "types.h"
#include "logger.h"

namespace adhan {

// Direct VolumeController probe, independent of the prayer scheduler.
// Sequence: get → set 50% → verify → set 25% → verify → set 0% → verify → restore.
bool run_volume_self_test(VolumeController* vol, Logger* log, std::string* report);

}  // namespace adhan
