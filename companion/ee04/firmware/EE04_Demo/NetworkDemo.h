#pragma once
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
#include <time.h>
#include <math.h>
#include <functional>
#include <memory>
#include <new>
#include "ImageLogic.h"
#include "ContentLogic.h"
#include "WeatherCache.h"
#include "PortalPage.h"
#include "PassportLink.h"

class NetworkDemo {
 public:
  String city = "Not configured", error = "Waiting for WiFi";
  String apPassword;
  float latitude = 0, longitude = 0, temperature = 0, wind = 0;
  int humidity = 0, weatherCode = -1;
  float utcOffset = 8; // User can edit the fixed offset in the setup page.
  bool hasCity = false, hasWeather = false, apActive = false, dirty = false;
  bool weatherRestored = false, noteRequested = false;
  bool cacheLoadedAtBoot = false;
  String note;
  PassportLink passport;
  bool passportNote = false, passportDone = false;
  uint32_t passportId = 0;
  uint32_t noteRevision = 0;
  time_t noteSavedAt = 0;
  String storageError;
  EE04Image::Record image;
  bool hasImage = false, imageRequested = false, imageLoadedAtBoot = false;
  uint32_t imageRevision() const { return hasImage ? EE04Image::revision(image) : 0; }
  std::function<void()> frameHandler;
  time_t observedAt = 0, receivedAt = 0;
  WebServer server{80}; // Only listens during the explicitly opened 10min editor window.

  bool clockReady() const { return time(nullptr) > 1704067200; }
  bool stale() const { return weatherIsStale(hasWeather, connected(), clockReady(), weatherRestored,
                                           !error.isEmpty(), time(nullptr), receivedAt); }
  bool connected() const { return WiFi.status() == WL_CONNECTED; }
  int restoredPage() { int p = prefs.getUChar("page", 0); return p < 8 && p != 5 ? p : 0; }
  void rememberPage(int page) { if (page >= 0 && page < 8 && page != 5) prefs.putUChar("page", page); }
  void printEditorConnection() const {
    // Explicit USB command only; never include the saved upstream Wi-Fi password.
    Serial.printf("EDITOR {\"ip\":\"%s\",\"active\":%s,\"key\":\"%s\"}\n",
                  WiFi.localIP().toString().c_str(), apActive ? "true" : "false",
                  apActive ? apPassword.c_str() : "");
  }
  uint32_t apSecondsLeft() const {
    return apActive ? (600000 - min(uint32_t(millis() - apStarted), uint32_t(600000))) / 1000 : 0;
  }
  bool requestWeather() {
    if (!connected() || !clockReady() || !hasCity) return false;
    if (attemptMade && uint32_t(millis() - lastAttempt) < 60000) return false;
    attemptMade = false;
    return true;
  }
  String statusText() const {
    if (!connected()) return "WiFi offline";
    if (!clockReady()) return "Waiting for NTP";
    if (!hasCity) return "Select weather city";
    return error.isEmpty() ? "Online" : error;
  }

  void localClock(struct tm& result, time_t value = 0) const {
    if (!value) value = time(nullptr);
    value += int(utcOffset * 3600);
    gmtime_r(&value, &result);
  }
  const char* description() const {
    if (weatherCode == 0) return "Clear";
    if (weatherCode <= 3 && weatherCode >= 1) return "Cloudy";
    if (weatherCode == 45 || weatherCode == 48) return "Fog";
    if (weatherCode >= 51 && weatherCode <= 57) return "Drizzle";
    if (weatherCode >= 61 && weatherCode <= 67) return "Rain";
    if (weatherCode >= 71 && weatherCode <= 77) return "Snow";
    if (weatherCode >= 80 && weatherCode <= 82) return "Rain showers";
    if (weatherCode == 85 || weatherCode == 86) return "Snow showers";
    if (weatherCode >= 95 && weatherCode <= 99) return "Thunderstorm";
    return "Unknown";
  }
  const char* descriptionZh() const {
    if (weatherCode == 0) return "晴";
    if (weatherCode >= 1 && weatherCode <= 3) return "多云";
    if (weatherCode == 45 || weatherCode == 48) return "雾";
    if (weatherCode >= 51 && weatherCode <= 57) return "毛毛雨";
    if (weatherCode >= 61 && weatherCode <= 67) return "雨";
    if (weatherCode >= 71 && weatherCode <= 77) return "雪";
    if (weatherCode >= 80 && weatherCode <= 82) return "阵雨";
    if (weatherCode == 85 || weatherCode == 86) return "阵雪";
    if (weatherCode >= 95 && weatherCode <= 99) return "雷雨";
    return "未知";
  }

