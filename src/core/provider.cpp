#include "provider.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <map>
#include <vector>

#include "json.h"
#include "locations.h"
#include "schedule.h"
#include "timezone.h"

namespace adhan {

std::string url_encode(const std::string& s) {
  std::string out;
  for (size_t i = 0; i < s.size(); ++i) {
    unsigned char c = static_cast<unsigned char>(s[i]);
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' ||
        c == '_' || c == '.' || c == '~') {
      out.push_back(static_cast<char>(c));
    } else if (c == ' ') {
      out.push_back('+');
    } else {
      char buf[8];
      std::snprintf(buf, sizeof(buf), "%%%02X", c);
      out.append(buf);
    }
  }
  return out;
}

int calculation_method_for_location(const Location& loc) {
  if (loc.country == "Turkey" || loc.timezone == "Europe/Istanbul") return 13;  // Diyanet
  if (loc.country == "Saudi Arabia") return 4;
  if (loc.country == "Egypt") return 5;
  if (loc.country == "United States" || loc.country == "Canada") return 2;
  return 3;  // Muslim World League
}

static const Json* find_named(const Json& obj, const char* const* names) {
  if (!names) return 0;
  for (int i = 0; names[i]; ++i) {
    if (obj.has(names[i]) && !obj.get(names[i]).is_null()) return &obj.get(names[i]);
  }
  return 0;
}

bool parse_prayer_timings_object(const Json& timings, PrayerSchedule* out, std::string* err) {
  struct Map {
    PrayerId id;
    const char* const* names;
    bool required;
  };
  static const char* kFajr[] = {"Fajr", "fajr", "Fajar", "fajar", "Imsak", "imsak", 0};
  static const char* kSunrise[] = {"Sunrise", "sunrise", 0};
  static const char* kDhuhr[] = {"Dhuhr", "dhuhr", "Zuhr", "zuhr", "Dhuhur", "dhuhur",
                                 "Dhuhar", "dhuhar", 0};
  static const char* kAsr[] = {"Asr", "asr", 0};
  static const char* kMaghrib[] = {"Maghrib", "maghrib", 0};
  static const char* kIsha[] = {"Isha", "isha", 0};
  const Map maps[] = {
      {PRAYER_FAJR, kFajr, true}, {PRAYER_SUNRISE, kSunrise, false}, {PRAYER_DHUHR, kDhuhr, true},
      {PRAYER_ASR, kAsr, true},   {PRAYER_MAGHRIB, kMaghrib, true},  {PRAYER_ISHA, kIsha, true},
  };
  for (size_t i = 0; i < sizeof(maps) / sizeof(maps[0]); ++i) {
    PrayerOccurrence& p = out->prayers[maps[i].id];
    p.id = maps[i].id;
    p.valid = false;
    const Json* v = find_named(timings, maps[i].names);
    if (!v || !v->is_string()) {
      if (maps[i].required) {
        if (err) *err = std::string("missing prayer field: ") + maps[i].names[0];
        return false;
      }
      continue;
    }
    if (!parse_hhmm(v->as_string(), &p.hour, &p.minute)) {
      if (maps[i].required) {
        if (err) *err = std::string("invalid time for ") + maps[i].names[0];
        return false;
      }
      continue;
    }
    p.valid = true;
  }
  return true;
}

static bool finish_schedule(PrayerSchedule* s, const Location& loc, const CalendarDate& date,
                            const char* source, int64_t fetched_at, std::string* err) {
  s->version = kScheduleVersion;
  s->cache_date_istanbul = date;
  s->local_date = date;
  s->location = loc;
  s->location.latitude = normalize_coord(loc.latitude);
  s->location.longitude = normalize_coord(loc.longitude);
  s->fetched_at_unix = fetched_at;
  s->source = source;
  if (!fill_unix_times(s)) {
    if (err) *err = "validated times failed integrity checks";
    return false;
  }
  return true;
}

AladhanProvider::AladhanProvider(HttpClient* http, std::string endpoint)
    : http_(http), endpoint_(endpoint.empty() ? kDefaultAladhanEndpoint : endpoint) {}

