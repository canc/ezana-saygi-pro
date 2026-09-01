#include "i18n.h"

#include "constants.h"
#include "timezone.h"

namespace adhan {
namespace ui {

Language language() { return LANG_TR; }

namespace {

bool is_en() { return language() == LANG_EN; }

}  // namespace

const wchar_t* app_title() { return L"Ezana Sayg\u0131 PRO"; }
const wchar_t* status_label() { return is_en() ? L"Status" : L"Durum"; }
const wchar_t* location_label() { return is_en() ? L"Location" : L"Konum"; }
const wchar_t* city_label() { return is_en() ? L"City" : L"\u015eehir"; }
const wchar_t* timezone_label() { return is_en() ? L"Timezone" : L"Zaman Dilimi"; }
const wchar_t* threshold_label() {
  return is_en() ? L"Before Adhan" : L"Ezan Vaktinden \u00d6nce";
}
const wchar_t* next_prayer_label() { return is_en() ? L"Next prayer" : L"Sonraki Ezan"; }
const wchar_t* detect_location() { return is_en() ? L"Detect" : L"Konumu Bul"; }
const wchar_t* refresh_times() { return is_en() ? L"Refresh" : L"Vakitleri Yenile"; }
const wchar_t* close_hint() {
  return is_en() ? L"Closing this window hides it to the tray."
                 : L"Pencereyi kapatmak uygulamay\u0131 tepsiye gizler.";
}
const wchar_t* active() { return is_en() ? L"On" : L"Aktif"; }
const wchar_t* inactive() { return is_en() ? L"Off" : L"Pasif"; }
const wchar_t* enable_action() { return is_en() ? L"Enable" : L"Etkinle\u015ftir"; }
const wchar_t* disable_action() {
  return is_en() ? L"Disable" : L"Devre D\u0131\u015f\u0131 B\u0131rak";
}
const wchar_t* toggle_action(bool currently_enabled) {
  return currently_enabled ? disable_action() : enable_action();
}
std::wstring app_version() {
  std::wstring w = L"v";
  for (const char* p = kVersion; p && *p; ++p)
    w.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*p)));
  return w;
}
const wchar_t* tray_show() { return is_en() ? L"Show" : L"G\u00f6ster"; }
const wchar_t* tray_exit() { return is_en() ? L"Exit" : L"Kapat"; }

const wchar_t* threshold_option(int seconds) {
  if (is_en()) {
    if (seconds == 30) return L"30 seconds";
    if (seconds == 60) return L"1 minute";
    if (seconds == 120) return L"2 minutes";
    if (seconds == 180) return L"3 minutes";
    if (seconds == 300) return L"5 minutes";
    return L"1 minute";
  }
  if (seconds == 30) return L"30 saniye";
  if (seconds == 60) return L"1 dakika";
  if (seconds == 120) return L"2 dakika";
  if (seconds == 180) return L"3 dakika";
  if (seconds == 300) return L"5 dakika";
  return L"1 dakika";
}

const wchar_t* prayer(PrayerId id) {
  if (is_en()) {
    switch (id) {
      case PRAYER_FAJR:
        return L"Fajr";
      case PRAYER_SUNRISE:
        return L"Sunrise";
      case PRAYER_DHUHR:
        return L"Dhuhr";
      case PRAYER_ASR:
        return L"Asr";
      case PRAYER_MAGHRIB:
        return L"Maghrib";
      case PRAYER_ISHA:
        return L"Isha";
      default:
        return L"";
    }
  }
  switch (id) {
    case PRAYER_FAJR:
      return L"\u0130msak";
    case PRAYER_SUNRISE:
      return L"G\u00fcne\u015f";
    case PRAYER_DHUHR:
      return L"\u00d6\u011fle";
    case PRAYER_ASR:
      return L"\u0130kindi";
    case PRAYER_MAGHRIB:
      return L"Ak\u015fam";
    case PRAYER_ISHA:
      return L"Yats\u0131";
    default:
      return L"";
  }
}

