#include "locations.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace adhan {
namespace {

const CityInfo kCities[] = {
    {"Turkey", "Adana", 37.0000, 35.3213, "Europe/Istanbul", 325363, "turkey", "adana"},
    {"Turkey", "Adiyaman", 37.7648, 38.2786, "Europe/Istanbul", 47563710, "turkey", "adiyaman"},
    {"Turkey", "Afyonkarahisar", 38.7569, 30.5387, "Europe/Istanbul", 325303, "turkey", "afyonkarahisar"},
    {"Turkey", "Agri", 39.7191, 43.0503, "Europe/Istanbul", 309647, "turkey", "agr"},
    {"Turkey", "Aksaray", 38.3687, 34.0370, "Europe/Istanbul", 47565719, "turkey", "aksaray"},
    {"Turkey", "Amasya", 40.6499, 35.8353, "Europe/Istanbul", 752015, "turkey", "amasya"},
    {"Turkey", "Ankara", 39.9334, 32.8597, "Europe/Istanbul", 323786, "turkey", "ankara"},
    {"Turkey", "Antalya", 36.8969, 30.6966, "Europe/Istanbul", 323777, "turkey", "antalya"},
    {"Turkey", "Ardahan", 41.1105, 42.7022, "Europe/Istanbul", 751952, "turkey", "ardahan"},
    {"Turkey", "Artvin", 41.1828, 41.8183, "Europe/Istanbul", 751817, "turkey", "artvin"},
    {"Turkey", "Aydin", 37.8560, 27.8416, "Europe/Istanbul", 322830, "turkey", "aydin"},
    {"Turkey", "Balikesir", 39.6484, 27.8826, "Europe/Istanbul", 322165, "turkey", "balkesir"},
    {"Turkey", "Bartin", 41.6358, 32.3375, "Europe/Istanbul", 751057, "turkey", "bartin"},
    {"Turkey", "Batman", 37.8812, 41.1351, "Europe/Istanbul", 321836, "turkey", "batman"},
    {"Turkey", "Bayburt", 40.2552, 40.2249, "Europe/Istanbul", 47572550, "turkey", "bayburt"},
    {"Turkey", "Bilecik", 40.1506, 29.9793, "Europe/Istanbul", 750598, "turkey", "bilecik"},
    {"Turkey", "Bingol", 38.8855, 40.4966, "Europe/Istanbul", 321082, "turkey", "bingol"},
    {"Turkey", "Bitlis", 38.4006, 42.1095, "Europe/Istanbul", 321025, "turkey", "bitlis"},
    {"Turkey", "Bolu", 40.7392, 31.6089, "Europe/Istanbul", 750516, "turkey", "bolu"},
    {"Turkey", "Burdur", 37.4613, 30.0665, "Europe/Istanbul", 320392, "turkey", "burdur"},
    {"Turkey", "Bursa", 40.1826, 29.0665, "Europe/Istanbul", 750269, "turkey", "bursa"},
    {"Turkey", "Canakkale", 40.1553, 26.4142, "Europe/Istanbul", 749780, "turkey", "canakkale"},
    {"Turkey", "Cankiri", 40.6013, 33.6134, "Europe/Istanbul", 47577944, "turkey", "cankiri"},
    {"Turkey", "Corum", 40.5506, 34.9556, "Europe/Istanbul", 47581583, "turkey", "corum"},
    {"Turkey", "Denizli", 37.7765, 29.0864, "Europe/Istanbul", 317109, "turkey", "denizli"},
    {"Turkey", "Diyarbakir", 37.9144, 40.2306, "Europe/Istanbul", 316541, "turkey", "diyarbakr"},
    {"Turkey", "Duzce", 40.8438, 31.1565, "Europe/Istanbul", 747764, "turkey", "duzce"},
    {"Turkey", "Edirne", 41.6771, 26.5557, "Europe/Istanbul", 747712, "turkey", "edirne"},
    {"Turkey", "Elazig", 38.6810, 39.2264, "Europe/Istanbul", 315808, "turkey", "elazg"},
    {"Turkey", "Erzincan", 39.7464, 39.4911, "Europe/Istanbul", 315373, "turkey", "erzincan"},
    {"Turkey", "Erzurum", 39.9043, 41.2679, "Europe/Istanbul", 315368, "turkey", "erzurum"},
    {"Turkey", "Eskisehir", 39.7767, 30.5206, "Europe/Istanbul", 315202, "turkey", "eskisehir"},
    {"Turkey", "Gaziantep", 37.0662, 37.3833, "Europe/Istanbul", 314830, "turkey", "gaziantep"},
    {"Turkey", "Giresun", 40.9128, 38.3895, "Europe/Istanbul", 746881, "turkey", "giresun"},
    {"Turkey", "Gumushane", 40.4608, 39.4797, "Europe/Istanbul", 47592583, "turkey", "gumushane"},
    {"Turkey", "Hakkari", 37.5770, 43.7365, "Europe/Istanbul", 318137, "turkey", "hakkari"},
    {"Turkey", "Hatay", 36.2023, 36.1613, "Europe/Istanbul", 47595550, "turkey", "hatay"},
    {"Turkey", "Igdir", 39.9237, 44.0450, "Europe/Istanbul", 47597539, "turkey", "igdir"},
    {"Turkey", "Isparta", 37.7648, 30.5566, "Europe/Istanbul", 311073, "turkey", "isparta"},
    {"Turkey", "Istanbul", 41.0082, 28.9784, "Europe/Istanbul", 745044, "turkey", "istanbul"},
    {"Turkey", "Izmir", 38.4192, 27.1287, "Europe/Istanbul", 311046, "turkey", "izmir"},
    {"Turkey", "Kahramanmaras", 37.5858, 36.9371, "Europe/Istanbul", 310859, "turkey", "kahramanmaras"},
    {"Turkey", "Karabuk", 41.2061, 32.6204, "Europe/Istanbul", 47601344, "turkey", "karabuk"},
    {"Turkey", "Karaman", 37.1810, 33.2150, "Europe/Istanbul", 47602922, "turkey", "karaman"},
    {"Turkey", "Kars", 40.6019, 43.0975, "Europe/Istanbul", 47604035, "turkey", "kars"},
    {"Turkey", "Kastamonu", 41.3766, 33.7765, "Europe/Istanbul", 743882, "turkey", "kastamonu"},
    {"Turkey", "Kayseri", 38.7205, 35.4826, "Europe/Istanbul", 308464, "turkey", "kayseri"},
    {"Turkey", "Kilis", 36.7184, 37.1212, "Europe/Istanbul", 307864, "turkey", "kilis"},
    {"Turkey", "Kirikkale", 39.8468, 33.5153, "Europe/Istanbul", 47607485, "turkey", "kirikkale"},
    {"Turkey", "Kirklareli", 41.7355, 27.2247, "Europe/Istanbul", 743166, "turkey", "krklareli"},
    {"Turkey", "Kirsehir", 39.1468, 34.1595, "Europe/Istanbul", 307515, "turkey", "krsehir"},
    {"Turkey", "Kocaeli", 40.7654, 29.9408, "Europe/Istanbul", 47609084, "turkey", "kocaeli"},
    {"Turkey", "Konya", 37.8746, 32.4932, "Europe/Istanbul", 306571, "turkey", "konya"},
    {"Turkey", "Kutahya", 39.4192, 29.9857, "Europe/Istanbul", 305268, "turkey", "kutahya"},
    {"Turkey", "Malatya", 38.3552, 38.3095, "Europe/Istanbul", 304922, "turkey", "malatya"},
    {"Turkey", "Manisa", 38.6191, 27.4289, "Europe/Istanbul", 47614547, "turkey", "manisa"},
    {"Turkey", "Mardin", 37.3212, 40.7245, "Europe/Istanbul", 304797, "turkey", "mardin"},
    {"Turkey", "Mersin", 36.8121, 34.6415, "Europe/Istanbul", 304531, "turkey", "mersin"},
    {"Turkey", "Mugla", 37.2153, 28.3636, "Europe/Istanbul", 304184, "turkey", "mugla"},
    {"Turkey", "Mus", 38.7433, 41.5065, "Europe/Istanbul", 47616804, "turkey", "mus"},
    {"Turkey", "Nevsehir", 38.6244, 34.7239, "Europe/Istanbul", 303831, "turkey", "nevsehir"},
    {"Turkey", "Nigde", 37.9667, 34.6794, "Europe/Istanbul", 47617557, "turkey", "nigde"},
    {"Turkey", "Ordu", 40.9839, 37.8764, "Europe/Istanbul", 741100, "turkey", "ordu"},
    {"Turkey", "Osmaniye", 37.0746, 36.2464, "Europe/Istanbul", 303195, "turkey", "osmaniye"},
    {"Turkey", "Rize", 41.0201, 40.5234, "Europe/Istanbul", 740483, "turkey", "rize"},
    {"Turkey", "Sakarya", 40.7569, 30.3783, "Europe/Istanbul", 47622066, "turkey", "sakarya"},
    {"Turkey", "Samsun", 41.2867, 36.3300, "Europe/Istanbul", 740264, "turkey", "samsun"},
    {"Turkey", "Sanliurfa", 37.1674, 38.7955, "Europe/Istanbul", 298333, "turkey", "sanliurfa"},
    {"Turkey", "Siirt", 37.9333, 41.9500, "Europe/Istanbul", 47625455, "turkey", "siirt"},
    {"Turkey", "Sinop", 42.0231, 35.1531, "Europe/Istanbul", 739600, "turkey", "sinop"},
    {"Turkey", "Sirnak", 37.5164, 42.4611, "Europe/Istanbul", 300640, "turkey", "sirnak"},
    {"Turkey", "Sivas", 39.7477, 37.0179, "Europe/Istanbul", 300619, "turkey", "sivas"},
    {"Turkey", "Tekirdag", 40.9780, 27.5110, "Europe/Istanbul", 47628923, "turkey", "tekirdag-ili"},
    {"Turkey", "Tokat", 40.3167, 36.5500, "Europe/Istanbul", 47630015, "turkey", "tokat"},
    {"Turkey", "Trabzon", 41.0015, 39.7178, "Europe/Istanbul", 738648, "turkey", "trabzon"},
    {"Turkey", "Tunceli", 39.1079, 39.5401, "Europe/Istanbul", 298846, "turkey", "tunceli"},
    {"Turkey", "Usak", 38.6823, 29.4082, "Europe/Istanbul", 298299, "turkey", "usak"},
    {"Turkey", "Van", 38.4891, 43.4089, "Europe/Istanbul", 298117, "turkey", "van"},
    {"Turkey", "Yalova", 40.6550, 29.2769, "Europe/Istanbul", 738025, "turkey", "yalova"},
    {"Turkey", "Yozgat", 39.8200, 34.8083, "Europe/Istanbul", 296562, "turkey", "yozgat"},
    {"Turkey", "Zonguldak", 41.4564, 31.7987, "Europe/Istanbul", 737022, "turkey", "zonguldak"},
    {"Saudi Arabia", "Makkah", 21.4225, 39.8262, "Asia/Riyadh", 104515, "saudi-arabia", "makkah"},
    {"Saudi Arabia", "Madinah", 24.4672, 39.6111, "Asia/Riyadh", 46525651, "saudi-arabia", "al-madinah"},
    {"Saudi Arabia", "Riyadh", 24.7136, 46.6753, "Asia/Riyadh", 108410, "saudi-arabia", "riyadh"},
    {"United Arab Emirates", "Dubai", 25.2048, 55.2708, "Asia/Dubai", 47786364, "united-arab-emirates", "dubai"},
    {"Egypt", "Cairo", 30.0444, 31.2357, "Africa/Cairo", 42601677, "egypt", "cairo"},
    {"Germany", "Berlin", 52.5200, 13.4050, "Europe/Berlin", 2950159, "germany", "berlin"},
    {"Germany", "Cologne", 50.9375, 6.9603, "Europe/Berlin", 43022942, "germany", "cologne"},
    {"United Kingdom", "London", 51.5074, -0.1278, "Europe/London", 2643743, "united-kingdom", "london"},
    {"France", "Paris", 48.8566, 2.3522, "Europe/Paris", 2988507, "france", "paris"},
    {"Netherlands", "Amsterdam", 52.3676, 4.9041, "Europe/Amsterdam", 2759794, "netherlands", "amsterdam"},
    {"Bosnia and Herzegovina", "Sarajevo", 43.8563, 18.4131, "Europe/Sarajevo", 3191281, "bosnia-and-herzegovina", "sarajevo"},
    {"United States", "New York", 40.7128, -74.0060, "America/New_York", 5128581, "united-states", "new-york-city-nyc"},
    {"United States", "Chicago", 41.8781, -87.6298, "America/Chicago", 4887398, "united-states", "chicago"},
    {"United States", "Los Angeles", 34.0522, -118.2437, "America/Los_Angeles", 5368361, "united-states", "los-angeles"},
    {"Canada", "Toronto", 43.6532, -79.3832, "America/Toronto", 6167865, "canada", "toronto"},
};

}  // namespace

