#include "memo_playback_pcm.h"

unsigned memo_playback_prepare(int16_t *pcm, size_t samples) {
  if (!pcm || !samples)
    return 0;
  uint64_t sum = 0;
  for (size_t i = 0; i < samples; i++) {
    int32_t value = pcm[i];
    uint32_t magnitude = (value < 0 ? -value : value) * 2u;
    // Linear below 24000, then smoothly approach 32000 without hard clipping.
    if (magnitude > 24000) {
      uint32_t excess = magnitude - 24000;
      magnitude = 24000 + excess * 8000u / (excess + 8000u);
    }
    pcm[i] = value < 0 ? -(int32_t)magnitude : (int32_t)magnitude;
    sum += magnitude;
  }
  unsigned level = sum / samples / 70;
  return level > 100 ? 100 : level;
}