const wchar_t* country(const char* english_name) {
  if (!english_name) return L"";
  std::string e = english_name;
  if (is_en()) return L"";  // caller falls back to the English identifier
  if (e == "Turkey") return L"T\u00fcrkiye";
  if (e == "Saudi Arabia") return L"Suudi Arabistan";
  if (e == "United Arab Emirates") return L"Birle\u015fik Arap Emirlikleri";
  if (e == "Egypt") return L"M\u0131s\u0131r";
  if (e == "Germany") return L"Almanya";
  if (e == "United Kingdom") return L"Birle\u015fik Krall\u0131k";
  if (e == "France") return L"Fransa";
  if (e == "Netherlands") return L"Hollanda";
  if (e == "Bosnia and Herzegovina") return L"Bosna-Hersek";
  if (e == "United States") return L"Amerika Birle\u015fik Devletleri";
  if (e == "Canada") return L"Kanada";
  return L"";
}

std::wstring location_item(const char* city, const char* country_en) {
  const wchar_t* ctry = country(country_en);
  std::wstring out;
  if (city) {
    while (*city) out.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*city++)));
  }
  out += L", ";
  if (ctry && ctry[0])
    out += ctry;
  else if (country_en) {
    while (*country_en)
      out.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*country_en++)));
  }
  return out;
}

const wchar_t* err_geo_unavailable() {
  return is_en() ? L"Automatic location is not available. Please select your city from the list."
                 : L"Otomatik konum kullan\u0131lam\u0131yor. L\u00fctfen listeden \u015fehir se\u00e7in.";
}
const wchar_t* err_geo_unmapped() {
  return is_en() ? L"Could not map this PC's country. Please select a city manually."
                 : L"Bu bilgisayar\u0131n \u00fclkesi e\u015fle\u015ftirilemedi. L\u00fctfen \u015fehri elle se\u00e7in.";
}
const wchar_t* err_geo_no_cities() {
  return is_en() ? L"No cities are bundled for this country. Select a city manually."
                 : L"Bu \u00fclke i\u00e7in kay\u0131tl\u0131 \u015fehir yok. L\u00fctfen \u015fehri elle se\u00e7in.";
}
const wchar_t* note_geo_same_country() {
  return is_en() ? L"Country already matches this PC."
                 : L"\u00dclke zaten bu bilgisayarla e\u015fle\u015fiyor.";
}
const wchar_t* note_schedule_updated() {
  return is_en() ? L"Prayer times updated." : L"Ezan vakitleri g\u00fcncellendi.";
}
const wchar_t* err_schedule_unavailable() {
  return is_en()
             ? L"Could not load prayer times. Cached data will be used when available."
             : L"Ezan vakitleri al\u0131namad\u0131. Varsa kay\u0131tl\u0131 vakitler kullan\u0131lacak.";
}
const wchar_t* status_updating() { return is_en() ? L"updating" : L"g\u00fcncelleniyor"; }
const wchar_t* status_no_schedule() { return is_en() ? L"no schedule" : L"vakit yok"; }
const wchar_t* status_fading_out() { return is_en() ? L"fading out" : L"ses k\u0131s\u0131l\u0131yor"; }
const wchar_t* status_muted() { return is_en() ? L"volume at 0" : L"ses kapal\u0131"; }
const wchar_t* status_fading_in() { return is_en() ? L"restoring volume" : L"ses geri y\u00fckleniyor"; }
const wchar_t* status_no_remaining() {
  return is_en() ? L"No remaining prayers today" : L"Bug\u00fcn kalan ezan yok";
}
const wchar_t* em_dash() { return L"\u2014"; }