bool AladhanProvider::fetch_daily(const Location& loc, const CalendarDate& date, int timeout_ms,
                                  PrayerSchedule* out, std::string* err) {
  if (!http_) {
    if (err) *err = "no http client";
    return false;
  }
  if (!loc.valid() || !date.valid()) {
    if (err) *err = "invalid location or date";
    return false;
  }
  char dbuf[32];
  std::snprintf(dbuf, sizeof(dbuf), "%02d-%02d-%04d", date.day, date.month, date.year);
  char q[512];
  std::snprintf(q, sizeof(q), "/%s?latitude=%.4f&longitude=%.4f&method=%d&timezonestring=%s", dbuf,
                loc.latitude, loc.longitude, calculation_method_for_location(loc),
                url_encode(loc.timezone).c_str());
  std::string url = endpoint_;
  if (!url.empty() && url[url.size() - 1] == '/') url.erase(url.size() - 1);
  url += q;

  HttpResult r = http_->get(url, timeout_ms);
  if (!r.ok) {
    if (err) *err = r.error.empty() ? "http request failed" : r.error;
    return false;
  }
  if (r.status != 200) {
    if (err) {
      char b[64];
      std::snprintf(b, sizeof(b), "http status %d", r.status);
      *err = b;
    }
    return false;
  }
  Json root;
  std::string perr;
  if (!Json::parse(r.body, &root, &perr)) {
    if (err) *err = "invalid JSON: " + perr;
    return false;
  }
  if (root.get("code").as_int(0) != 200 && !root.has("data")) {
    if (err) *err = "API returned error";
    return false;
  }
  const Json& data = root.get("data");
  const Json& timings = data.get("timings");
  if (!timings.is_object()) {
    if (err) *err = "missing timings";
    return false;
  }
  PrayerSchedule s;
  if (!parse_prayer_timings_object(timings, &s, err)) return false;

  const Json& greg = data.get("date").get("gregorian");
  if (greg.is_object() && greg.has("date")) {
    std::string gd = greg.get("date").as_string();
    int dd = 0, mm = 0, yy = 0;
    if (std::sscanf(gd.c_str(), "%d-%d-%d", &dd, &mm, &yy) == 3) {
      if (yy != date.year || mm != date.month || dd != date.day) {
        if (err) *err = "API date mismatch";
        return false;
      }
    }
  }
  const Json& meta = data.get("meta");
  if (meta.is_object()) {
    if (meta.has("timezone")) {
      std::string tz = meta.get("timezone").as_string();
      if (!tz.empty() && tz != loc.timezone) {
        // Keep configured timezone as authoritative for scheduling.
      }
    }
  }
  return finish_schedule(&s, loc, date, kSourceAladhan, SystemClock().now_unix(), err) &&
         (*out = s, true);
}

static void plog(Logger* log, bool warn, const std::string& msg) {
  if (!log) return;
  if (warn)
    log->warn(msg);
  else
    log->info(msg);
}

static bool http_access_denied(int status) {
  return status == 401 || status == 403 || status == 429 || status >= 500;
}

static std::string http_status_error(int status) {
  char b[64];
  std::snprintf(b, sizeof(b), "HTTP %d", status);
  return b;
}

static bool timings_from_json(const Json& j, PrayerSchedule* out, int depth) {
  if (depth > 6 || !out) return false;
  if (j.is_array()) {
    for (size_t i = 0; i < j.size(); ++i) {
      if (timings_from_json(j.at(i), out, depth + 1)) return true;
    }
    return false;
  }
  if (!j.is_object()) return false;

  const Json* direct = 0;
  if (j.get("results").is_object())
    direct = &j.get("results");
  else if (j.get("times").is_object())
    direct = &j.get("times");
  else if (j.get("timings").is_object())
    direct = &j.get("timings");
  else if (j.get("data").get("timings").is_object())
    direct = &j.get("data").get("timings");

  PrayerSchedule tmp;
  std::string e;
  if (direct && parse_prayer_timings_object(*direct, &tmp, &e)) {
    *out = tmp;
    return true;
  }
  if (parse_prayer_timings_object(j, &tmp, &e)) {
    *out = tmp;
    return true;
  }

  const std::map<std::string, Json>& items = j.object_items();
  for (std::map<std::string, Json>::const_iterator it = items.begin(); it != items.end(); ++it) {
    if (timings_from_json(it->second, out, depth + 1)) return true;
  }
  return false;
}

bool parse_islamicfinder_json_body(const std::string& body, PrayerSchedule* out, std::string* err) {
  Json root;
  std::string perr;
  if (!Json::parse(body, &root, &perr)) {
    if (err) *err = "invalid JSON: " + perr;
    return false;
  }
  if (!timings_from_json(root, out, 0)) {
    if (err) *err = "missing timings in IslamicFinder response";
    return false;
  }
  return true;
}

static std::string ascii_lower(const std::string& s) {
  std::string o = s;
  for (size_t i = 0; i < o.size(); ++i) {
    if (o[i] >= 'A' && o[i] <= 'Z') o[i] = static_cast<char>(o[i] - 'A' + 'a');
  }
  return o;
}

