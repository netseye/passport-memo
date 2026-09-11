// EE04 V1.2 + E029A01-FPC-A1 (GDEH029A1 / GxEPD2_290).
#include <Arduino.h>
#include <SPI.h>
#include <GxEPD2_BW.h>
#include "TracedPanel290.h"
#include "DemoLogic.h"
#include "NetworkDemo.h"
#include "ChineseUI.h"
#include "RefreshLogic.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#if !defined(CONFIG_IDF_TARGET_ESP32S3)
#error "Select XIAO_ESP32S3"
#endif
constexpr int SCK_PIN = 7, MOSI_PIN = 9, CS_PIN = 44, DC_PIN = 10;
constexpr int RST_PIN = 38, BUSY_PIN = 4, POWER_PIN = 43;
constexpr int BAT_PIN = 1, ADC_EN_PIN = 6;
constexpr int KEY_PINS[] = {2, 3, 5};
// EE04 V1.2: R28/R29 = 10k/10k. analogReadMilliVolts uses ADC calibration.
// Adjust CALIBRATION using a multimeter: actual voltage / displayed voltage.
constexpr float BAT_DIVIDER = 2.0f, CALIBRATION = 1.0f;
// Render to a logical canvas before touching the panel; compare real pixels.
GFXcanvas1 display(296, 128);
TracedPanel290 panel(CS_PIN, DC_PIN, RST_PIN, BUSY_PIN);
uint8_t nextFrame[EE04Refresh::BYTES];
constexpr const char* DEMO_VERSION = "1.7-refresh";
struct ButtonEvent { uint8_t index; uint8_t kind; uint32_t pressedAt; };
QueueHandle_t buttonQueue = nullptr;
volatile uint32_t droppedButtonEvents = 0;

