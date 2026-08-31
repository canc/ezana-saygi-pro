#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <sys/stat.h>

#include "core/cache_manager.h"
#include "core/config.h"
#include "core/fade.h"
#include "core/fsutil.h"
#include "core/json.h"
#include "core/locations.h"
#include "core/logger.h"
#include "core/provider.h"
#include "core/schedule.h"
#include "core/scheduler.h"
#include "core/timezone.h"

using namespace adhan;

static int g_fails = 0;
static int g_pass = 0;

#define CHECK(cond)                                                             \
  do {                                                                          \
    if (!(cond)) {                                                              \
      std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);      \
      ++g_fails;                                                                \
    } else {                                                                    \
      ++g_pass;                                                                 \
    }                                                                           \
  } while (0)

#define CHECK_EQ(a, b)                                                          \
  do {                                                                          \
    if ((a) != (b)) {                                                           \
      std::fprintf(stderr, "FAIL %s:%d: %s == %s (%lld != %lld)\n", __FILE__,    \
                   __LINE__, #a, #b, (long long)(a), (long long)(b));           \
      ++g_fails;                                                                \
    } else {                                                                    \
      ++g_pass;                                                                 \
    }                                                                           \
  } while (0)

struct FakeClock : Clock {
  int64_t t;
  explicit FakeClock(int64_t unix_s) : t(unix_s) {}
  int64_t now_unix() override { return t; }
  int64_t now_ms() override { return t * 1000; }
};

struct FakeHttp : HttpClient {
  int calls;
  bool fail;
  int status;
  std::string body;
  std::string last_url;
  FakeHttp() : calls(0), fail(false), status(200) {}
  HttpResult get(const std::string& url, int) override {
    ++calls;
    last_url = url;
    HttpResult r;
    r.ok = !fail;
    r.status = fail ? 0 : status;
    r.body = fail ? "" : body;
    r.error = fail ? "network down" : "";
    return r;
  }
};

struct FakeVolume : VolumeController {
  float vol;
  bool mute;
  int sets;
  FakeVolume() : vol(0.65f), mute(false), sets(0) {}
  bool get_master_volume(float* v) override {
    *v = vol;
    return true;
  }
  bool set_master_volume(float v) override {
    vol = v;
    ++sets;
    return true;
  }
  bool get_mute(bool* m) override {
    *m = mute;
    return true;
  }
};

struct FakeProvider : PrayerTimeProvider {
  int calls;
  bool fail;
  bool invalid;
  FakeProvider() : calls(0), fail(false), invalid(false) {}
  const char* name() const override { return "Fake"; }
  bool fetch_daily(const Location& loc, const CalendarDate& date, int, PrayerSchedule* out,
                   std::string* err) override {
    ++calls;
    if (fail) {
      if (err) *err = "unavailable";
      return false;
    }
    PrayerSchedule s;
    s.location = loc;
    s.cache_date_istanbul = date;
    s.local_date = date;
    struct T {
      PrayerId id;
      int h, m;
    };
    const T times[] = {{PRAYER_FAJR, 5, 0},     {PRAYER_SUNRISE, 6, 20}, {PRAYER_DHUHR, 13, 3},
                       {PRAYER_ASR, 16, 41},    {PRAYER_MAGHRIB, 19, 23}, {PRAYER_ISHA, 20, 53}};
    for (size_t i = 0; i < sizeof(times) / sizeof(times[0]); ++i) {
      s.prayers[times[i].id].id = times[i].id;
      s.prayers[times[i].id].valid = true;
      s.prayers[times[i].id].hour = times[i].h;
      s.prayers[times[i].id].minute = times[i].m;
    }
    if (invalid) s.prayers[PRAYER_MAGHRIB].valid = false;
    s.source = "Fake";
    s.version = kScheduleVersion;
    s.fetched_at_unix = 1;
    if (!fill_unix_times(&s)) {
      if (err) *err = "invalid fake schedule";
      return false;
    }
    *out = s;
    return true;
  }
};

static Location antalya() {
  Location loc;
  loc.country = "Turkey";
  loc.city = "Antalya";
  loc.timezone = "Europe/Istanbul";
  loc.latitude = 36.8969;
  loc.longitude = 30.6966;
  return loc;
}

static Location istanbul_city() {
  Location loc;
  loc.country = "Turkey";
  loc.city = "Istanbul";
  loc.timezone = "Europe/Istanbul";
  loc.latitude = 41.0082;
  loc.longitude = 28.9784;
  return loc;
}

static std::string make_tmpdir() {
  char buf[] = "/tmp/adhanvolXXXXXX";
  char* p = mkdtemp(buf);
  if (!p) {
    std::perror("mkdtemp");
    std::exit(1);
  }
  return p;
}

static void noop_sleep(int) {}

// Known values (UTC):
// 2026-08-31 03:10 Istanbul = 2026-08-31 00:10 UTC = 1788135000
// 2026-09-01 03:10 Istanbul = 1788221400
// 2026-08-31 10:00 Istanbul = 1788159600
// 2026-08-31 19:23 Istanbul = 1788193380
// 2026-08-31 08:00 Istanbul = 1788152400
constexpr int64_t k0310 = 1788135000;
constexpr int64_t k0310_next = 1788221400;
constexpr int64_t k1000_ist = 1788159600;
constexpr int64_t k0800_ist = 1788152400;
constexpr int64_t k1923_ist = 1788193380;
constexpr int64_t k0300_ist = 1788134400;  // 03:00 Istanbul = 00:00 UTC

static void test_timezone() {
  CalendarDate d = istanbul_date(k0310);
  CHECK_EQ(d.year, 2026);
  CHECK_EQ(d.month, 8);
  CHECK_EQ(d.day, 31);
  CHECK_EQ(today_istanbul_0310(k0310), k0310);
  CHECK_EQ(next_istanbul_0310(k0310), k0310_next);  // exactly 03:10 -> tomorrow
  CHECK_EQ(next_istanbul_0310(k0310 - 1), k0310);
  CHECK_EQ(next_istanbul_0310(k1000_ist), k0310_next);

  // 21:00 UTC 30 Aug = 00:00 Istanbul 31 Aug
  CalendarDate midnight = istanbul_date(1788123600);
  CHECK_EQ(midnight.day, 31);
  CHECK_EQ(utc_date(1788123600).day, 30);

  int64_t maghrib = istanbul_local_to_unix(d, 19, 23, 0);
  CHECK_EQ(maghrib, k1923_ist);

  CHECK_EQ(timezone_offset_seconds("Europe/Istanbul", k1000_ist), 3 * 3600);
  CHECK_EQ(timezone_offset_seconds("UTC", k1000_ist), 0);

  int h, m;
  CHECK(parse_hhmm("19:23", &h, &m) && h == 19 && m == 23);
  CHECK(parse_hhmm("04:56 (EEST)", &h, &m) && h == 4 && m == 56);
  CHECK(!parse_hhmm("25:00", &h, &m));

  std::string disp = timezone_display("Europe/Istanbul", k1000_ist);
  CHECK(disp.find("GMT+3") != std::string::npos);
}

static void test_fade() {
  bool done = false;
  CHECK(std::fabs(fade_volume(0.67f, 0.f, 0, 4000, &done) - 0.67f) < 0.001f);
  CHECK(!done);
  float mid = fade_volume(0.67f, 0.f, 2000, 4000, &done);
  CHECK(std::fabs(mid - 0.335f) < 0.01f);
  CHECK(std::fabs(fade_volume(0.f, 0.67f, 4000, 4000, &done) - 0.67f) < 0.001f);
  CHECK(done);
}

static void test_aladhan_parse() {
  FakeHttp http;
  http.body =
      "{\"code\":200,\"status\":\"OK\",\"data\":{\"timings\":{\"Fajr\":\"04:56\",\"Sunrise\":\"06:"
      "20\",\"Dhuhr\":\"13:03\",\"Asr\":\"16:41\",\"Sunset\":\"19:34\",\"Maghrib\":\"19:34\","
      "\"Isha\":\"20:53\"},\"date\":{\"gregorian\":{\"date\":\"31-08-2026\",\"year\":\"2026\","
      "\"month\":{\"number\":8},\"day\":\"31\"}},\"meta\":{\"timezone\":\"Europe/"
      "Istanbul\",\"latitude\":36.8969,\"longitude\":30.6966}}}";
  AladhanProvider p(&http, kDefaultAladhanEndpoint);
  PrayerSchedule s;
  std::string err;
  CalendarDate d;
  d.year = 2026;
  d.month = 8;
  d.day = 31;
  CHECK(p.fetch_daily(antalya(), d, 1000, &s, &err));
  CHECK(s.valid());
  CHECK_EQ(s.prayers[PRAYER_MAGHRIB].hour, 19);
  CHECK_EQ(s.prayers[PRAYER_MAGHRIB].minute, 34);
  CHECK(s.prayers[PRAYER_SUNRISE].valid);
  CHECK(http.last_url.find("https://") == 0);
  CHECK(http.last_url.find("method=13") != std::string::npos);

  http.body = "{\"not\":\"a schedule\"}";
  CHECK(!p.fetch_daily(antalya(), d, 1000, &s, &err));
}

static CacheManager make_cache(const std::string& root, PrayerTimeProvider* p, Logger* log) {
  CacheManager c(root, p, log);
  c.set_max_retries(1);
  c.set_sleeper(noop_sleep);
  return c;
}

static void test_cache_scenarios() {
  std::string root = make_tmpdir();
  Logger log(root + "/logs");
  FakeProvider prov;
  Location loc = antalya();

  // Scenario 1 — first launch at 10:00, no cache -> 1 API
  {
    CacheManager c = make_cache(root, &prov, &log);
    CacheEnsureResult r = c.ensure_today(loc, k1000_ist, false);
    CHECK(r.have_schedule);
    CHECK(r.did_api_request);
    CHECK(!r.used_cache);
    CHECK_EQ(prov.calls, 1);
  }

  // Scenario 2 — restart same day, cache exists -> 0 API
  int calls_after_first = prov.calls;
  for (int i = 0; i < 5; ++i) {
    CacheManager c = make_cache(root, &prov, &log);
    CacheEnsureResult r = c.ensure_today(loc, k1000_ist, false);
    CHECK(r.have_schedule);
    CHECK(r.used_cache);
    CHECK(!r.did_api_request);
  }
  CHECK_EQ(prov.calls, calls_after_first);

  // peek_today also must not call API
  {
    CacheManager c = make_cache(root, &prov, &log);
    PrayerSchedule s;
    CHECK(c.peek_today(loc, k1000_ist, &s));
    CHECK_EQ(prov.calls, calls_after_first);
  }

  // Scenario 3 — 03:10 with existing cache -> 0 API
  {
    // Start before 03:10 so next_0310 is today's 03:10, cache already exists.
    CacheManager c = make_cache(root, &prov, &log);
    c.ensure_today(loc, k0300_ist, false);  // 03:00, cache hit
    int before = prov.calls;
    CacheEnsureResult r = c.on_tick(loc, k0310);
    CHECK(r.have_schedule);
    CHECK(!r.did_api_request);
    CHECK_EQ(prov.calls, before);
    CHECK_EQ(c.next_0310_unix(), k0310_next);
  }

  // Scenario 4 — 03:10 without cache -> 1 API
  {
    std::string root2 = make_tmpdir();
    Logger log2(root2 + "/logs");
    FakeProvider prov2;
    CacheManager c = make_cache(root2, &prov2, &log2);
    c.ensure_today(loc, k0300_ist, false);  // creates cache (1 call)
    // Delete today's cache file.
    std::string fn = CacheManager::make_cache_filename(loc, istanbul_date(k0300_ist));
    remove_file(root2 + "/cache/" + fn);
    int before = prov2.calls;
    CacheEnsureResult r = c.on_tick(loc, k0310);
    CHECK(r.have_schedule);
    CHECK(r.did_api_request);
    CHECK_EQ(prov2.calls, before + 1);
  }

  // Scenario 5/6 — Windows TZ must not matter; dates come from unix+Istanbul.
  {
    CHECK_EQ(istanbul_date(k0310).day, 31);
    CHECK_EQ(next_istanbul_0310(k0310 - 60), k0310);
  }

  // Scenario 7 — sleep through 03:10, wake at 08:00
  {
    std::string root3 = make_tmpdir();
    Logger log3(root3 + "/logs");
    FakeProvider prov3;
    CacheManager c = make_cache(root3, &prov3, &log3);
    c.ensure_today(loc, k0300_ist, false);  // cache created
    int before = prov3.calls;
    // Sleep: never fire 03:10. Wake 08:00.
    CacheEnsureResult r = c.on_resume(loc, k0800_ist);
    CHECK(r.have_schedule);
    CHECK(!r.did_api_request);  // cache present
    CHECK_EQ(prov3.calls, before);
    CHECK_EQ(c.next_0310_unix(), k0310_next);
  }
  {
    std::string root3 = make_tmpdir();
    Logger log3(root3 + "/logs");
    FakeProvider prov3;
    CacheManager c = make_cache(root3, &prov3, &log3);
    // No cache, wake at 08:00 after sleeping through 03:10.
    CacheEnsureResult r = c.on_resume(loc, k0800_ist);
    CHECK(r.have_schedule);
    CHECK(r.did_api_request);
    CHECK_EQ(prov3.calls, 1);
  }

  // Scenario 8 — start after 03:10, no cache -> immediate API, do not wait
  {
    std::string root4 = make_tmpdir();
    Logger log4(root4 + "/logs");
    FakeProvider prov4;
    CacheManager c = make_cache(root4, &prov4, &log4);
    CacheEnsureResult r = c.ensure_today(loc, k0800_ist, false);
    CHECK(r.did_api_request);
    CHECK_EQ(prov4.calls, 1);
    CHECK_EQ(c.next_0310_unix(), k0310_next);  // next check is tomorrow
  }

  // Scenario 9 — location change
  {
    FakeProvider prov5;
    std::string root5 = make_tmpdir();
    Logger log5(root5 + "/logs");
    CacheManager c = make_cache(root5, &prov5, &log5);
    CHECK(c.ensure_today(loc, k1000_ist, false).did_api_request);
    CHECK_EQ(prov5.calls, 1);
    Location ist = istanbul_city();
    CHECK(c.ensure_today(ist, k1000_ist, false).did_api_request);
    CHECK_EQ(prov5.calls, 2);
    // Switch back to Antalya: cache exists
    CacheEnsureResult back = c.ensure_today(loc, k1000_ist, false);
    CHECK(back.used_cache);
    CHECK(!back.did_api_request);
    CHECK_EQ(prov5.calls, 2);
  }

  // Scenario 10 — API failure, no cache: no fabricated schedule
  {
    FakeProvider prov6;
    prov6.fail = true;
    std::string root6 = make_tmpdir();
    Logger log6(root6 + "/logs");
    CacheManager c = make_cache(root6, &prov6, &log6);
    CacheEnsureResult r = c.ensure_today(loc, k1000_ist, false);
    CHECK(!r.have_schedule);
    CHECK(r.did_api_request);
  }

  // Invalid API response discarded; previous cache kept
  {
    FakeProvider prov7;
    std::string root7 = make_tmpdir();
    Logger log7(root7 + "/logs");
    CacheManager c = make_cache(root7, &prov7, &log7);
    c.ensure_today(loc, k1000_ist, false);
    prov7.invalid = true;
    CacheEnsureResult r = c.ensure_today(loc, k1000_ist, true);
    CHECK(r.have_schedule);
    CHECK(r.used_cache);  // kept previous
  }

  // Scheduler must not be able to call API: CacheManager on_tick without due 03:10
  {
    int before = prov.calls;
    CacheManager c = make_cache(root, &prov, &log);
    c.ensure_today(loc, k1000_ist, false);  // cache hit
    c.on_tick(loc, k1000_ist);              // 10:00, 03:10 already passed, next is tomorrow
    CHECK_EQ(prov.calls, before);
  }
}

static void test_scheduler() {
  std::string root = make_tmpdir();
  Logger log(root + "/logs");
  FakeVolume vol;
  vol.vol = 0.65f;
  FakeProvider prov;
  Location loc = antalya();
  CalendarDate d = istanbul_date(k1923_ist);
  PrayerSchedule s;
  std::string err;
  CHECK(prov.fetch_daily(loc, d, 0, &s, &err));

  AppConfig cfg = default_config();
  cfg.threshold_seconds = 60;
  cfg.adhan_duration_seconds = DEFAULT_ADHAN_DURATION_SECONDS;
  cfg.fade_duration_ms = 4000;
  cfg.enabled = true;

  Scheduler sch(&vol, &log, root);
  sch.set_config(cfg);
  sch.set_schedule(s, true);

  // 19:21 — before threshold (19:22)
  int64_t t1921 = (k1923_ist - 120) * 1000;
  sch.evaluate(t1921);
  CHECK(std::fabs(vol.vol - 0.65f) < 0.001f);
  CHECK(sch.status().state == ST_IDLE || sch.status().state == ST_WAITING_FOR_THRESHOLD);
  CHECK(sch.status().next_prayer_name == "Maghrib");

  // 19:22 fade-out starts
  int64_t t1922 = (k1923_ist - 60) * 1000;
  sch.evaluate(t1922);
  CHECK(sch.status().state == ST_FADING_OUT);
  CHECK(std::fabs(vol.vol - 0.65f) < 0.02f);  // just started

  // 19:22:04 volume 0
  sch.evaluate(t1922 + 4000);
  CHECK(sch.status().state == ST_MUTED);
  CHECK(vol.vol < 0.01f);

  // 19:23 prayer time — still muted
  sch.evaluate(k1923_ist * 1000);
  CHECK(sch.status().state == ST_MUTED);
  CHECK(vol.vol < 0.01f);

  // User raises volume during mute -> forced back to 0
  vol.vol = 0.40f;
  sch.evaluate(k1923_ist * 1000 + 1000);
  CHECK(vol.vol < 0.01f);

  // 19:26 fade-in starts (prayer + 180s)
  int64_t t1926 = (k1923_ist + 180) * 1000;
  sch.evaluate(t1926);
  CHECK(sch.status().state == ST_FADING_IN || sch.status().state == ST_RESTORED);

  sch.evaluate(t1926 + 4000);
  CHECK(std::fabs(vol.vol - 0.65f) < 0.02f);
  CHECK(sch.status().state == ST_IDLE);

  // Duplicate: evaluating again must not re-fade
  int sets = vol.sets;
  sch.evaluate(t1926 + 5000);
  CHECK(std::fabs(vol.vol - 0.65f) < 0.02f);
  CHECK_EQ(vol.sets, sets);

  // Disabled: no action
  FakeVolume vol2;
  vol2.vol = 0.65f;
  Scheduler sch2(&vol2, &log, make_tmpdir());
  cfg.enabled = false;
  sch2.set_config(cfg);
  sch2.set_schedule(s, true);
  sch2.evaluate(t1922);
  CHECK(std::fabs(vol2.vol - 0.65f) < 0.001f);

  // Volume already 0
  FakeVolume vol3;
  vol3.vol = 0;
  cfg.enabled = true;
  Scheduler sch3(&vol3, &log, make_tmpdir());
  sch3.set_config(cfg);
  sch3.set_schedule(s, true);
  sch3.evaluate(t1922);
  CHECK(vol3.vol < 0.01f);
  sch3.evaluate(t1926 + 4000);
  CHECK(vol3.vol < 0.01f);

  // Wake during Adhan window (19:24)
  FakeVolume vol4;
  vol4.vol = 0.65f;
  Scheduler sch4(&vol4, &log, make_tmpdir());
  sch4.set_config(cfg);
  sch4.set_schedule(s, true);
  sch4.evaluate((k1923_ist + 60) * 1000);  // 19:24
  CHECK(vol4.vol < 0.01f);
  CHECK(sch4.status().state == ST_MUTED);

  // Disable while muted restores original
  sch4.set_enabled(false, (k1923_ist + 90) * 1000);
  CHECK(std::fabs(vol4.vol - 0.65f) < 0.02f);

  // Sunrise must not trigger
  CHECK(!prayer_triggers_volume(PRAYER_SUNRISE));

  // No schedule: do nothing
  FakeVolume vol5;
  vol5.vol = 0.5f;
  Scheduler sch5(&vol5, &log, make_tmpdir());
  cfg.enabled = true;
  sch5.set_config(cfg);
  sch5.set_schedule(PrayerSchedule(), false);
  sch5.evaluate(t1922);
  CHECK(std::fabs(vol5.vol - 0.5f) < 0.001f);

  // Mute preserved: we never call set_mute (FakeVolume has no set_mute)
  CHECK(vol4.mute == false);
}

static void test_config_roundtrip() {
  std::string root = make_tmpdir();
  AppConfig c = default_config();
  c.city = "Ankara";
  c.latitude = 39.9334;
  c.longitude = 32.8597;
  c.threshold_seconds = 180;
  CHECK(save_config(root + "/config.json", c));
  AppConfig d;
  std::string err;
  CHECK(load_config(root + "/config.json", &d, &err));
  CHECK(d.city == "Ankara");
  CHECK_EQ(d.threshold_seconds, 180);
  CHECK_EQ(d.adhan_duration_seconds, DEFAULT_ADHAN_DURATION_SECONDS);
}

static void test_cache_key() {
  Location loc = antalya();
  CalendarDate d;
  d.year = 2026;
  d.month = 8;
  d.day = 31;
  std::string key = CacheManager::make_cache_key(loc, d);
  CHECK(key == "Europe/Istanbul:36.8969:30.6966:2026-08-31");
  // Tiny float jitter must normalize to the same key.
  Location loc2 = loc;
  loc2.latitude = 36.89690001;
  CHECK(CacheManager::make_cache_key(loc2, d) == key);
}

int main() {
  test_timezone();
  test_fade();
  test_aladhan_parse();
  test_cache_key();
  test_config_roundtrip();
  test_cache_scenarios();
  test_scheduler();
  std::printf("%d passed, %d failed\n", g_pass, g_fails);
  return g_fails ? 1 : 0;
}
