#pragma once
#include <epd/GxEPD2_290.h>

// Keep GxEPD2 commands, timings and waveforms unchanged. The preview is
// committed only after a completed full or partial transfer and power-off.
class TracedPanel290 : public GxEPD2_290 {
 public:
  using GxEPD2_290::GxEPD2_290;
  using GxEPD2_290::refresh;

  void setTracing(bool enabled) { traceEnabled = enabled; _diag_enabled = enabled; }
  bool tracing() const { return traceEnabled; }
  static constexpr size_t FRAME_BYTES = WIDTH * HEIGHT / 8;
  const uint8_t* frame() const { return lastFrame; }
  bool hasFrame() const { return frameValid; }
  void invalidateFrame() { frameValid = false; }
  void commitFrame(const uint8_t* bitmap) {
    memcpy(lastFrame, bitmap, FRAME_BYTES); frameValid = true;
  }

  void writeImageForFullRefresh(const uint8_t bitmap[], int16_t x, int16_t y,
                               int16_t w, int16_t h, bool invert = false,
                               bool mirror_y = false, bool pgm = false) override {
    trace("RAM", [&]() {
      GxEPD2_290::writeImageForFullRefresh(bitmap, x, y, w, h, invert, mirror_y, pgm);
    });
  }

  void refresh(bool partial_update_mode = false) override {
    trace("UPDATE", [&]() { GxEPD2_290::refresh(partial_update_mode); });
  }

  void refresh(int16_t x, int16_t y, int16_t w, int16_t h) override {
    trace("PARTIAL", [&]() { GxEPD2_290::refresh(x, y, w, h); });
  }

  void writeImageAgain(const uint8_t bitmap[], int16_t x, int16_t y,
                      int16_t w, int16_t h, bool invert = false,
                      bool mirror_y = false, bool pgm = false) override {
    trace("SYNC", [&]() {
      GxEPD2_290::writeImageAgain(bitmap, x, y, w, h, invert, mirror_y, pgm);
    });
  }

  void powerOff() override {
    trace("OFF", [&]() { GxEPD2_290::powerOff(); });
  }

 private:
  uint8_t lastFrame[FRAME_BYTES] = {};
  bool frameValid = false;
  bool traceEnabled = false;
  template <typename Operation> void trace(const char* stage, Operation operation) {
    if (!traceEnabled) { operation(); return; }
    Serial.printf("DISPLAY %s begin @%lu BUSY=%d\n", stage,
                  (unsigned long)millis(), digitalRead(_busy));
    const uint32_t started = millis();
    operation();
    const uint32_t elapsed = millis() - started;
    Serial.printf("DISPLAY %s end elapsed=%lums BUSY=%d%s\n", stage,
                  (unsigned long)elapsed, digitalRead(_busy),
                  elapsed >= 10000 ? " SLOW: check Busy Timeout lines above" : "");
  }
};
