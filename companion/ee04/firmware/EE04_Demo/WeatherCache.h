#pragma once
#include <ArduinoJson.h>
#include "ContentLogic.h"

struct WeatherSnapshot {
  float latitude = 0, longitude = 0, temperature = 0, wind = 0;
  int humidity = 0, code = 0;
  int64_t observed = 0, received = 0;
};

inline std::string encodeWeatherCache(const WeatherSnapshot& s) {
  JsonDocument doc;
  doc["v"] = 1; doc["lat"] = s.latitude; doc["lon"] = s.longitude;
  doc["temp"] = s.temperature; doc["rh"] = s.humidity; doc["wind"] = s.wind; doc["code"] = s.code;
  doc["observed"] = s.observed; doc["received"] = s.received;
  std::string json; serializeJson(doc, json); return json;
}

inline bool decodeWeatherCache(const std::string& json, float latitude, float longitude, WeatherSnapshot& out) {
  if (json.empty() || json.size() > 2048) return false;
  JsonDocument doc;
  if (deserializeJson(doc, json) || doc["v"] != 1 ||
      !doc["lat"].is<float>() || !doc["lon"].is<float>() || !doc["temp"].is<float>() ||
      !doc["rh"].is<int>() || !doc["wind"].is<float>() || !doc["code"].is<int>() ||
      !doc["observed"].is<int64_t>() || !doc["received"].is<int64_t>()) return false;
  WeatherSnapshot s;
  s.latitude = doc["lat"]; s.longitude = doc["lon"]; s.temperature = doc["temp"];
  s.humidity = doc["rh"]; s.wind = doc["wind"]; s.code = doc["code"];
  s.observed = doc["observed"]; s.received = doc["received"];
  if (!isfinite(s.latitude) || !isfinite(s.longitude) || fabs(s.latitude-latitude) > .0001f ||
      fabs(s.longitude-longitude) > .0001f || !validWeatherValues(s.temperature, s.humidity, s.wind, s.code, s.observed, s.received)) return false;
  out = s; return true;
}