static void replace_entities(std::string* s) {
  if (!s) return;
  size_t p;
  while ((p = s->find("&nbsp;")) != std::string::npos) s->replace(p, 6, " ");
  while ((p = s->find("&#160;")) != std::string::npos) s->replace(p, 6, " ");
  while ((p = s->find("\xC2\xA0")) != std::string::npos) s->replace(p, 2, " ");
}

static std::string inner_text(const std::string& html) {
  std::string o;
  o.reserve(html.size());
  bool in = false;
  for (size_t i = 0; i < html.size(); ++i) {
    if (html[i] == '<')
      in = true;
    else if (html[i] == '>')
      in = false;
    else if (!in)
      o.push_back(html[i]);
  }
  replace_entities(&o);
  return o;
}

static std::string trim_copy(const std::string& s) {
  size_t a = 0;
  while (a < s.size() && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
  size_t b = s.size();
  while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
  return s.substr(a, b - a);
}

static bool extract_balanced(const std::string& s, size_t start, std::string* out) {
  if (start >= s.size() || s[start] != '{') return false;
  int depth = 0;
  bool in_str = false;
  bool esc = false;
  for (size_t i = start; i < s.size(); ++i) {
    char c = s[i];
    if (in_str) {
      if (esc)
        esc = false;
      else if (c == '\\')
        esc = true;
      else if (c == '"')
        in_str = false;
      continue;
    }
    if (c == '"') {
      in_str = true;
      continue;
    }
    if (c == '{')
      ++depth;
    else if (c == '}') {
      --depth;
      if (depth == 0) {
        *out = s.substr(start, i - start + 1);
        return true;
      }
    }
  }
  return false;
}

static bool extract_time_after_label(const std::string& html, const std::string& html_lower,
                                     const char* label, int* hour, int* minute) {
  std::string lab = ascii_lower(label);
  size_t pos = 0;
  while ((pos = html_lower.find(lab, pos)) != std::string::npos) {
    size_t start = pos + lab.size();
    if (start >= html.size()) break;
    std::string slice = html.substr(start, 40);
    if (parse_hhmm(slice, hour, minute)) return true;
    pos = start;
  }
  return false;
}

static bool extract_tile_time(const std::string& html, const std::string& html_lower,
                              const char* tile_class, int* hour, int* minute) {
  std::string marker = ascii_lower(tile_class);
  size_t pos = html_lower.find(marker);
  if (pos == std::string::npos) return false;
  size_t t = html_lower.find("pt-tile-time", pos);
  if (t == std::string::npos || t > pos + 400) return false;
  size_t gt = html.find('>', t);
  if (gt == std::string::npos || gt > t + 80) return false;
  std::string slice = html.substr(gt + 1, 24);
  return parse_hhmm(slice, hour, minute);
}

static bool apply_required_times(PrayerSchedule* out, int fh, int fm, int dh, int dm, int ah,
                                 int am, int mh, int mm, int ih, int im) {
  out->prayers[PRAYER_FAJR].id = PRAYER_FAJR;
  out->prayers[PRAYER_FAJR].valid = true;
  out->prayers[PRAYER_FAJR].hour = fh;
  out->prayers[PRAYER_FAJR].minute = fm;
  out->prayers[PRAYER_DHUHR].id = PRAYER_DHUHR;
  out->prayers[PRAYER_DHUHR].valid = true;
  out->prayers[PRAYER_DHUHR].hour = dh;
  out->prayers[PRAYER_DHUHR].minute = dm;
  out->prayers[PRAYER_ASR].id = PRAYER_ASR;
  out->prayers[PRAYER_ASR].valid = true;
  out->prayers[PRAYER_ASR].hour = ah;
  out->prayers[PRAYER_ASR].minute = am;
  out->prayers[PRAYER_MAGHRIB].id = PRAYER_MAGHRIB;
  out->prayers[PRAYER_MAGHRIB].valid = true;
  out->prayers[PRAYER_MAGHRIB].hour = mh;
  out->prayers[PRAYER_MAGHRIB].minute = mm;
  out->prayers[PRAYER_ISHA].id = PRAYER_ISHA;
  out->prayers[PRAYER_ISHA].valid = true;
  out->prayers[PRAYER_ISHA].hour = ih;
  out->prayers[PRAYER_ISHA].minute = im;
  return true;
}

static bool schedule_has_required(const PrayerSchedule& s) {
  return s.prayers[PRAYER_FAJR].valid && s.prayers[PRAYER_DHUHR].valid &&
         s.prayers[PRAYER_ASR].valid && s.prayers[PRAYER_MAGHRIB].valid &&
         s.prayers[PRAYER_ISHA].valid;
}

static bool parse_embedded_json_times(const std::string& html, PrayerSchedule* out) {
  std::string lower = ascii_lower(html);
  size_t pos = 0;
  while ((pos = lower.find("<script", pos)) != std::string::npos) {
    size_t gt = html.find('>', pos);
    if (gt == std::string::npos) break;
    size_t end = lower.find("</script", gt);
    if (end == std::string::npos) break;
    std::string body = html.substr(gt + 1, end - (gt + 1));
    size_t obj = 0;
    while ((obj = body.find('{', obj)) != std::string::npos) {
      std::string json;
      if (!extract_balanced(body, obj, &json)) break;
      Json root;
      std::string perr;
      PrayerSchedule tmp;
      if (Json::parse(json, &root, &perr) && timings_from_json(root, &tmp, 0) &&
          schedule_has_required(tmp)) {
        *out = tmp;
        return true;
      }
      obj += json.size();
    }
    pos = end + 1;
  }
  return false;
}

static int month_from_name(const std::string& text) {
  std::string l = ascii_lower(text);
  static const char* names[] = {"january", "february", "march",     "april",   "may",      "june",
                                "july",    "august",   "september", "october", "november", "december"};
  for (int i = 0; i < 12; ++i) {
    size_t p = l.find(names[i]);
    if (p == std::string::npos) continue;
    size_t e = p + std::strlen(names[i]);
    bool before = p == 0 || !std::isalpha(static_cast<unsigned char>(l[p - 1]));
    bool after = e >= l.size() || !std::isalpha(static_cast<unsigned char>(l[e]));
    if (before && after) return i + 1;
  }
  return 0;
}

static bool parse_page_gregorian_date(const std::string& html, const std::string& html_lower,
                                      CalendarDate* d) {
  size_t p = html_lower.find("pt-date-gregorian");
  if (p == std::string::npos) return false;
  size_t gt = html.find('>', p);
  if (gt == std::string::npos) return false;
  size_t end = html_lower.find("</span", gt);
  if (end == std::string::npos || end > gt + 80) end = gt + 40;
  std::string text = trim_copy(inner_text(html.substr(gt + 1, end - (gt + 1))));
  int day = 0, year = 0;
  char mon[40];
  mon[0] = 0;
  if (std::sscanf(text.c_str(), "%d %39[^,], %d", &day, mon, &year) != 3 &&
      std::sscanf(text.c_str(), "%d %39s %d", &day, mon, &year) != 3) {
    return false;
  }
  int month = month_from_name(mon);
  if (month < 1 || day < 1 || day > 31 || year < 1970) return false;
  d->year = year;
  d->month = month;
  d->day = day;
  return d->valid();
}

static bool extract_element_by_id(const std::string& html, const std::string& html_lower,
                                  const char* id, std::string* inner) {
  std::string a = std::string("id=\"") + id + "\"";
  std::string b = std::string("id='") + id + "'";
  size_t p = html_lower.find(a);
  if (p == std::string::npos) p = html_lower.find(b);
  if (p == std::string::npos) return false;
  size_t start = html_lower.rfind('<', p);
  if (start == std::string::npos) return false;
  size_t name_s = start + 1;
  while (name_s < html.size() && std::isspace(static_cast<unsigned char>(html[name_s]))) ++name_s;
  size_t name_e = name_s;
  while (name_e < html.size() && std::isalpha(static_cast<unsigned char>(html[name_e]))) ++name_e;
  std::string tag = ascii_lower(html.substr(name_s, name_e - name_s));
  if (tag.empty()) return false;
  std::string close = "</" + tag;
  size_t gt = html.find('>', p);
  if (gt == std::string::npos) return false;
  size_t end = html_lower.find(close, gt);
  if (end == std::string::npos) return false;
  *inner = html.substr(gt + 1, end - (gt + 1));
  return true;
}

static void collect_cells(const std::string& row, std::vector<std::string>* cells) {
  cells->clear();
  std::string lower = ascii_lower(row);
  size_t i = 0;
  while (i < row.size()) {
    size_t th = lower.find("<th", i);
    size_t td = lower.find("<td", i);
    size_t t = std::string::npos;
    bool is_th = false;
    if (th != std::string::npos && (td == std::string::npos || th < td)) {
      t = th;
      is_th = true;
    } else {
      t = td;
    }
    if (t == std::string::npos) break;
    size_t gt = row.find('>', t);
    if (gt == std::string::npos) break;
    const char* endtag = is_th ? "</th" : "</td";
    size_t end = lower.find(endtag, gt);
    if (end == std::string::npos) break;
    cells->push_back(trim_copy(inner_text(row.substr(gt + 1, end - (gt + 1)))));
    i = end + 1;
  }
}

static int classify_header(const std::string& h) {
  std::string l = ascii_lower(h);
  if (l.find("sunrise") != std::string::npos || l.find("sunset") != std::string::npos) return -1;
  if (l.find("qiyam") != std::string::npos || l.find("qiyaam") != std::string::npos) return -1;
  if (l.find("fajr") != std::string::npos || l.find("fajar") != std::string::npos ||
      l.find("imsak") != std::string::npos)
    return PRAYER_FAJR;
  if (l.find("dhuhr") != std::string::npos || l.find("zuhr") != std::string::npos ||
      l.find("dhuhur") != std::string::npos || l.find("dhuhar") != std::string::npos)
    return PRAYER_DHUHR;
  if (l.find("asr") != std::string::npos) return PRAYER_ASR;
  if (l.find("maghrib") != std::string::npos) return PRAYER_MAGHRIB;
  if (l.find("isha") != std::string::npos) return PRAYER_ISHA;
  return -2;
}

static bool parse_monthly_table(const std::string& html, const std::string& html_lower,
                                const CalendarDate& want, const CalendarDate& page_date,
                                PrayerSchedule* out) {
  std::string table;
  if (!extract_element_by_id(html, html_lower, "monthly-prayers", &table)) return false;
  std::string table_lower = ascii_lower(table);
  int table_month = 0;
  int table_year = page_date.valid() ? page_date.year : (want.valid() ? want.year : 0);

  std::vector<std::string> headers;
  size_t thead = table_lower.find("<thead");
  size_t header_region_end = table.size();
  std::string header_src = table;
  if (thead != std::string::npos) {
    size_t thead_end = table_lower.find("</thead", thead);
    if (thead_end != std::string::npos) {
      header_src = table.substr(thead, thead_end - thead);
      header_region_end = thead_end;
    }
  }
  size_t htr = ascii_lower(header_src).find("<tr");
  if (htr != std::string::npos) {
    size_t htr_end = ascii_lower(header_src).find("</tr", htr);
    if (htr_end == std::string::npos) htr_end = header_src.size();
    collect_cells(header_src.substr(htr, htr_end - htr), &headers);
  }
  if (!headers.empty()) table_month = month_from_name(headers[0]);

  std::vector<int> col_kind(headers.size(), -2);
  for (size_t i = 0; i < headers.size(); ++i) col_kind[i] = classify_header(headers[i]);

  size_t search = header_region_end;
  std::string tlower = table_lower;
  PrayerSchedule best;
  bool have = false;
  while ((search = tlower.find("<tr", search)) != std::string::npos) {
    size_t tr_end = tlower.find("</tr", search);
    if (tr_end == std::string::npos) break;
    std::string row = table.substr(search, tr_end - search);
    std::string row_l = ascii_lower(row);
    bool active = row_l.find("tr-active") != std::string::npos;
    std::vector<std::string> cells;
    collect_cells(row, &cells);
    search = tr_end + 4;
    if (cells.size() < 5) continue;

    int day = 0;
    if (!cells.empty()) std::sscanf(cells[0].c_str(), "%d", &day);

    bool match = false;
    if (want.valid() && table_month > 0 && day == want.day && table_month == want.month &&
        (table_year == 0 || table_year == want.year)) {
      match = true;
    } else if (!want.valid() && active) {
      match = true;
    } else if (want.valid() && page_date.valid() && want == page_date && active) {
      match = true;
    }
    if (!match) continue;

    PrayerSchedule tmp;
    size_t n = cells.size() < col_kind.size() ? cells.size() : col_kind.size();
    if (col_kind.empty()) {
      // Fallback column order used by IslamicFinder: date, hijri, day, fajr, sunrise,
      // dhuhr, asr, maghrib, isha.
      static const int kDefault[] = {-2, -2, -2, PRAYER_FAJR, -1, PRAYER_DHUHR,
                                     PRAYER_ASR, PRAYER_MAGHRIB, PRAYER_ISHA};
      n = cells.size() < 9 ? cells.size() : 9;
      for (size_t i = 0; i < n; ++i) {
        if (kDefault[i] < 0) continue;
        int h = 0, m = 0;
        if (!parse_hhmm(cells[i], &h, &m)) continue;
        PrayerId id = static_cast<PrayerId>(kDefault[i]);
        tmp.prayers[id].id = id;
        tmp.prayers[id].valid = true;
        tmp.prayers[id].hour = h;
        tmp.prayers[id].minute = m;
      }
    } else {
      for (size_t i = 0; i < n; ++i) {
        if (col_kind[i] < 0) continue;
        int h = 0, m = 0;
        if (!parse_hhmm(cells[i], &h, &m)) continue;
        PrayerId id = static_cast<PrayerId>(col_kind[i]);
        tmp.prayers[id].id = id;
        tmp.prayers[id].valid = true;
        tmp.prayers[id].hour = h;
        tmp.prayers[id].minute = m;
      }
    }
    if (schedule_has_required(tmp)) {
      best = tmp;
      have = true;
      if (want.valid()) break;
    }
  }
  if (!have) return false;
  *out = best;
  return true;
}

static bool parse_meta_or_tiles(const std::string& html, const std::string& lower,
                                PrayerSchedule* out) {
  int fh = 0, fm = 0, dh = 0, dm = 0, ah = 0, am = 0, mh = 0, mm = 0, ih = 0, im = 0;
  bool meta = extract_time_after_label(html, lower, "Fajar Prayer Time", &fh, &fm) ||
              extract_time_after_label(html, lower, "Fajr Prayer Time", &fh, &fm);
  meta = meta && (extract_time_after_label(html, lower, "Dhuhur Prayer Time", &dh, &dm) ||
                  extract_time_after_label(html, lower, "Dhuhr Prayer Time", &dh, &dm));
  meta = meta && extract_time_after_label(html, lower, "Asr Prayer Time", &ah, &am);
  meta = meta && extract_time_after_label(html, lower, "Maghrib Prayer Time", &mh, &mm);
  meta = meta && extract_time_after_label(html, lower, "Isha Prayer Time", &ih, &im);

  bool tiles = false;
  if (!meta) {
    tiles = extract_tile_time(html, lower, "fajar-tile", &fh, &fm) &&
            extract_tile_time(html, lower, "dhuhar-tile", &dh, &dm) &&
            extract_tile_time(html, lower, "asr-tile", &ah, &am) &&
            extract_tile_time(html, lower, "maghrib-tile", &mh, &mm) &&
            extract_tile_time(html, lower, "isha-tile", &ih, &im);
  }
  if (!meta && !tiles) return false;
  apply_required_times(out, fh, fm, dh, dm, ah, am, mh, mm, ih, im);
  int sh = 0, sm = 0;
  if (extract_tile_time(html, lower, "sunrise-tile", &sh, &sm) ||
      extract_time_after_label(html, lower, "Sunrise Prayer Time", &sh, &sm)) {
    out->prayers[PRAYER_SUNRISE].id = PRAYER_SUNRISE;
    out->prayers[PRAYER_SUNRISE].valid = true;
    out->prayers[PRAYER_SUNRISE].hour = sh;
    out->prayers[PRAYER_SUNRISE].minute = sm;
  }
  return true;
}

bool parse_islamicfinder_page_html(const std::string& html, const CalendarDate& date,
                                   PrayerSchedule* out, std::string* err) {
  if (!out) return false;
  *out = PrayerSchedule();
  std::string lower = ascii_lower(html);

  PrayerSchedule parsed;
  if (parse_embedded_json_times(html, &parsed) && schedule_has_required(parsed)) {
    *out = parsed;
    return true;
  }

  CalendarDate page_date;
  page_date.year = page_date.month = page_date.day = 0;
  parse_page_gregorian_date(html, lower, &page_date);

  if (parse_monthly_table(html, lower, date, page_date, &parsed) && schedule_has_required(parsed)) {
    *out = parsed;
    int sh = 0, sm = 0;
    if (extract_tile_time(html, lower, "sunrise-tile", &sh, &sm)) {
      out->prayers[PRAYER_SUNRISE].id = PRAYER_SUNRISE;
      out->prayers[PRAYER_SUNRISE].valid = true;
      out->prayers[PRAYER_SUNRISE].hour = sh;
      out->prayers[PRAYER_SUNRISE].minute = sm;
    }
    return true;
  }

  bool today_ok = !date.valid() || !page_date.valid() || date == page_date;
  if (today_ok && parse_meta_or_tiles(html, lower, &parsed) && schedule_has_required(parsed)) {
    *out = parsed;
    return true;
  }
  if (err) *err = "IslamicFinder page missing prayer times";
  return false;
}

bool parse_islamicfinder_page_html(const std::string& html, PrayerSchedule* out, std::string* err) {
  CalendarDate d;
  d.year = d.month = d.day = 0;
  return parse_islamicfinder_page_html(html, d, out, err);
}

static const char* country_slug_for_name(const std::string& country_name) {
  if (country_name == "Turkey") return "turkey";
  if (country_name == "Saudi Arabia") return "saudi-arabia";
  if (country_name == "United Arab Emirates") return "united-arab-emirates";
  if (country_name == "Egypt") return "egypt";
  if (country_name == "Germany") return "germany";
  if (country_name == "United Kingdom") return "united-kingdom";
  if (country_name == "France") return "france";
  if (country_name == "Netherlands") return "netherlands";
  if (country_name == "Bosnia and Herzegovina") return "bosnia-and-herzegovina";
  if (country_name == "United States") return "united-states";
  if (country_name == "Canada") return "canada";
  return 0;
}

static bool valid_if_slug(const std::string& s) {
  if (s.empty() || s.size() > 80) return false;
  for (size_t i = 0; i < s.size(); ++i) {
    char c = s[i];
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return false;
  }
  return true;
}

IslamicFinderApiClient::IslamicFinderApiClient(HttpClient* http, std::string json_endpoint)
    : http_(http), json_endpoint_(canonical_islamicfinder_endpoint(json_endpoint)) {}

bool IslamicFinderApiClient::fetch(const Location& loc, const CalendarDate& date, int timeout_ms,
                                   PrayerSchedule* out, std::string* err) {
  if (!islamicfinder_json_api_configured(json_endpoint_)) {
    if (err) *err = "no public JSON endpoint";
    return false;
  }
  if (!http_) {
    if (err) *err = "no http client";
    return false;
  }
  std::string url = json_endpoint_;
  char sep = json_endpoint_.find('?') == std::string::npos ? '?' : '&';
  char q2[640];
  std::snprintf(q2, sizeof(q2),
                "%clatitude=%.4f&longitude=%.4f&timezone=%s&time_format=0&date=%04d-%02d-%02d", sep,
                loc.latitude, loc.longitude, url_encode(loc.timezone).c_str(), date.year, date.month,
                date.day);
  url += q2;
  HttpResult r = http_->get(url, timeout_ms);
  if (!r.ok) {
    if (err) *err = r.error.empty() ? "http request failed" : r.error;
    return false;
  }
  if (http_access_denied(r.status) || r.status != 200) {
    if (err) *err = http_status_error(r.status);
    return false;
  }
  PrayerSchedule s;
  if (!parse_islamicfinder_json_body(r.body, &s, err)) return false;
  return finish_schedule(&s, loc, date, kSourceIslamicFinder, SystemClock().now_unix(), err) &&
         (*out = s, true);
}

IslamicFinderPageClient::IslamicFinderPageClient(HttpClient* http) : http_(http) {}

bool IslamicFinderPageClient::resolve_page_url(const Location& loc, int timeout_ms, std::string* url,
                                               std::string* err) {
  const CityInfo* place = islamicfinder_place_for_location(loc);
  if (place) {
    *url = islamicfinder_prayer_page_url(*place);
    if (!url->empty()) return true;
  }
  if (!http_ || loc.city.empty()) {
    if (err) *err = "no IslamicFinder city page for this location";
    return false;
  }
  std::string search =
      std::string(kIslamicFinderOrigin) + kIslamicFinderSearchPath + url_encode(loc.city);
  HttpResult r = http_->get(search, timeout_ms);
  if (!r.ok || r.status != 200) {
    if (err) *err = "IslamicFinder city lookup failed";
    return false;
  }
  Json root;
  std::string perr;
  if (!Json::parse(r.body, &root, &perr) || !root.is_array() || root.size() == 0) {
    if (err) *err = "IslamicFinder city lookup returned no results";
    return false;
  }
  int best_i = -1;
  double best_d = 1e18;
  for (size_t i = 0; i < root.size(); ++i) {
    const Json& hit = root.at(i);
    std::string country = hit.get("countryName").as_string("");
    if (!loc.country.empty() && !country.empty() && country != loc.country) continue;
    double lat = hit.get("latitude").as_number(0);
    double lon = hit.get("longitude").as_number(0);
    double d = (lat - loc.latitude) * (lat - loc.latitude) +
               (lon - loc.longitude) * (lon - loc.longitude);
    std::string title = hit.get("title").as_string("");
    if (ascii_lower(title) == ascii_lower(loc.city)) d -= 1000.0;
    if (d < best_d) {
      best_d = d;
      best_i = static_cast<int>(i);
    }
  }
  if (best_i < 0) best_i = 0;
  const Json& best = root.at(static_cast<size_t>(best_i));
  int id = best.get("id").as_int(0);
  std::string slug = ascii_lower(best.get("slug").as_string(""));
  std::string country_name = best.get("countryName").as_string(loc.country);
  const char* cslug = country_slug_for_name(country_name);
  if (id <= 0 || !valid_if_slug(slug) || !cslug) {
    if (err) *err = "IslamicFinder city lookup missing id/slug";
    return false;
  }
  char buf[384];
  std::snprintf(buf, sizeof(buf), "%s/world/%s/%d/%s-prayer-times/", kIslamicFinderOrigin, cslug, id,
                slug.c_str());
  *url = buf;
  return true;
}

bool IslamicFinderPageClient::fetch(const Location& loc, const CalendarDate& date, int timeout_ms,
                                    PrayerSchedule* out, std::string* err) {
  if (!http_) {
    if (err) *err = "no http client";
    return false;
  }
  std::string url;
  if (!resolve_page_url(loc, timeout_ms, &url, err)) return false;
  HttpResult r = http_->get(url, timeout_ms);
  if (!r.ok) {
    if (err) *err = r.error.empty() ? "http request failed" : r.error;
    return false;
  }
  if (http_access_denied(r.status) || r.status != 200) {
    if (err) *err = http_status_error(r.status);
    return false;
  }
  PrayerSchedule s;
  if (!parse_islamicfinder_page_html(r.body, date, &s, err)) return false;
  Location stored = loc;
  apply_islamicfinder_place(&stored);
  return finish_schedule(&s, stored, date, kSourceIslamicFinder, SystemClock().now_unix(), err) &&
         (*out = s, true);
}

IslamicFinderProvider::IslamicFinderProvider(HttpClient* http, std::string json_endpoint)
    : api_(http, json_endpoint), page_(http), log_(0) {}

bool IslamicFinderProvider::fetch_daily(const Location& loc, const CalendarDate& date,
                                        int timeout_ms, PrayerSchedule* out, std::string* err) {
  if (!loc.valid() || !date.valid()) {
    if (err) *err = "invalid location or date";
    return false;
  }
  plog(log_, false, "Prayer provider: IslamicFinder");
  std::string api_err;
  if (api_.fetch(loc, date, timeout_ms, out, &api_err)) {
    plog(log_, false, "IslamicFinder API parsed successfully");
    plog(log_, false, "Prayer schedule source: IslamicFinder");
    return true;
  }
  plog(log_, false, std::string("IslamicFinder API unavailable: ") +
                        (api_err.empty() ? std::string("unknown") : api_err));
  plog(log_, false, "Trying IslamicFinder public page");
  std::string page_err;
  if (page_.fetch(loc, date, timeout_ms, out, &page_err)) {
    plog(log_, false, "IslamicFinder page parsed successfully");
    plog(log_, false, "Prayer schedule source: IslamicFinder");
    return true;
  }
  plog(log_, true, "IslamicFinder API unavailable");
  plog(log_, true, std::string("IslamicFinder page unavailable") +
                       (page_err.empty() ? std::string("") : (": " + page_err)));
  if (err) {
    *err = "IslamicFinder unavailable";
    if (!api_err.empty()) *err += " [api: " + api_err + "]";
    if (!page_err.empty()) *err += " [page: " + page_err + "]";
  }
  return false;
}

FallbackProvider::FallbackProvider(PrayerTimeProvider* primary, PrayerTimeProvider* secondary,
                                   Logger* log)
    : primary_(primary), secondary_(secondary), log_(log) {}

bool FallbackProvider::fetch_daily(const Location& loc, const CalendarDate& date, int timeout_ms,
                                   PrayerSchedule* out, std::string* err) {
  std::string e1, e2;
  if (primary_ && primary_->fetch_daily(loc, date, timeout_ms, out, &e1)) return true;
  if (secondary_) {
    plog(log_, false, "Falling back to Aladhan");
    if (secondary_->fetch_daily(loc, date, timeout_ms, out, &e2)) {
      plog(log_, false, "Prayer schedule source: Aladhan");
      return true;
    }
  }
  if (err) {
    *err = "all providers failed";
    if (!e1.empty()) *err += " [primary: " + e1 + "]";
    if (!e2.empty()) *err += " [secondary: " + e2 + "]";
  }
  return false;
}

}  // namespace adhan
