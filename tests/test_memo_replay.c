#include "memo_replay_format.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
  const char *vector = "123456789";
  uint32_t crc = memo_replay_crc(UINT32_MAX, vector, 9) ^ UINT32_MAX;
  assert(crc == 0xcbf43926u); // Independent standard CRC32 test vector.
  uint32_t split = memo_replay_crc(UINT32_MAX, vector, 4);
  assert((memo_replay_crc(split, vector + 4, 5) ^ UINT32_MAX) == crc);

  // Exact output duration: remove encoder delay and only the final padding.
  const unsigned durations[] = {20, 100, 1000, 21920, 120000};
  for (unsigned n = 0; n < sizeof(durations) / sizeof(durations[0]); n++) {
    memo_replay_info_t info = {.frames = durations[n] / 20 + 1,
                               .samples = durations[n] * 16,
                               .record_id = 7, .data_crc = crc};
    memset(info.text_hash, 0x42, sizeof(info.text_hash));
    uint8_t header[MEMO_REPLAY_HEADER_BYTES];
    memo_replay_info_t restored = {0};
    assert(memo_replay_header_write(header, &info));
    assert(memo_replay_header_read(header, sizeof(header), &restored));
    assert(restored.record_id == 7 && restored.frames == info.frames);
    assert(restored.samples == info.samples && restored.data_crc == crc);
    assert(!memcmp(restored.text_hash, info.text_hash, sizeof(info.text_hash)));
    size_t total = 0, skip;
    for (uint32_t i = 0; i < restored.frames; i++) {
      size_t samples = memo_replay_slice(&restored, i, &skip);
      assert(skip == (i ? 0 : 104));
      assert(skip + samples <= 320);
      total += samples;
    }
    assert(total == durations[n] * 16);
    assert(memo_replay_slice(&restored, restored.frames, &skip) == 0);
    // Every single-byte corruption and truncated header must be rejected.
    for (size_t i = 0; i < sizeof(header); i++) {
      header[i] ^= 1;
      assert(!memo_replay_header_read(header, sizeof(header), &restored));
      header[i] ^= 1;
      assert(!memo_replay_header_read(header, i, &restored));
    }
  }
  memo_replay_info_t interrupted = {.frames = 1, .samples = 216};
  size_t skip;
  assert(memo_replay_slice(&interrupted, 0, &skip) == 216 && skip == 104);
  interrupted.samples = 320; // No padding frame: unavailable tail is rejected.
  assert(!memo_replay_info_valid(&interrupted));
  interrupted.frames = UINT32_MAX;
  assert(!memo_replay_info_valid(&interrupted));
  interrupted.frames = 6001;
  interrupted.samples = MEMO_REPLAY_MAX_SAMPLES + 1;
  assert(!memo_replay_info_valid(&interrupted));
  memset(&interrupted, 0, sizeof(interrupted));
  assert(!memo_replay_info_valid(&interrupted));
  assert(!memo_replay_info_valid(NULL));
  puts("Replay integrity and exact-duration tests: PASS");
  return 0;
}