  void begin() {
    prefs.begin("ee04-demo", false);
    ssid = prefs.getString("ssid", "");
    password = prefs.getString("pass", "");
    city = prefs.getString("city", "Not configured");
    latitude = prefs.getFloat("lat", 0);
    longitude = prefs.getFloat("lon", 0);
    utcOffset = prefs.getFloat("offset", 8);
    hasCity = prefs.getBool("hasCity", false);
    restoreContent();
    passport.begin([this](const String& text, uint32_t id, bool done) {
      if (note != text || !passportNote || passportId != id || passportDone != done) {
        JsonDocument doc; uint32_t revision = noteRevision + 1; if (!revision) revision = 1;
        time_t at = clockReady() ? time(nullptr) : 0;
        doc["v"] = 1; doc["text"] = text; doc["rev"] = revision; doc["at"] = int64_t(at);
        doc["passport"] = true; doc["done"] = done; doc["id"] = id;
        String json; serializeJson(doc,json);
        if (prefs.putString("note-v1",json) != json.length()) return false;
        note = text; noteRevision = revision; noteSavedAt = at;
        passportNote = true; passportDone = done; passportId = id;
      }
      noteRequested = true; dirty = true; return true;
    });
    WiFi.persistent(false);
    WiFi.setAutoReconnect(true);
    WiFi.mode(WIFI_STA);
    configTime(0, 0, "pool.ntp.org", "time.cloudflare.com", "time.google.com");
    if (ssid.length()) connectWiFi();
    else startSetup();
    const char* headers[] = {"X-EE04-Token"};
    server.collectHeaders(headers, 1);
    server.on("/", HTTP_GET, [this]() { if (authorize()) server.send_P(200, "text/html; charset=utf-8", EE04_PORTAL); });
    server.on("/state", HTTP_GET, [this]() { if (authorize()) sendState(); });
    server.on("/search", HTTP_GET, [this]() { if (authorize()) searchCity(); });
    server.on("/save", HTTP_POST, [this]() { if (authorize(true)) save(); });
    server.on("/passport/pair", HTTP_POST, [this]() {
      if (!authorize(true)) return;
      if (!passport.pair(server.arg("enable") == "1")) { server.send(500,"text/plain","配对保存失败"); return; }
      server.send(200,"application/json","{\"key\":\"" + passport.key + "\"}");
    });
    server.on("/note", HTTP_POST, [this]() { if (authorize(true)) saveNote(); });
    server.on("/image", HTTP_GET, [this]() {
      if (!authorize()) return;
      if (!hasImage) { server.send(404, "text/plain", "尚未保存图片"); return; }
      server.setContentLength(EE04Image::BYTES);
      server.send(200, "application/octet-stream", "");
      server.sendContent(reinterpret_cast<const char*>(image.pixels()), EE04Image::BYTES);
    });
    server.on("/image", HTTP_POST, [this]() { if (authorize(true)) saveImage(); });
    server.on("/image/show", HTTP_POST, [this]() {
      if (!authorize(true)) return;
      if (!hasImage) { server.send(404, "text/plain", "尚未保存图片"); return; }
      imageRequested = true; apStarted = millis();
      server.send(200, "application/json", "{\"shown\":true}");
    });
    server.on("/image/clear", HTTP_POST, [this]() {
      if (!authorize(true)) return;
      if (prefs.isKey("image-v1") && !prefs.remove("image-v1")) {
        server.send(500, "text/plain", "清除失败，请重试"); return;
      }
      hasImage = false; imageRequested = true; apStarted = millis();
      server.send(200, "application/json", "{\"cleared\":true}");
    });
    server.on("/screen", HTTP_GET, [this]() {
      if (!authorize()) return;
      if (frameHandler) frameHandler(); else server.send(503, "text/plain", "Screen not ready");
    });
    server.onNotFound([this]() { server.send(404, "text/plain", "Not found"); });
  }

  void startSetup() {
    if (apActive) { apStarted = millis(); dirty = true; return; }
    char key[16];
    snprintf(key, sizeof(key), "EE04%08lx", (unsigned long)esp_random());
    apPassword = key;
    WiFi.mode(WIFI_AP_STA);
    apActive = WiFi.softAP("EE04-Setup", apPassword.c_str());
    if (apActive) { server.begin(); apStarted = millis(); }
    dirty = true;
  }