const CityInfo* city_table(size_t* count) {
  if (count) *count = sizeof(kCities) / sizeof(kCities[0]);
  return kCities;
}

std::vector<std::string> country_list() {
  std::vector<std::string> out;
  for (size_t i = 0; i < sizeof(kCities) / sizeof(kCities[0]); ++i) {
    bool found = false;
    for (size_t j = 0; j < out.size(); ++j) {
      if (out[j] == kCities[i].country) {
        found = true;
        break;
      }
    }
    if (!found) out.push_back(kCities[i].country);
  }
  return out;
}

std::vector<const CityInfo*> cities_in_country(const std::string& country) {
  std::vector<const CityInfo*> out;
  for (size_t i = 0; i < sizeof(kCities) / sizeof(kCities[0]); ++i) {
    if (country == kCities[i].country) out.push_back(&kCities[i]);
  }
  return out;
}

const CityInfo* find_city(const std::string& country, const std::string& city) {
  for (size_t i = 0; i < sizeof(kCities) / sizeof(kCities[0]); ++i) {
    if (country == kCities[i].country && city == kCities[i].city) return &kCities[i];
  }
  return 0;
}

const CityInfo* nearest_city(double latitude, double longitude) {
  const CityInfo* best = &kCities[0];
  double best_d = 1e18;
  for (size_t i = 0; i < sizeof(kCities) / sizeof(kCities[0]); ++i) {
    double dy = kCities[i].latitude - latitude;
    double dx = kCities[i].longitude - longitude;
    double d = dx * dx + dy * dy;
    if (d < best_d) {
      best_d = d;
      best = &kCities[i];
    }
  }
  return best;
}

