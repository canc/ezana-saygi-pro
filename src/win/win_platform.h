#pragma once

#include <string>

#include "core/types.h"

namespace adhan {

HttpClient* create_win_http_client();
VolumeController* create_win_volume_controller();
std::string win_appdata_root();
void win_enable_dpi_aware();

}  // namespace adhan
