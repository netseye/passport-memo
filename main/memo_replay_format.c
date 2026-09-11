#include "memo_replay_format.h"
#include <string.h>

static uint32_t read32(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
         (uint32_t)p[3] << 24;
}
static void write32(uint8_t *p, uint32_t value) {
  for (unsigned i = 0; i < 4; i++)
    p[i] = value >> (i * 8);
}
uint32_t memo_replay_crc(uint32_t state, const void *data, size_t bytes) {
  const uint8_t *p = data;
  for (size_t i = 0; i < bytes; i++) {
    state ^= p[i];
    for (unsigned b = 0; b < 8; b++)
      state = (state >> 1) ^ ((state & 1) ? 0xedb88320u : 0);
  }
  return state;
}
bool memo_replay_info_valid(const memo_replay_info_t *info) {
  return info && info->frames && info->frames <= MEMO_REPLAY_MAX_FRAMES &&
         info->samples && info->samples <= MEMO_REPLAY_MAX_SAMPLES &&
         info->samples <= info->frames * MEMO_REPLAY_FRAME_SAMPLES - MEMO_REPLAY_PRESKIP &&
         info->frames * MEMO_REPLAY_PACKET_BYTES <=
             MEMO_REPLAY_PARTITION_BYTES - MEMO_REPLAY_DATA_OFFSET;
}
bool memo_replay_header_write(uint8_t out[MEMO_REPLAY_HEADER_BYTES],
                             const memo_replay_info_t *info) {
  if (!out || !memo_replay_info_valid(info))
    return false;
  memset(out, 0, MEMO_REPLAY_HEADER_BYTES);
  memcpy(out, "PMOPUS1", 7);
  write32(out + 8, info->frames);
  write32(out + 12, info->samples);
  write32(out + 16, info->data_crc);
  write32(out + 20, info->record_id);
  memcpy(out + 24, info->text_hash, 32);
  write32(out + 56, memo_replay_crc(UINT32_MAX, out, 56) ^ UINT32_MAX);
  return true;
}
bool memo_replay_header_read(const uint8_t *data, size_t bytes,
                            memo_replay_info_t *info) {
  if (!data || !info || bytes != MEMO_REPLAY_HEADER_BYTES ||
      memcmp(data, "PMOPUS1\0", 8) || read32(data + 60) ||
      read32(data + 56) != (memo_replay_crc(UINT32_MAX, data, 56) ^ UINT32_MAX))
    return false;
  memo_replay_info_t decoded = {.frames = read32(data + 8),
                                .samples = read32(data + 12),
                                .data_crc = read32(data + 16),
                                .record_id = read32(data + 20)};
  memcpy(decoded.text_hash, data + 24, 32);
  if (!memo_replay_info_valid(&decoded))
    return false;
  *info = decoded;
  return true;
}
size_t memo_replay_slice(const memo_replay_info_t *info, uint32_t frame,
                         size_t *skip) {
  if (!skip)
    return 0;
  *skip = 0;
  if (!memo_replay_info_valid(info) || frame >= info->frames)
    return 0;
  uint32_t start = frame * MEMO_REPLAY_FRAME_SAMPLES;
  uint32_t end = start + MEMO_REPLAY_FRAME_SAMPLES;
  uint32_t wanted_end = MEMO_REPLAY_PRESKIP + info->samples;
  if (start < MEMO_REPLAY_PRESKIP) {
    *skip = MEMO_REPLAY_PRESKIP - start;
    start = MEMO_REPLAY_PRESKIP;
  }
  if (end > wanted_end)
    end = wanted_end;
  return end > start ? end - start : 0;
}