const CityInfo* default_city() { return find_city(kDefaultCountry, kDefaultCity); }

const CityInfo* find_islamicfinder_city_id(int city_id) {
  if (city_id <= 0) return 0;
  for (size_t i = 0; i < sizeof(kCities) / sizeof(kCities[0]); ++i) {
    if (kCities[i].islamicfinder_city_id == city_id) return &kCities[i];
  }
  return 0;
}

static bool place_usable(const CityInfo* c) {
  return c && c->islamicfinder_city_id > 0 && c->islamicfinder_country_slug &&
         c->islamicfinder_country_slug[0] && c->islamicfinder_city_slug &&
         c->islamicfinder_city_slug[0];
}

const CityInfo* islamicfinder_place_for_location(const Location& loc) {
  const CityInfo* named = find_city(loc.country, loc.city);
  if (place_usable(named)) return named;
  if (loc.islamicfinder_city_id > 0) {
    const CityInfo* by_id = find_islamicfinder_city_id(loc.islamicfinder_city_id);
    if (place_usable(by_id)) return by_id;
  }
  const CityInfo* near = nearest_city(loc.latitude, loc.longitude);
  if (place_usable(near)) return near;
  return 0;
}

std::string islamicfinder_prayer_page_url(const CityInfo& city) {
  if (!place_usable(&city)) return "";
  char buf[384];
  std::snprintf(buf, sizeof(buf), "%s/world/%s/%d/%s-prayer-times/", kIslamicFinderOrigin,
                city.islamicfinder_country_slug, city.islamicfinder_city_id,
                city.islamicfinder_city_slug);
  return buf;
}

void apply_islamicfinder_place(Location* loc) {
  if (!loc) return;
  const CityInfo* p = islamicfinder_place_for_location(*loc);
  if (!p) return;
  loc->islamicfinder_city_id = p->islamicfinder_city_id;
}

void apply_islamicfinder_place(AppConfig* cfg) {
  if (!cfg) return;
  Location loc = cfg->location();
  apply_islamicfinder_place(&loc);
  cfg->islamicfinder_city_id = loc.islamicfinder_city_id;
}

std::string iso2_to_country(const std::string& iso2) {
  if (iso2 == "TR") return "Turkey";
  if (iso2 == "SA") return "Saudi Arabia";
  if (iso2 == "AE") return "United Arab Emirates";
  if (iso2 == "EG") return "Egypt";
  if (iso2 == "DE") return "Germany";
  if (iso2 == "GB" || iso2 == "UK") return "United Kingdom";
  if (iso2 == "FR") return "France";
  if (iso2 == "NL") return "Netherlands";
  if (iso2 == "BA") return "Bosnia and Herzegovina";
  if (iso2 == "US") return "United States";
  if (iso2 == "CA") return "Canada";
  return "";
}

}  // namespace adhan
