#pragma once

#include <string>
#include <vector>

namespace adhan {

bool read_file(const std::string& path, std::string* out);
bool write_file_atomic(const std::string& path, const std::string& data);
bool file_exists(const std::string& path);
bool mkdir_p(const std::string& path);
bool remove_file(const std::string& path);
std::vector<std::string> list_files(const std::string& dir);
std::string join_path(const std::string& a, const std::string& b);
std::string parent_path(const std::string& path);

}  // namespace adhan
