#pragma once

#include <string>

namespace adhan {

class Logger {
 public:
  explicit Logger(const std::string& log_dir);
  void info(const std::string& msg);
  void warn(const std::string& msg);
  void error(const std::string& msg);
  void debug(const std::string& msg);

 private:
  std::string path_;
  std::string dir_;
  void write(const char* level, const std::string& msg);
  void rotate_if_needed();
};

}  // namespace adhan
