#pragma once

#include <string>
#include <vector>

#include "types.h"

namespace adhan {

struct CityInfo {
  const char* country;
  const char* city;
  double latitude;
  double longitude;
  const char* timezone;
  int islamicfinder_city_id;
  const char* islamicfinder_country_slug;
  const char* islamicfinder_city_slug;
};

const CityInfo* city_table(size_t* count);
std::vector<std::string> country_list();
std::vector<const CityInfo*> cities_in_country(const std::string& country);
const CityInfo* find_city(const std::string& country, const std::string& city);
const CityInfo* nearest_city(double latitude, double longitude);
const CityInfo* find_islamicfinder_city_id(int city_id);
const CityInfo* default_city();
std::string iso2_to_country(const std::string& iso2);
const CityInfo* islamicfinder_place_for_location(const Location& loc);
std::string islamicfinder_prayer_page_url(const CityInfo& city);
void apply_islamicfinder_place(Location* loc);
void apply_islamicfinder_place(struct AppConfig* cfg);

}  // namespace adhan
