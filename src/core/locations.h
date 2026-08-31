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
};

const CityInfo* city_table(size_t* count);
std::vector<std::string> country_list();
std::vector<const CityInfo*> cities_in_country(const std::string& country);
const CityInfo* find_city(const std::string& country, const std::string& city);
const CityInfo* nearest_city(double latitude, double longitude);
const CityInfo* default_city();
std::string iso2_to_country(const std::string& iso2);

}  // namespace adhan