const wchar_t* duration_settings() {
  return is_en() ? L"Adhan durations" : L"Ezan S\u00fcrelerini Ayarla";
}
const wchar_t* duration_save() { return is_en() ? L"Save" : L"Kaydet"; }
const wchar_t* duration_cancel() { return is_en() ? L"Cancel" : L"\u0130ptal"; }
const wchar_t* duration_minutes() { return is_en() ? L"minutes" : L"dakika"; }
const wchar_t* duration_invalid() {
  return is_en() ? L"Enter a whole number of minutes from 1 to 30."
                 : L"S\u00fcre 1 ile 30 dakika aras\u0131nda bir tam say\u0131 olmal\u0131d\u0131r.";
}

std::wstring timezone_text(const std::string& iana, int64_t unix_utc) {
  std::string raw = timezone_display(iana, unix_utc);
  // Replace well-known IANA prefixes for display; keep the GMT offset.
  std::wstring w;
  auto append_utf8 = [&w](const std::string& s) {
    for (size_t i = 0; i < s.size();) {
      unsigned char c = static_cast<unsigned char>(s[i]);
      if (c < 0x80) {
        w.push_back(c);
        ++i;
      } else if ((c & 0xE0) == 0xC0 && i + 1 < s.size()) {
        w.push_back(((c & 0x1F) << 6) | (s[i + 1] & 0x3F));
        i += 2;
      } else if ((c & 0xF0) == 0xE0 && i + 2 < s.size()) {
        w.push_back(((c & 0x0F) << 12) | ((s[i + 1] & 0x3F) << 6) | (s[i + 2] & 0x3F));
        i += 3;
      } else {
        ++i;
      }
    }
  };
  if (is_en()) {
    append_utf8(raw);
    return w;
  }
  size_t paren = raw.find(" (");
  std::string offset = paren == std::string::npos ? "" : raw.substr(paren);
  const wchar_t* loc = 0;
  if (iana == "Europe/Istanbul")
    loc = L"Avrupa/\u0130stanbul";
  else if (iana == "Europe/London")
    loc = L"Avrupa/Londra";
  else if (iana == "Europe/Berlin")
    loc = L"Avrupa/Berlin";
  else if (iana == "Europe/Paris")
    loc = L"Avrupa/Paris";
  else if (iana == "Europe/Amsterdam")
    loc = L"Avrupa/Amsterdam";
  else if (iana == "Europe/Sarajevo")
    loc = L"Avrupa/Saraybosna";
  else if (iana == "Asia/Riyadh")
    loc = L"Asya/Riyad";
  else if (iana == "Asia/Dubai")
    loc = L"Asya/Dubai";
  else if (iana == "Africa/Cairo")
    loc = L"Afrika/Kahire";
  else if (iana == "America/New_York")
    loc = L"Amerika/New York";
  else if (iana == "America/Chicago")
    loc = L"Amerika/Chicago";
  else if (iana == "America/Los_Angeles")
    loc = L"Amerika/Los Angeles";
  else if (iana == "America/Toronto")
    loc = L"Amerika/Toronto";
  if (loc) {
    w = loc;
    append_utf8(offset);
    return w;
  }
  append_utf8(raw);
  return w;
}

std::wstring tray_tip(const wchar_t* next_prayer, bool enabled) {
  std::wstring t = app_title();
  if (next_prayer && next_prayer[0]) {
    t += L" - ";
    t += next_prayer;
  }
  t += L" (";
  t += enabled ? active() : inactive();
  t += L")";
  return t;
}

std::wstring source_text(const std::string& source) {
  if (source.empty()) return std::wstring();
  std::wstring label = is_en() ? L"Source: " : L"Kaynak: ";
  if (source == kSourceIslamicFinder || source == "IslamicFinder")
    label += L"IslamicFinder";
  else if (source == kSourceAladhan || source == "Aladhan")
    label += L"Aladhan";
  else {
    for (size_t i = 0; i < source.size(); ++i)
      label.push_back(static_cast<wchar_t>(static_cast<unsigned char>(source[i])));
  }
  return label;
}

}  // namespace ui
}  // namespace adhan
