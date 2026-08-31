#include "logger.h"

#include <cstdio>
#include <cstring>

#include "constants.h"
#include "fsutil.h"
#include "timezone.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static CRITICAL_SECTION g_log_cs;
static bool g_log_cs_init = false;
#else
#include <pthread.h>
static pthread_mutex_t g_log_mu = PTHREAD_MUTEX_INITIALIZER;
#endif

namespace adhan {
namespace {

void lock_log() {
#ifdef _WIN32
  if (!g_log_cs_init) {
    InitializeCriticalSection(&g_log_cs);
    g_log_cs_init = true;
  }
  EnterCriticalSection(&g_log_cs);
#else
  pthread_mutex_lock(&g_log_mu);
#endif
}

void unlock_log() {
#ifdef _WIN32
  LeaveCriticalSection(&g_log_cs);
#else
  pthread_mutex_unlock(&g_log_mu);
#endif
}

}  // namespace

Logger::Logger(const std::string& log_dir) : dir_(log_dir) {
  mkdir_p(log_dir);
  path_ = join_path(log_dir, "adhanvolume.log");
}

void Logger::rotate_if_needed() {
  std::string data;
  if (!read_file(path_, &data)) return;
  if (static_cast<int>(data.size()) < kLogMaxBytes) return;
  std::string p3 = join_path(dir_, "adhanvolume.log.3");
  std::string p2 = join_path(dir_, "adhanvolume.log.2");
  std::string p1 = join_path(dir_, "adhanvolume.log.1");
  remove_file(p3);
#ifdef _WIN32
  MoveFileA(p2.c_str(), p3.c_str());
  MoveFileA(p1.c_str(), p2.c_str());
  MoveFileA(path_.c_str(), p1.c_str());
#else
  rename(p2.c_str(), p3.c_str());
  rename(p1.c_str(), p2.c_str());
  rename(path_.c_str(), p1.c_str());
#endif
}

void Logger::write(const char* level, const std::string& msg) {
  lock_log();
  rotate_if_needed();
  int64_t now = SystemClock().now_unix();
  int y, m, d, h, mi, s;
  unix_to_civil_utc(now, &y, &m, &d, &h, &mi, &s);
  char header[80];
  std::snprintf(header, sizeof(header), "%04d-%02d-%02d %02d:%02d:%02dZ %s  ", y, m, d, h, mi, s,
                level);
#ifdef _WIN32
  FILE* f = fopen(path_.c_str(), "ab");
#else
  FILE* f = std::fopen(path_.c_str(), "ab");
#endif
  if (f) {
    std::fputs(header, f);
    std::fwrite(msg.data(), 1, msg.size(), f);
    std::fputc('\n', f);
    std::fclose(f);
  }
  unlock_log();
}

void Logger::info(const std::string& msg) { write("INFO ", msg); }
void Logger::warn(const std::string& msg) { write("WARN ", msg); }
void Logger::error(const std::string& msg) { write("ERROR", msg); }
void Logger::debug(const std::string& msg) { write("DEBUG", msg); }

}  // namespace adhan
