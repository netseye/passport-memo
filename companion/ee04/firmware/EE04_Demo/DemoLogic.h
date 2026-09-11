#pragma once
#include <stdint.h>

struct DebouncedButton {
  bool raw = false;
  bool stable = false;
  uint32_t changedAt = 0;
  void sync(bool pressed, uint32_t now) {
    raw = stable = pressed;
    changedAt = now;
  }
  bool update(bool pressed, uint32_t now) {
    if (pressed != raw) { raw = pressed; changedAt = now; }
    if (raw != stable && uint32_t(now - changedAt) >= 35) {
      stable = raw;
      return stable; // One event per debounced press, no auto-repeat.
    }
    return false;
  }
};

// Rough resting-voltage estimate for a single 4.2V Li-ion cell.
// -1 means outside the plausible range, NOT proof that no battery is fitted.
inline int batteryPercent(float voltage) {
  if (!(voltage >= 3.0f && voltage <= 4.35f)) return -1;
  const float volts[] = {3.30f, 3.50f, 3.65f, 3.70f, 3.75f, 3.80f, 3.85f, 3.90f, 4.00f, 4.10f, 4.20f};
  const int pct[] = {0, 5, 10, 20, 30, 40, 50, 60, 75, 90, 100};
  if (voltage <= volts[0]) return 0;
  for (int i = 1; i < 11; ++i)
    if (voltage <= volts[i])
      return pct[i - 1] + int((voltage - volts[i - 1]) * (pct[i] - pct[i - 1]) /
                             (volts[i] - volts[i - 1]) + 0.5f);
  return 100;
}

// SHORT fires on release; LONG fires once at 1.2s and suppresses SHORT.
// A key held at boot must first be released before it can trigger an action.
struct ButtonGesture {
  DebouncedButton debounce;
  bool armed = false, longSent = false;
  uint32_t downAt = 0;
  void sync(bool pressed, uint32_t now) {
    debounce.sync(pressed, now); armed = false; longSent = false;
  }
  uint8_t update(bool pressed, uint32_t now) {
    bool wasDown = debounce.stable;
    debounce.update(pressed, now);
    if (!wasDown && debounce.stable) { armed = true; longSent = false; downAt = now; }
    if (wasDown && !debounce.stable) {
      bool shortPress = armed && !longSent;
      armed = false;
      return shortPress ? 1 : 0;
    }
    if (armed && debounce.stable && debounce.raw && !longSent && uint32_t(now - downAt) >= 1200) {
      longSent = true;
      return 2;
    }
    return 0;
  }
};