  void tick() {
    passport.tick(connected());
    if (apActive) {
      server.handleClient();
      if (uint32_t(millis() - apStarted) >= 600000) {
        server.stop(); WiFi.softAPdisconnect(true); WiFi.mode(WIFI_STA);
        apActive = false; dirty = true;
      }
    }
    if (reconnectPending) { reconnectPending = false; connectWiFi(); }
    bool online = connected();
    if (online != wasOnline) {
      wasOnline = online; dirty = true;
      if (online) { configTime(0, 0, "pool.ntp.org", "time.cloudflare.com", "time.google.com"); attemptMade = false; }
      else error = "WiFi offline";
    }
    if (!online && ssid.length() && uint32_t(millis() - connectStarted) > 30000 && !apActive && !fallbackOffered) {
      fallbackOffered = true; startSetup();
    }
    if (!online) return;
    if (!hasCity) { error = "Select city in SETUP"; return; }
    if (!clockReady()) { error = "Waiting for NTP"; return; }
    uint32_t interval = error.isEmpty() ? 900000 : 60000;
    if (!attemptMade || uint32_t(millis() - lastAttempt) >= interval) {
      attemptMade = true; lastAttempt = millis(); fetchWeather(); dirty = true;
    }
  }

 private:
  Preferences prefs;
  String ssid, password;
  bool reconnectPending = false, wasOnline = false, attemptMade = false, fallbackOffered = false;
  uint32_t lastAttempt = 0, connectStarted = 0, apStarted = 0;

  bool authorize(bool write = false) {
    if (!apActive) { server.send(403, "text/plain", "请先长按K2开启手机管理"); return false; }
    bool fromAP = server.client().localIP() == WiFi.softAPIP();
    if (!fromAP && !server.authenticate("ee04", apPassword.c_str())) {
      server.requestAuthentication(BASIC_AUTH, "EE04", "用户名ee04，密码见屏幕设置页"); return false;
    }
    if (write && server.header("X-EE04-Token") != apPassword) {
      server.send(403, "text/plain", "会话已过期，请刷新页面"); return false;
    }
    server.sendHeader("Cache-Control", "no-store");
    return true;
  }

  void restoreContent() {
    JsonDocument doc;
    String json = prefs.getString("note-v1", "");
    if (json.length() && json.length() <= 2048 && !deserializeJson(doc, json) &&
        doc["v"] == 1 && doc["text"].is<String>() && doc["rev"].is<uint32_t>()) {
      std::string clean;
      String text = doc["text"].as<String>();
      if (!normalizeNote(std::string(text.c_str(), text.length()), clean)) {
        note = clean.c_str(); noteRevision = doc["rev"]; noteSavedAt = doc["at"] | int64_t(0);
        passportNote = doc["passport"] | false; passportDone = doc["done"] | false; passportId = doc["id"] | uint32_t(0);
      }
    }
    // A single small blob keeps the bitmap and its version/checksum together.
    // No partition changes or filesystem formatting: preserve existing settings.
    if (prefs.isKey("image-v1")) {
      hasImage = prefs.getBytesLength("image-v1") == image.bytes.size() &&
          prefs.getBytes("image-v1", image.bytes.data(), image.bytes.size()) == image.bytes.size() &&
          EE04Image::valid(image);
      imageLoadedAtBoot = hasImage;
      if (!hasImage) storageError = "图片缓存无效，请重新上传";
    }
    if (!hasCity) return;
    json = prefs.getString("weather-v1", "");
    WeatherSnapshot s;
    if (!decodeWeatherCache(json.c_str(), latitude, longitude, s)) return;
    temperature = s.temperature; humidity = s.humidity; wind = s.wind; weatherCode = s.code;
    observedAt = s.observed; receivedAt = s.received;
    hasWeather = true; weatherRestored = true; cacheLoadedAtBoot = true;
  }

  void storeWeather() {
    WeatherSnapshot s{latitude, longitude, temperature, wind, humidity, weatherCode, observedAt, receivedAt};
    String json = encodeWeatherCache(s).c_str();
    if (prefs.putString("weather-v1", json) != json.length()) storageError = "天气缓存保存失败";
  }

  void saveNote() {
    if (!server.hasArg("text")) { server.send(400, "text/plain", "Missing text"); return; }
    String input = server.arg("text"); std::string clean;
    if (const char* issue = normalizeNote(std::string(input.c_str(), input.length()), clean)) {
      server.send(400, "text/plain; charset=utf-8", issue); return;
    }
    if (String(clean.c_str()) != note || passportNote) {
      JsonDocument doc;
      uint32_t revision = noteRevision + 1;
      if (!revision) revision = 1;
      time_t at = clockReady() ? time(nullptr) : 0;
      doc["v"] = 1; doc["text"] = clean; doc["rev"] = revision; doc["at"] = int64_t(at);
      String json; serializeJson(doc, json);
      if (prefs.putString("note-v1", json) != json.length()) {
        server.send(500, "text/plain; charset=utf-8", "保存失败，请重试；原便签仍保留在内存中"); return;
      }
      note = clean.c_str(); noteRevision = revision; noteSavedAt = at;
      passportNote = false; passportDone = false; passportId = 0;
    }
    noteRequested = true; dirty = true; apStarted = millis();
    server.send(200, "application/json", "{\"saved\":true,\"revision\":" + String(noteRevision) + "}");
  }

