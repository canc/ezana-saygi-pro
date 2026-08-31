#pragma once

#include <string>

#include "types.h"

namespace adhan {

bool load_config(const std::string& path, AppConfig* out, std::string* err);
bool save_config(const std::string& path, const AppConfig& cfg);

std::string config_to_json(const AppConfig& cfg);
bool config_from_json(const std::string& text, AppConfig* out, std::string* err);

}  // namespace adhan
