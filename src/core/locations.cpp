#include "locations.h"

#include <cmath>
#include <cstring>

namespace adhan {
namespace {

const CityInfo kCities[] = {
    {"Turkey", "Adana", 37.0000, 35.3213, "Europe/Istanbul"},
    {"Turkey", "Adiyaman", 37.7648, 38.2786, "Europe/Istanbul"},
    {"Turkey", "Afyonkarahisar", 38.7569, 30.5387, "Europe/Istanbul"},
    {"Turkey", "Agri", 39.7191, 43.0503, "Europe/Istanbul"},
    {"Turkey", "Aksaray", 38.3687, 34.0370, "Europe/Istanbul"},
    {"Turkey", "Amasya", 40.6499, 35.8353, "Europe/Istanbul"},
    {"Turkey", "Ankara", 39.9334, 32.8597, "Europe/Istanbul"},
    {"Turkey", "Antalya", 36.8969, 30.6966, "Europe/Istanbul"},
    {"Turkey", "Ardahan", 41.1105, 42.7022, "Europe/Istanbul"},
    {"Turkey", "Artvin", 41.1828, 41.8183, "Europe/Istanbul"},
    {"Turkey", "Aydin", 37.8560, 27.8416, "Europe/Istanbul"},
    {"Turkey", "Balikesir", 39.6484, 27.8826, "Europe/Istanbul"},
    {"Turkey", "Bartin", 41.6358, 32.3375, "Europe/Istanbul"},
    {"Turkey", "Batman", 37.8812, 41.1351, "Europe/Istanbul"},
    {"Turkey", "Bayburt", 40.2552, 40.2249, "Europe/Istanbul"},
    {"Turkey", "Bilecik", 40.1506, 29.9793, "Europe/Istanbul"},
    {"Turkey", "Bingol", 38.8855, 40.4966, "Europe/Istanbul"},
    {"Turkey", "Bitlis", 38.4006, 42.1095, "Europe/Istanbul"},
    {"Turkey", "Bolu", 40.7392, 31.6089, "Europe/Istanbul"},
    {"Turkey", "Burdur", 37.4613, 30.0665, "Europe/Istanbul"},
    {"Turkey", "Bursa", 40.1826, 29.0665, "Europe/Istanbul"},
    {"Turkey", "Canakkale", 40.1553, 26.4142, "Europe/Istanbul"},
    {"Turkey", "Cankiri", 40.6013, 33.6134, "Europe/Istanbul"},
    {"Turkey", "Corum", 40.5506, 34.9556, "Europe/Istanbul"},
    {"Turkey", "Denizli", 37.7765, 29.0864, "Europe/Istanbul"},
    {"Turkey", "Diyarbakir", 37.9144, 40.2306, "Europe/Istanbul"},
    {"Turkey", "Duzce", 40.8438, 31.1565, "Europe/Istanbul"},
    {"Turkey", "Edirne", 41.6771, 26.5557, "Europe/Istanbul"},
    {"Turkey", "Elazig", 38.6810, 39.2264, "Europe/Istanbul"},
    {"Turkey", "Erzincan", 39.7464, 39.4911, "Europe/Istanbul"},
    {"Turkey", "Erzurum", 39.9043, 41.2679, "Europe/Istanbul"},
    {"Turkey", "Eskisehir", 39.7767, 30.5206, "Europe/Istanbul"},
    {"Turkey", "Gaziantep", 37.0662, 37.3833, "Europe/Istanbul"},
    {"Turkey", "Giresun", 40.9128, 38.3895, "Europe/Istanbul"},
    {"Turkey", "Gumushane", 40.4608, 39.4797, "Europe/Istanbul"},
    {"Turkey", "Hakkari", 37.5770, 43.7365, "Europe/Istanbul"},
    {"Turkey", "Hatay", 36.2023, 36.1613, "Europe/Istanbul"},
    {"Turkey", "Igdir", 39.9237, 44.0450, "Europe/Istanbul"},
    {"Turkey", "Isparta", 37.7648, 30.5566, "Europe/Istanbul"},
    {"Turkey", "Istanbul", 41.0082, 28.9784, "Europe/Istanbul"},
    {"Turkey", "Izmir", 38.4192, 27.1287, "Europe/Istanbul"},
    {"Turkey", "Kahramanmaras", 37.5858, 36.9371, "Europe/Istanbul"},
    {"Turkey", "Karabuk", 41.2061, 32.6204, "Europe/Istanbul"},
    {"Turkey", "Karaman", 37.1810, 33.2150, "Europe/Istanbul"},
    {"Turkey", "Kars", 40.6019, 43.0975, "Europe/Istanbul"},
    {"Turkey", "Kastamonu", 41.3766, 33.7765, "Europe/Istanbul"},
    {"Turkey", "Kayseri", 38.7205, 35.4826, "Europe/Istanbul"},
    {"Turkey", "Kilis", 36.7184, 37.1212, "Europe/Istanbul"},
    {"Turkey", "Kirikkale", 39.8468, 33.5153, "Europe/Istanbul"},
    {"Turkey", "Kirklareli", 41.7355, 27.2247, "Europe/Istanbul"},
    {"Turkey", "Kirsehir", 39.1468, 34.1595, "Europe/Istanbul"},
    {"Turkey", "Kocaeli", 40.7654, 29.9408, "Europe/Istanbul"},
    {"Turkey", "Konya", 37.8746, 32.4932, "Europe/Istanbul"},
    {"Turkey", "Kutahya", 39.4192, 29.9857, "Europe/Istanbul"},
    {"Turkey", "Malatya", 38.3552, 38.3095, "Europe/Istanbul"},
    {"Turkey", "Manisa", 38.6191, 27.4289, "Europe/Istanbul"},
    {"Turkey", "Mardin", 37.3212, 40.7245, "Europe/Istanbul"},
    {"Turkey", "Mersin", 36.8121, 34.6415, "Europe/Istanbul"},
    {"Turkey", "Mugla", 37.2153, 28.3636, "Europe/Istanbul"},
    {"Turkey", "Mus", 38.7433, 41.5065, "Europe/Istanbul"},
    {"Turkey", "Nevsehir", 38.6244, 34.7239, "Europe/Istanbul"},
    {"Turkey", "Nigde", 37.9667, 34.6794, "Europe/Istanbul"},
    {"Turkey", "Ordu", 40.9839, 37.8764, "Europe/Istanbul"},
    {"Turkey", "Osmaniye", 37.0746, 36.2464, "Europe/Istanbul"},
    {"Turkey", "Rize", 41.0201, 40.5234, "Europe/Istanbul"},
    {"Turkey", "Sakarya", 40.7569, 30.3783, "Europe/Istanbul"},
    {"Turkey", "Samsun", 41.2867, 36.3300, "Europe/Istanbul"},
    {"Turkey", "Sanliurfa", 37.1674, 38.7955, "Europe/Istanbul"},
    {"Turkey", "Siirt", 37.9333, 41.9500, "Europe/Istanbul"},
    {"Turkey", "Sinop", 42.0231, 35.1531, "Europe/Istanbul"},
    {"Turkey", "Sirnak", 37.5164, 42.4611, "Europe/Istanbul"},
    {"Turkey", "Sivas", 39.7477, 37.0179, "Europe/Istanbul"},
    {"Turkey", "Tekirdag", 40.9780, 27.5110, "Europe/Istanbul"},
    {"Turkey", "Tokat", 40.3167, 36.5500, "Europe/Istanbul"},
    {"Turkey", "Trabzon", 41.0015, 39.7178, "Europe/Istanbul"},
    {"Turkey", "Tunceli", 39.1079, 39.5401, "Europe/Istanbul"},
    {"Turkey", "Usak", 38.6823, 29.4082, "Europe/Istanbul"},
    {"Turkey", "Van", 38.4891, 43.4089, "Europe/Istanbul"},
    {"Turkey", "Yalova", 40.6550, 29.2769, "Europe/Istanbul"},
    {"Turkey", "Yozgat", 39.8200, 34.8083, "Europe/Istanbul"},
    {"Turkey", "Zonguldak", 41.4564, 31.7987, "Europe/Istanbul"},
    {"Saudi Arabia", "Makkah", 21.4225, 39.8262, "Asia/Riyadh"},
    {"Saudi Arabia", "Madinah", 24.4672, 39.6111, "Asia/Riyadh"},
    {"Saudi Arabia", "Riyadh", 24.7136, 46.6753, "Asia/Riyadh"},
    {"United Arab Emirates", "Dubai", 25.2048, 55.2708, "Asia/Dubai"},
    {"Egypt", "Cairo", 30.0444, 31.2357, "Africa/Cairo"},
    {"Germany", "Berlin", 52.5200, 13.4050, "Europe/Berlin"},
    {"Germany", "Cologne", 50.9375, 6.9603, "Europe/Berlin"},
    {"United Kingdom", "London", 51.5074, -0.1278, "Europe/London"},
    {"France", "Paris", 48.8566, 2.3522, "Europe/Paris"},
    {"Netherlands", "Amsterdam", 52.3676, 4.9041, "Europe/Amsterdam"},
    {"Bosnia and Herzegovina", "Sarajevo", 43.8563, 18.4131, "Europe/Sarajevo"},
    {"United States", "New York", 40.7128, -74.0060, "America/New_York"},
    {"United States", "Chicago", 41.8781, -87.6298, "America/Chicago"},
    {"United States", "Los Angeles", 34.0522, -118.2437, "America/Los_Angeles"},
    {"Canada", "Toronto", 43.6532, -79.3832, "America/Toronto"},
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
