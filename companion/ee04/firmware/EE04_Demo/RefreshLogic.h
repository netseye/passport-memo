#pragma once
#include <cstdint>
#include <cstring>

namespace EE04Refresh {
constexpr int WIDTH = 128, HEIGHT = 296, BYTES = WIDTH * HEIGHT / 8;
constexpr uint32_t FULL_GAP_MS = 3000, PARTIAL_GAP_MS = 750;
constexpr uint32_t MAX_PARTIAL_AGE_MS = 15 * 60 * 1000;
constexpr uint8_t MAX_PARTIALS = 10;
enum class Mode { Skip, Full, Partial };
struct Rect { int x = 0, y = 0, w = 0, h = 0; };

// Canvas: 296x128, row-major, 1=white. Panel: 128x296 at rotation 1.
inline void toPanel(const uint8_t* canvas, uint8_t* panel) {
  memset(panel, 0, BYTES);
  for (int y = 0; y < 128; ++y) for (int x = 0; x < 296; ++x) {
    if (canvas[y * 37 + x / 8] & (0x80 >> (x % 8)))
      panel[x * 16 + (127 - y) / 8] |= 0x80 >> ((127 - y) % 8);
  }
}

// The controller needs native X/width aligned to bytes. Include whole changed
// bytes and use the full new framebuffer, so aligned border pixels stay intact.
inline Rect difference(const uint8_t* before, const uint8_t* after) {
  int left = 16, right = -1, top = HEIGHT, bottom = -1;
  for (int y = 0; y < HEIGHT; ++y) for (int b = 0; b < 16; ++b) {
    if (before[y * 16 + b] == after[y * 16 + b]) continue;
    if (b < left) left = b;
    if (b > right) right = b;
    if (y < top) top = y;
    if (y > bottom) bottom = y;
  }
  if (right < 0) return {};
  return {left * 8, top, (right - left + 1) * 8, bottom - top + 1};
}

inline bool partialPage(int page) { return page == 0 || page == 3; }
inline Mode choose(bool validFrame, int previousPage, int page, bool forceFull,
                   bool partialEnabled, uint8_t partialCount, uint32_t sinceFull, Rect changed) {
  if (!validFrame || page != previousPage || forceFull) return Mode::Full;
  if (!changed.w || !changed.h) return Mode::Skip;
  if (!partialEnabled || !partialPage(page) || partialCount >= MAX_PARTIALS ||
      sinceFull >= MAX_PARTIAL_AGE_MS || changed.w * changed.h > WIDTH * HEIGHT / 2)
    return Mode::Full;
  return Mode::Partial;
}
inline bool healthy(Mode mode, uint32_t elapsed, bool busy) {
  return !busy && elapsed < (mode == Mode::Partial ? 5000u : 10000u);
}
inline bool ready(uint32_t now, uint32_t finished, Mode mode, bool first) {
  return first || mode == Mode::Skip || uint32_t(now - finished) >=
      (mode == Mode::Partial ? PARTIAL_GAP_MS : FULL_GAP_MS);
}
} // namespace EE04Refresh