  void saveImage() {
    const String& hex = server.arg("hex");
    if (hex.length() != EE04Image::BYTES * 2) {
      server.send(400, "text/plain", "需要296×128的一位黑白图片"); return;
    }
    // Keep the ~4.8KB candidate off the Arduino loop task's limited stack.
    std::unique_ptr<EE04Image::Record> candidate(new (std::nothrow) EE04Image::Record);
    if (!candidate) { server.send(503, "text/plain", "内存不足，请稍后重试"); return; }
    if (!EE04Image::decodeHex(hex.c_str(), hex.length(), *candidate)) {
      server.send(400, "text/plain", "图片数据格式错误"); return;
    }
    if (!hasImage || memcmp(candidate->pixels(), image.pixels(), EE04Image::BYTES) != 0) {
      EE04Image::seal(*candidate, imageRevision() + 1);
      if (prefs.putBytes("image-v1", candidate->bytes.data(), candidate->bytes.size()) != candidate->bytes.size()) {
        server.send(500, "text/plain", "图片保存失败，请重试"); return;
      }
      image = *candidate; hasImage = true;
    }
    imageRequested = true; apStarted = millis();
    server.send(200, "application/json", "{\"saved\":true,\"revision\":" + String(imageRevision()) + "}");
  }

  void connectWiFi() {
    WiFi.disconnect();
    WiFi.begin(ssid.c_str(), password.c_str());
    connectStarted = millis();
    error = "Connecting WiFi";
  }
  static String encode(const String& text) {
    String out;
    const char* hex = "0123456789ABCDEF";
    for (size_t i = 0; i < text.length(); ++i) {
      uint8_t c = text[i];
      if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_') out += char(c);
      else { out += '%'; out += hex[c >> 4]; out += hex[c & 15]; }
    }
    return out;
  }
  static esp_err_t onHttp(esp_http_client_event_t* event) {
    if (event->event_id == HTTP_EVENT_ON_DATA) {
      auto* data = static_cast<String*>(event->user_data);
      if (data->length() + event->data_len > 24000) return ESP_FAIL;
      data->concat(static_cast<const char*>(event->data), event->data_len);
    }
    return ESP_OK;
  }
  bool getJson(const String& url, JsonDocument& doc) {
    if (!connected()) { error = "WiFi offline"; return false; }
    if (!clockReady()) { error = "Waiting for NTP"; return false; }
    String response;
    esp_http_client_config_t config = {};
    config.url = url.c_str();
    config.crt_bundle_attach = esp_crt_bundle_attach; // Full trusted roots, hostname/date verification.
    config.timeout_ms = 7000;
    config.disable_auto_redirect = true;
    config.event_handler = onHttp;
    config.user_data = &response;
    auto client = esp_http_client_init(&config);
    if (!client) { error = "HTTP allocation failed"; return false; }
    esp_err_t result = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    if (result != ESP_OK || status != 200) {
      error = result != ESP_OK ? "HTTPS connection failed" : "HTTP " + String(status);
      return false;
    }
    if (deserializeJson(doc, response)) { error = "Invalid weather JSON"; return false; }
    return true;
  }
  void fetchWeather() {
    String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(latitude, 5) +
        "&longitude=" + String(longitude, 5) +
        "&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m&timeformat=unixtime&forecast_days=1";
    JsonDocument doc;
    if (!getJson(url, doc)) { Serial.println(error); return; }
    JsonObject current = doc["current"];
    if (!current["temperature_2m"].is<float>() || !current["relative_humidity_2m"].is<int>() ||
        !current["weather_code"].is<int>() || !current["wind_speed_10m"].is<float>() || !current["time"].is<long>()) {
      error = "Missing weather fields"; return;
    }
    if (!validWeatherValues(current["temperature_2m"], current["relative_humidity_2m"],
                            current["wind_speed_10m"], current["weather_code"],
                            current["time"].as<int64_t>(), time(nullptr))) {
      error = "Invalid weather values"; return;
    }
    temperature = current["temperature_2m"];
    humidity = current["relative_humidity_2m"];
    wind = current["wind_speed_10m"];
    weatherCode = current["weather_code"];
    observedAt = current["time"].as<long>();
    receivedAt = time(nullptr);
    hasWeather = true; weatherRestored = false; error = "";
    storeWeather();
    Serial.println("Weather updated (Open-Meteo)");
  }
  void sendState() {
    JsonDocument doc;
    doc["ssid"] = ssid; // Never return the stored password.
    doc["online"] = connected(); doc["clockReady"] = clockReady();
    doc["city"] = city; doc["lat"] = latitude; doc["lon"] = longitude;
    doc["hasCity"] = hasCity; doc["offset"] = utcOffset; doc["error"] = statusText();
    doc["ip"] = connected() ? WiFi.localIP().toString() : "";
    doc["rssi"] = connected() ? WiFi.RSSI() : 0;
    doc["apSeconds"] = apSecondsLeft();
    doc["token"] = apPassword;
    doc["passportPaired"] = passport.key.length() == 32;
    doc["note"] = note; doc["noteRevision"] = noteRevision; doc["noteSavedAt"] = int64_t(noteSavedAt);
    doc["hasImage"] = hasImage; doc["imageRevision"] = imageRevision();
    doc["imageLoadedAtBoot"] = imageLoadedAtBoot;
    doc["hasWeather"] = hasWeather; doc["weatherStale"] = stale(); doc["weatherRestored"] = weatherRestored;
    doc["cacheLoadedAtBoot"] = cacheLoadedAtBoot;
    doc["weatherReceivedAt"] = int64_t(receivedAt); doc["storageError"] = storageError;
    String json; serializeJson(doc, json);
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", json);
  }
  void searchCity() {
    String query = server.arg("q"); query.trim();
    if (query.length() < 2 || query.length() > 80) { server.send(400, "text/plain", "Enter 2-80 characters"); return; }
    JsonDocument doc;
    String previousError = error;
    if (!getJson("https://geocoding-api.open-meteo.com/v1/search?count=5&language=zh&format=json&name=" + encode(query), doc)) {
      server.send(503, "text/plain", error); error = previousError; return;
    }
    String json; serializeJson(doc, json);
    server.send(200, "application/json", json);
    error = previousError;
  }
  static bool number(const String& input, float& output) {
    if (input.isEmpty()) return false;
    char* end = nullptr;
    output = strtof(input.c_str(), &end);
    return end && *end == '\0' && isfinite(output);
  }
  void save() {
    String newSsid = server.arg("ssid"), newPass = server.arg("password");
    float newOffset, newLat = 0, newLon = 0;
    bool selected = server.arg("selected") == "1";
    if (newSsid.isEmpty() || newSsid.length() > 32 || newPass.length() > 63 ||
        !number(server.arg("offset"), newOffset) || newOffset < -12 || newOffset > 14) {
      server.send(400, "text/plain", "Invalid SSID, password or UTC offset"); return;
    }
    if (selected && (!number(server.arg("lat"), newLat) || !number(server.arg("lon"), newLon) ||
        newLat < -90 || newLat > 90 || newLon < -180 || newLon > 180 || server.arg("city").isEmpty() || server.arg("city").length() > 120)) {
      server.send(400, "text/plain", "Select a valid city"); return;
    }
    if (newPass.isEmpty() && newSsid != ssid && server.arg("open") != "1") {
      server.send(400, "text/plain", "Enter a Wi-Fi password or mark the network as open"); return;
    }
    if (server.arg("open") == "1") newPass = "";
    else if (newPass.isEmpty() && newSsid == ssid) newPass = password;
    if (newPass.length() > 0 && newPass.length() < 8) {
      server.send(400, "text/plain", "Wi-Fi password must contain at least 8 characters"); return;
    }
    bool changedWifi = newSsid != ssid || newPass != password;
    ssid = newSsid; password = newPass; utcOffset = newOffset;
    prefs.putString("ssid", ssid); prefs.putString("pass", password); prefs.putFloat("offset", utcOffset);
    if (selected) {
      bool changedCity = !hasCity || fabs(newLat - latitude) > 0.0001f || fabs(newLon - longitude) > 0.0001f;
      city = server.arg("city"); latitude = newLat; longitude = newLon; hasCity = true;
      prefs.putString("city", city); prefs.putFloat("lat", latitude); prefs.putFloat("lon", longitude); prefs.putBool("hasCity", true);
      if (changedCity) { hasWeather = false; weatherRestored = false; prefs.remove("weather-v1"); }
      attemptMade = false;
    }
    reconnectPending = changedWifi || !connected();
    dirty = true; apStarted = millis();
    server.send(200, "text/plain", "Saved. If this is your first Wi-Fi setup, wait for Wi-Fi + NTP, then search and save your city.");
  }

};