// This task never calls display, Wi-Fi or Serial. It keeps polling while loop()
// is blocked inside GxEPD2/HTTPS. Only loop() changes page/UI state.
void buttonScanner(void*) {
  ButtonGesture scanned[3];
  for (int i = 0; i < 3; ++i) scanned[i].sync(digitalRead(KEY_PINS[i]) == LOW, millis());
  for (;;) {
    uint32_t now = millis();
    for (uint8_t i = 0; i < 3; ++i) {
      uint8_t kind = scanned[i].update(digitalRead(KEY_PINS[i]) == LOW, now);
      if (kind) {
        ButtonEvent event{i, kind, now};
        if (xQueueSend(buttonQueue, &event, 0) != pdTRUE) ++droppedButtonEvents;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(5) > 0 ? pdMS_TO_TICKS(5) : 1);
  }
}
uint32_t presses[3] = {};
uint32_t refreshCount = 0, lastDuration = 0, finishedAt = 0;
uint32_t maxDuration = 0;
int renderedPage = -1;
uint32_t adcMv = 0;
float batteryV = 0;
int batteryPct = -1;
NetworkDemo network;
ChineseUI cn;
int page = 0; // Home, battery, pattern, clock, weather, setup, notes, image.
constexpr int PAGE_COUNT = 8;
constexpr int NOTE_PAGE = 6, IMAGE_PAGE = 7;
size_t noteSheet = 0;
time_t lastMinute = -1;
bool pending = true, forceFullPending = true;
bool partialEnabled = true, partialFault = false;
uint8_t partialStreak = 0;
uint32_t fullCount = 0, partialCount = 0, skippedCount = 0;
uint32_t lastFullAt = 0, lastRenderAttempt = 0, batterySampleAt = 0;
const char* lastRefreshMode = "none";

void requestFullRefresh() { pending = true; forceFullPending = true; }
bool keyDiagnostics = false;
int lastRawMask = -1;
bool panelInitialized = false;
int savedPage = -1;
uint32_t lastUserInput = 0;
String lastNetworkStatus;

void printButtonDiagnostics() {
  // One compact snapshot also records progress made while no host was reading.
  Serial.printf("STATE FW=%s up=%lu page=%d drawn=%d frames=%lu end=%lu ms=%lu max=%lu K=%d/%d/%d keys=%lu/%lu/%lu queued=%u dropped=%lu diag=%d mode=%s full=%lu part=%lu skip=%lu streak=%u auto=%d fault=%d\n",
                DEMO_VERSION, (unsigned long)millis(), page + 1, renderedPage + 1,
                (unsigned long)refreshCount, (unsigned long)finishedAt,
                (unsigned long)lastDuration, (unsigned long)maxDuration,
                digitalRead(KEY_PINS[0]), digitalRead(KEY_PINS[1]), digitalRead(KEY_PINS[2]),
                (unsigned long)presses[0], (unsigned long)presses[1], (unsigned long)presses[2],
                buttonQueue ? (unsigned)uxQueueMessagesWaiting(buttonQueue) : 0,
                (unsigned long)droppedButtonEvents, keyDiagnostics, lastRefreshMode,
                (unsigned long)fullCount, (unsigned long)partialCount, (unsigned long)skippedCount,
                partialStreak, partialEnabled, partialFault);
}

void sampleBattery() {
  batterySampleAt = millis();
  digitalWrite(ADC_EN_PIN, HIGH);
  delay(15);
  (void)analogReadMilliVolts(BAT_PIN); // Discard the first sample after enabling.
  uint32_t total = 0;
  for (int i = 0; i < 16; ++i) {
    total += analogReadMilliVolts(BAT_PIN);
    delay(2);
  }
  digitalWrite(ADC_EN_PIN, LOW);
  adcMv = (total + 8) / 16;
  batteryV = adcMv * BAT_DIVIDER * CALIBRATION / 1000.0f;
  batteryPct = batteryPercent(batteryV);
}

void label(int x, int y, const char* text, int size = 1) {
  display.setTextSize(size);
  display.setCursor(x, y);
  display.print(text);
}

void frame(const char* title) {
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
  display.drawRect(0, 0, 296, 128, GxEPD_BLACK);
  label(8, 7, title, 2);
  display.drawFastHLine(8, 29, 280, GxEPD_BLACK);
  display.drawFastHLine(8, 108, 280, GxEPD_BLACK);
  label(8, 115, "K1:PREV     K2:REFRESH     K3:NEXT");
}

void drawStatus() {
  display.fillScreen(GxEPD_WHITE); display.setTextColor(GxEPD_BLACK);
  display.drawRect(0, 0, 296, 128, GxEPD_BLACK);
  cn.small(); cn.text(8, 19, "桌面信息"); label(83, 9, "v1.7");
  cn.small(true); cn.text(170, 18, network.connected() ? "联网" : "离线", 28);
  char line[64]; snprintf(line, sizeof(line), batteryPct >= 0 ? "~%d%%" : "--%%", batteryPct);
  label(210, 10, line); label(266, 10, "1/8");
  display.drawFastHLine(8, 26, 280, GxEPD_BLACK);
  display.drawFastVLine(176, 33, 69, GxEPD_BLACK);
  if (network.clockReady()) {
    struct tm now; network.localClock(now);
    strftime(line, sizeof(line), "%H:%M", &now); label(10, 39, line, 5);
    const char* week[] = {"日", "一", "二", "三", "四", "五", "六"};
    snprintf(line, sizeof(line), "%02d月%02d日 星期%s", now.tm_mon + 1, now.tm_mday, week[now.tm_wday]);
    cn.small(); cn.text(10, 97, line, 160);
  } else {
    label(10, 39, "--:--", 5);
    cn.small(); cn.text(10, 97, "联网后自动校时", 160);
  }
  drawWeatherIcon(185, 33, network.hasWeather ? network.weatherCode : -1);
  if (network.hasWeather) {
    snprintf(line, sizeof(line), "%.0fC", network.temperature); label(230, 39, line, 2);
    std::string description = network.descriptionZh();
    if (network.stale()) description += "(缓存)";
    cn.small(); cn.text(185, 80, description, 103);
    cn.small(true); cn.text(185, 99, network.city.c_str(), 103);
  } else {
    label(234, 39, "--", 2);
    cn.small(); cn.text(185, 80, "天气待更新", 103);
    cn.small(true); cn.text(185, 99, network.hasCity ? network.city.c_str() : "手机选择城市", 103);
  }
  display.drawFastHLine(8, 108, 280, GxEPD_BLACK);
  cn.small(true);
  cn.text(8, 123, network.note.isEmpty() ? "长按K2，用手机写一张便签" : (std::string("便签：") + network.note.c_str()), 280);
  cn.small();
}

void drawWeatherIcon(int x, int y, int code) {
  if (code < 0) { display.drawCircle(x + 15, y + 15, 12, GxEPD_BLACK); label(x + 10, y + 9, "?", 2); return; }
  if (code == 0) {
    display.drawCircle(x + 16, y + 16, 8, GxEPD_BLACK);
    const int dx[] = {0, 1, 1, 1, 0, -1, -1, -1};
    const int dy[] = {-1, -1, 0, 1, 1, 1, 0, -1};
    for (int i = 0; i < 8; ++i) display.drawLine(x + 16 + dx[i] * 11, y + 16 + dy[i] * 11,
                                              x + 16 + dx[i] * 15, y + 16 + dy[i] * 15, GxEPD_BLACK);
  } else {
    display.fillCircle(x + 9, y + 16, 8, GxEPD_BLACK);
    display.fillCircle(x + 17, y + 11, 10, GxEPD_BLACK);
    display.fillCircle(x + 26, y + 17, 7, GxEPD_BLACK);
    display.fillRect(x + 8, y + 14, 20, 10, GxEPD_BLACK);
    display.fillCircle(x + 9, y + 16, 6, GxEPD_WHITE);
    display.fillCircle(x + 17, y + 11, 8, GxEPD_WHITE);
    display.fillCircle(x + 26, y + 17, 5, GxEPD_WHITE);
    display.fillRect(x + 8, y + 14, 20, 8, GxEPD_WHITE);
    if (code >= 51) for (int i = 0; i < 3; ++i) display.drawLine(x + 8 + i * 8, y + 27, x + 6 + i * 8, y + 32, GxEPD_BLACK);
    else if (code == 45 || code == 48) display.drawFastHLine(x + 3, y + 29, 28, GxEPD_BLACK);
  }
}

void chineseFrame(const char* title, const char* footer) {
  display.fillScreen(GxEPD_WHITE); display.setTextColor(GxEPD_BLACK);
  display.drawRect(0, 0, 296, 128, GxEPD_BLACK);
  cn.small(); cn.text(8, 21, title);
  display.drawFastHLine(8, 28, 280, GxEPD_BLACK);
  display.drawFastHLine(8, 109, 280, GxEPD_BLACK);
  cn.small(true); cn.text(8, 124, footer); cn.small();
}

void drawNote() {
  auto lines = cn.wrap(network.note.c_str());
  size_t sheets = (lines.size() + 3) / 4;
  noteSheet %= sheets;
  chineseFrame(network.passportNote ? (network.passportDone ? "语音备忘 · 已完成" : "语音备忘 · 待办") : "手机便签 7/8", "K1前页   K2翻张   K3后页");
  char count[20]; snprintf(count, sizeof(count), "%u/%u", (unsigned)noteSheet + 1, (unsigned)sheets);
  label(250, 12, count);
  if (network.note.isEmpty()) {
    cn.text(10, 62, "长按K2，开启手机管理");
    cn.text(10, 89, "写下你的第一张便签");
  } else {
    for (size_t row = 0; row < 4 && noteSheet * 4 + row < lines.size(); ++row)
      cn.text(10, 46 + row * 19, lines[noteSheet * 4 + row], 276);
  }
}

void drawImage() {
  if (network.hasImage) {
    display.fillScreen(GxEPD_WHITE);
    display.drawBitmap(0, 0, network.image.pixels(), EE04Image::WIDTH, EE04Image::HEIGHT, GxEPD_BLACK);
  } else {
    chineseFrame("图片展示 8/8", "K1前页   长按K2上传图片   K3首页");
    cn.text(10, 53, "用手机上传你喜欢的图片");
    cn.text(10, 79, "支持裁切、旋转和黑白预览");
    cn.small(true); cn.text(10, 101, "296×128全屏显示，重启后保留"); cn.small();
  }
}

void nextNoteSheet() {
  auto lines = cn.wrap(network.note.c_str());
  noteSheet = (noteSheet + 1) % ((lines.size() + 3) / 4);
  pending = true;
}

void drawBattery() {
  frame("EE04 / BATTERY 2/8");
  char line[48];
  snprintf(line, sizeof(line), "%.2f V", batteryV);
  label(10, 40, line, 3);
  if (batteryPct >= 0) {
    snprintf(line, sizeof(line), "~%d%%", batteryPct);
    label(177, 44, line, 2);
  } else label(177, 44, "N/A", 2);
  display.drawRect(177, 65, 90, 16, GxEPD_BLACK);
  display.fillRect(267, 70, 3, 6, GxEPD_BLACK);
  if (batteryPct > 0) display.fillRect(180, 68, batteryPct * 84 / 100, 10, GxEPD_BLACK);
  snprintf(line, sizeof(line), "ADC %lumV x %.2f", (unsigned long)adcMv, BAT_DIVIDER * CALIBRATION);
  label(10, 76, line);
  label(10, 93, batteryPct >= 0 ? "Estimate only; charging changes voltage." : "Check battery/wiring/calibration.");
}

void drawPattern() {
  frame("EE04 / PATTERN 3/8");
  for (int row = 0; row < 4; ++row)
    for (int col = 0; col < 12; ++col)
      if ((row + col) % 2 == 0) display.fillRect(10 + col * 10, 39 + row * 10, 10, 10, GxEPD_BLACK);
  for (int x = 148; x < 282; x += 4) display.drawFastVLine(x, 39, 40, GxEPD_BLACK);
  label(10, 92, "Checkerboard / lines / outer border");
}

void drawClock() {
  frame("EE04 / CLOCK 4/8");
  if (!network.clockReady()) {
    label(10, 45, "Waiting for Wi-Fi + NTP", 1);
    label(10, 68, "Open SETUP page to configure Wi-Fi.");
    return;
  }
  struct tm now;
  network.localClock(now);
  char text[40];
  strftime(text, sizeof(text), "%H:%M", &now);
  label(10, 40, text, 5);
  strftime(text, sizeof(text), "%Y-%m-%d  %a", &now);
  label(10, 87, text);
  snprintf(text, sizeof(text), "UTC%+.2f", network.utcOffset);
  label(204, 44, text);
  label(204, 65, network.connected() ? "WiFi online" : "Offline");
  label(204, 84, "1min update");
}

void drawWeather() {
  chineseFrame("天气 5/8", "K2更新天气   K3下一页");
  if (!network.hasWeather) {
    cn.text(10, 55, !network.hasCity ? "请在手机管理中选择城市" : "正在等待天气数据");
    cn.text(10, 84, "长按K2，设置网络和城市");
    return;
  }
  cn.text(10, 49, network.city.c_str(), 145);
  char text[48];
  snprintf(text, sizeof(text), "%.1f C", network.temperature);
  label(10, 60, text, 3);
  cn.text(170, 48, network.descriptionZh(), 110);
  snprintf(text, sizeof(text), "湿度 %d%%", network.humidity); cn.small(true); cn.text(170, 67, text, 110);
  snprintf(text, sizeof(text), "风 %.1fkm/h", network.wind); cn.text(170, 86, text, 110);
  struct tm observed; network.localClock(observed, network.observedAt);
  char stamp[20]; strftime(stamp, sizeof(stamp), "%m-%d %H:%M", &observed);
  snprintf(text, sizeof(text), "%s %s  Open-Meteo", network.stale() ? "缓存" : "更新", stamp);
  cn.text(10, 103, text); cn.small();
}

void drawSetup() {
  chineseFrame("手机管理 6/8", "短按K2延长开放时间，共10分钟");
  if (network.apActive) {
    label(10, 36, "WiFi: EE04-Setup");
    String pass = "Pass: " + network.apPassword;
    label(10, 50, pass.c_str());
    label(10, 65, "http://192.168.4.1");
    if (network.connected()) {
      String lan = "LAN: http://" + WiFi.localIP().toString(); label(10, 80, lan.c_str());
      cn.small(true); cn.text(10, 101, "局域网登录：ee04 / 上方密码"); cn.small();
    } else { cn.small(true); cn.text(10, 98, "连接热点后，用浏览器打开上方地址"); cn.small(); }
  } else {
    cn.text(10, 56, "按K2开启手机管理");
    cn.text(10, 84, "写便签、传图片、设置天气城市");
  }
}

void render() {
  using namespace EE04Refresh;
  const uint32_t now = millis(); lastRenderAttempt = now;
  const bool first = refreshCount == 0;
  const bool canPartial = panel.hasFrame() && panelInitialized && renderedPage == page &&
      !forceFullPending && partialEnabled && !partialFault && partialPage(page) &&
      partialStreak < MAX_PARTIALS && uint32_t(now - lastFullAt) < MAX_PARTIAL_AGE_MS;
  if (!ready(now, finishedAt, canPartial ? Mode::Partial : Mode::Full, first)) return;
  // Only pages that show battery values need a sample. Explicit refresh always samples.
  if ((page == 0 || page == 1) && (!batterySampleAt || forceFullPending ||
      page != renderedPage || uint32_t(now - batterySampleAt) >= 60000)) sampleBattery();
  if (!panelInitialized) {
    // Verbose library diagnostics are opt-in; USB TX itself never waits.
    panel.selectSPI(SPI, SPISettings(1000000, MSBFIRST, SPI_MODE0));
    panel.init(0, true, 10, false);
    panel.setTracing(panel.tracing());
    panelInitialized = true;
  }
  display.setRotation(0);
  display.setTextWrap(false);
  switch (page) {
    case 0: drawStatus(); break;
    case 1: drawBattery(); break;
    case 2: drawPattern(); break;
    case 3: drawClock(); break;
    case 4: drawWeather(); break;
    case 5: drawSetup(); break;
    case NOTE_PAGE: drawNote(); break;
    case IMAGE_PAGE: drawImage(); break;
    default: display.fillScreen(page == PAGE_COUNT + 1 ? GxEPD_BLACK : GxEPD_WHITE); break;
  }
  toPanel(display.getBuffer(), nextFrame);
  const Rect changed = panel.hasFrame() ? difference(panel.frame(), nextFrame) : Rect{};
  const Mode mode = choose(panel.hasFrame(), renderedPage, page, forceFullPending,
                           partialEnabled && !partialFault, partialStreak,
                           uint32_t(millis() - lastFullAt), changed);
  if (mode == Mode::Skip) {
    pending = false; forceFullPending = false; ++skippedCount;
    Serial.printf("SKIP page=%d unchanged total=%lu\n", page + 1, (unsigned long)skippedCount);
    return;
  }
  if (!ready(millis(), finishedAt, mode, first)) return;
  pending = false; forceFullPending = false;
  const char* modeName = mode == Mode::Partial ? "partial" : "full";
  Serial.printf("REFRESH #%lu begin page=%d @%lu mode=%s rect=%d/%d/%d/%d\n",
                (unsigned long)(refreshCount + 1), page + 1, (unsigned long)millis(),
                modeName, changed.x, changed.y, changed.w, changed.h);
  uint32_t start = millis();
  if (mode == Mode::Partial) {
    panel.writeImagePart(nextFrame, changed.x, changed.y, 128, 296,
                         changed.x, changed.y, changed.w, changed.h);
    panel.refresh(changed.x, changed.y, changed.w, changed.h);
    panel.writeImagePartAgain(nextFrame, changed.x, changed.y, 128, 296,
                              changed.x, changed.y, changed.w, changed.h);
  } else {
    // Same full-transfer sequence previously used by GxEPD2_BW::display(false).
    panel.writeImageForFullRefresh(nextFrame, 0, 0, 128, 296);
    panel.refresh(false);
    panel.writeImageAgain(nextFrame, 0, 0, 128, 296);
  }
  panel.powerOff(); // Exactly once for either mode; retain controller RAM/standby.
  lastDuration = millis() - start;
  if (lastDuration > maxDuration) maxDuration = lastDuration;
  ++refreshCount; finishedAt = millis(); lastRefreshMode = modeName;
  const bool ok = healthy(mode, lastDuration, digitalRead(BUSY_PIN) == HIGH);
  if (ok) {
    panel.commitFrame(nextFrame); renderedPage = page;
    if (mode == Mode::Partial) { ++partialCount; ++partialStreak; }
    else { ++fullCount; partialStreak = 0; lastFullAt = finishedAt; }
  } else {
    panel.invalidateFrame(); panelInitialized = false; renderedPage = -1;
    if (mode == Mode::Partial) {
      partialFault = true; requestFullRefresh(); // One automatic full recovery, after cooldown.
      Serial.println("DISPLAY partial failed; disabled for this boot, full recovery queued.");
    } else Serial.println("DISPLAY full refresh failed; press K2 or send f to retry.");
  }
  Serial.printf("REFRESH #%lu returned page=%d elapsed=%lums BUSY=%d queued=%u mode=%s ok=%d; standby, no deep sleep\n",
                (unsigned long)refreshCount, page + 1, (unsigned long)lastDuration,
                digitalRead(BUSY_PIN), (unsigned)uxQueueMessagesWaiting(buttonQueue), modeName, ok);
}

void help() {
  Serial.printf("EE04_Demo firmware %s\n", DEMO_VERSION);
  Serial.println("EE04: 1=Chinese home 2=battery 3/t=pattern 4=clock 5=weather 6=setup 7=note 8=image");
  Serial.println("K2 on note / serial ] = next note sheet; i=editor connection info (temporary access key).");
  Serial.println("n/p=next/previous, c=enable setup hotspot; clock updates each minute.");
  Serial.println("r=force full, u=check changes, a=toggle partial, w=white, b=black, v=sample voltage (no refresh), ?=help");
  Serial.println("K1=previous, K2=full refresh (special on note/setup/weather), K3=next; home/clock auto partial.");
  Serial.println("SHORT=release; LONG=hold 1.2s: K1 clock, K2 setup, K3 weather.");
  Serial.println("Buttons are scanned independently and queued during refresh/HTTPS.");
  Serial.println("f=force panel reinit; K2 on weather requests fresh data (60s minimum).");
  Serial.println("Full/partial both power off high voltage; 3s/750ms cooldown. 10 partials then full. DISPLAY stages use ms; library _Power/_Update times use us.");
  Serial.println("USB logging never waits for a host; unread logs may be dropped. l=toggle verbose display trace.");
  Serial.println("k=raw button diagnostics; d=toggle key-only mode (pauses network/display).");
  Serial.println("STATE: up/end/ms/max in milliseconds, K1/K2/K3 levels (0=pressed), keys=handled counts.");
  Serial.println("KEY logs show event and delivery timestamps; RAW logs show electrical edges in key-only mode.");
}

void movePage(int direction) {
  page = ((page < PAGE_COUNT ? page : 0) + direction + PAGE_COUNT) % PAGE_COUNT;
  pending = true;
}

void applyNetworkChanges() {
  if (network.noteRequested) {
    network.noteRequested = false; page = NOTE_PAGE; noteSheet = 0;
    lastUserInput = millis(); pending = true;
  }
  if (network.imageRequested) {
    network.imageRequested = false; page = IMAGE_PAGE;
    lastUserInput = millis(); pending = true;
  }
  if (network.dirty) {
    network.dirty = false;
    if (page == 0 || (page >= 3 && page <= 5)) pending = true;
  }
  time_t minute = time(nullptr) / 60;
  if (network.clockReady() && minute != lastMinute) {
    lastMinute = minute;
    if (page == 0 || page == 3) pending = true;
  }
  String status = network.statusText();
  if (status != lastNetworkStatus) {
    lastNetworkStatus = status;
    Serial.println("Network: " + status);
    if (page == 0 || page == 4 || page == 5) pending = true;
  }
}

void setup() {
  Serial.begin(115200);
  // HWCDC defaults to 100ms per enqueue attempt, with up to 20 attempts per
  // write in core 3.3.11. A closed terminal must never stall the UI for logs.
#if ARDUINO_USB_CDC_ON_BOOT
  Serial.setTxTimeoutMs(0);
#endif
  uint32_t start = millis();
  while (!Serial && millis() - start < 2500) delay(10);
  const int interfacePins[] = {CS_PIN, DC_PIN, RST_PIN, SCK_PIN, MOSI_PIN};
  for (int pin : interfacePins) { pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }
  pinMode(BUSY_PIN, INPUT);
  pinMode(POWER_PIN, OUTPUT);
  digitalWrite(POWER_PIN, HIGH);
  pinMode(ADC_EN_PIN, OUTPUT);
  digitalWrite(ADC_EN_PIN, LOW);
  pinMode(BAT_PIN, INPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(BAT_PIN, ADC_11db);
  for (int pin : KEY_PINS) pinMode(pin, INPUT_PULLUP);
  buttonQueue = xQueueCreate(32, sizeof(ButtonEvent));
  if (!buttonQueue || xTaskCreatePinnedToCore(buttonScanner, "ee04-buttons", 2048, nullptr, 2, nullptr, 1) != pdPASS) {
    Serial.println("FATAL: button scanner could not start. Reset the board.");
    for (;;) delay(1000);
  }
  printButtonDiagnostics();
  delay(100);
  digitalWrite(CS_PIN, HIGH);
  SPI.begin(SCK_PIN, -1, MOSI_PIN, CS_PIN);
  if (!display.getBuffer()) {
    Serial.println("FATAL: no canvas memory. Reset the board.");
    for (;;) delay(1000);
  }
  cn.begin(display);
  network.frameHandler = []() {
    if (!panel.hasFrame()) { network.server.send(503, "text/plain", "Screen not ready"); return; }
    network.server.setContentLength(TracedPanel290::FRAME_BYTES);
    network.server.send(200, "application/octet-stream", "");
    network.server.sendContent(reinterpret_cast<const char*>(panel.frame()), TracedPanel290::FRAME_BYTES);
  };
  network.begin();
  savedPage = network.restoredPage();
  page = network.apActive ? 5 : savedPage;
  lastNetworkStatus = network.statusText();
  network.dirty = false;
  lastMinute = time(nullptr) / 60;
  help();
  render();
}

void loop() {

  if (keyDiagnostics) {
    int rawMask = 0;
    for (int i = 0; i < 3; ++i) rawMask |= digitalRead(KEY_PINS[i]) << i;
    if (rawMask != lastRawMask) {
      lastRawMask = rawMask;
      Serial.print("RAW "); printButtonDiagnostics();
    }
  }
  ButtonEvent event;
  while (xQueueReceive(buttonQueue, &event, 0) == pdTRUE) {
    const int i = event.index;
    ++presses[i];
    lastUserInput = millis();
    if (!keyDiagnostics) {
      if (event.kind == 2) {
        page = i == 0 ? 3 : (i == 1 ? 5 : 4);
        if (i == 1) network.startSetup();
        pending = true;
      } else if (i == 0) movePage(-1);
      else if (i == 2) movePage(1);
      else {
        if (page == NOTE_PAGE) nextNoteSheet();
        else if (page == 5) network.startSetup();
        else if (page == 4) Serial.println(network.requestWeather() ? "Weather refresh queued" : "Weather refresh requires WiFi/NTP/city and 60s between attempts");
        else requestFullRefresh();
      }
    }
    Serial.printf("KEY K%d GPIO%d %s event@%lu handled@%lu -> target page=%d, total=%lu\n",
                  i + 1, KEY_PINS[i], event.kind == 2 ? "LONG" : "SHORT", (unsigned long)event.pressedAt,
                  (unsigned long)millis(), page + 1, (unsigned long)presses[i]);
  }
  while (Serial.available()) {
    char cmd = Serial.read();
    if (cmd != '\r' && cmd != '\n') lastUserInput = millis();
    if (cmd >= '1' && cmd <= '8') { page = cmd - '1'; pending = true; }
    else if (cmd == 'n') movePage(1);
    else if (cmd == 'p') movePage(-1);
    else if (cmd == 'r') requestFullRefresh();
    else if (cmd == 'u') pending = true;
    else if (cmd == 'a') {
      partialEnabled = !partialEnabled; requestFullRefresh();
      Serial.printf("AUTO partial=%d fault=%d; fault clears on reboot\n", partialEnabled, partialFault);
    }
    else if (cmd == 'f') { panelInitialized = false; panel.invalidateFrame(); requestFullRefresh(); }
    else if (cmd == 't') { page = 2; pending = true; }
    else if (cmd == 'w' || cmd == 'b') { page = cmd == 'w' ? PAGE_COUNT : PAGE_COUNT + 1; pending = true; }
    else if (cmd == 'c') { network.startSetup(); page = 5; pending = true; }
    else if (cmd == 'i') network.printEditorConnection();
    else if (cmd == ']') { page = NOTE_PAGE; nextNoteSheet(); }
    else if (cmd == 'v') {
      sampleBattery();
      Serial.printf("ADC=%lumV, battery=%.3fV, estimate=%d%%\n", (unsigned long)adcMv, batteryV, batteryPct);
    } else if (cmd == 'd') {
      keyDiagnostics = !keyDiagnostics;
      lastRawMask = -1;
      Serial.printf("Key-only diagnostics %s. Network/display %s; press K1/K2/K3.\n",
                    keyDiagnostics ? "ON" : "OFF", keyDiagnostics ? "paused" : "resumed");
      if (!keyDiagnostics) pending = true;
    } else if (cmd == 'k') printButtonDiagnostics();
    else if (cmd == 'l') {
      panel.setTracing(!panel.tracing());
      Serial.printf("Display trace %s; USB logging remains nonblocking.\n",
                    panel.tracing() ? "ON" : "OFF");
    }
    else if (cmd == '?') help();
  }
  if (!keyDiagnostics) {
    applyNetworkChanges();
    // Show a pending user selection before starting potentially blocking HTTPS.
    if (pending && uint32_t(millis() - lastRenderAttempt) >= 100) render();
    const uint32_t networkStarted = millis();
    network.tick();
    const uint32_t networkElapsed = millis() - networkStarted;
    if (networkElapsed >= 1000)
      Serial.printf("NETWORK tick elapsed=%lums queued=%u\n", (unsigned long)networkElapsed,
                    (unsigned)uxQueueMessagesWaiting(buttonQueue));
    applyNetworkChanges();
    // Consume any input queued during the refresh/network operation next loop
    // before rendering again, so stale pages cannot get ahead of button input.
    if (!pending && page < PAGE_COUNT && page != 5 && page != savedPage && uint32_t(millis() - lastUserInput) >= 5000) {
      network.rememberPage(page); savedPage = page;
    }
  }
  delay(5);
}
