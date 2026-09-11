#include "memo_replay_export.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t raw[MEMO_REPLAY_MAX_FRAMES * MEMO_REPLAY_PACKET_BYTES];
static uint8_t output[408167];
static size_t used, reads, writes, fail_read, fail_write;
static bool read_data(void *ctx, size_t offset, void *data, size_t bytes) {
  assert(ctx == raw);
  assert(offset + bytes <= sizeof(raw));
  if (++reads == fail_read) return false;
  memcpy(data, raw + offset, bytes);
  return true;
}
static bool write_data(void *ctx, const void *data, size_t bytes) {
  assert(ctx == raw);
  assert(bytes <= 1360 && used + bytes <= sizeof(output));
  if (++writes == fail_write) return false;
  memcpy(output + used, data, bytes);
  used += bytes;
  return true;
}
static uint32_t le32(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static uint32_t ogg_crc(const uint8_t *p, size_t n) {
  uint32_t c = 0;
  for (size_t i = 0; i < n; i++) {
    c ^= (uint32_t)(i >= 22 && i < 26 ? 0 : p[i]) << 24;
    for (unsigned b = 0; b < 8; b++) c = (c << 1) ^ (c & 0x80000000u ? 0x04c11db7u : 0);
  }
  return c;
}
static memo_export_result_t run(memo_replay_info_t *info) {
  used = reads = writes = 0;
  return memo_replay_export(info, read_data, write_data, raw);
}
int main(void) {
  for (size_t i = 0; i < sizeof(raw); i++) raw[i] = i * 17;
  const unsigned samples[] = {1, 216, 320, 1599, 16000, MEMO_REPLAY_MAX_SAMPLES};
  for (unsigned i = 0; i < sizeof(samples) / sizeof(*samples); i++) {
    memo_replay_info_t info = {.samples = samples[i], .record_id = 42};
    info.frames = (info.samples + MEMO_REPLAY_PRESKIP + 319) / 320;
    info.data_crc = memo_replay_crc(UINT32_MAX, raw, info.frames * 40) ^ UINT32_MAX;
    assert(run(&info) == MEMO_EXPORT_OK);
    assert(used == 99 + 68 * info.frames);
    size_t offset = 0;
    uint32_t seq = 0;
    while (offset < used) {
      const uint8_t *page = output + offset;
      assert(!memcmp(page, "OggS", 4) && page[26] == 1);
      size_t n = 28 + page[27];
      assert(le32(page + 18) == seq && le32(page + 14) == 42);
      assert(le32(page + 22) == ogg_crc(page, n));
      if (seq >= 2) {
        assert(page[27] == 40 && !memcmp(page + 28, raw + (seq - 2) * 40, 40));
        bool last = offset + n == used;
        assert(page[5] == (last ? 4 : 0));
        assert(le32(page + 6) == (last ? (info.samples + 104) * 3 : (seq - 1) * 960));
      }
      offset += n;
      seq++;
    }
    assert(seq == info.frames + 2);
    raw[0] ^= 1;
    assert(run(&info) == MEMO_EXPORT_CRC && writes == 0);
    raw[0] ^= 1;
    fail_read = 1;
    assert(run(&info) == MEMO_EXPORT_READ && writes == 0);
    fail_read = (info.frames * 40 + 1359) / 1360 + 1;
    assert(run(&info) == MEMO_EXPORT_READ && writes == 1);
    fail_read = 0;
    fail_write = 2;
    assert(run(&info) == MEMO_EXPORT_WRITE && writes == 2);
    fail_write = 0;
  }
  memo_replay_info_t extra = {.samples = 320, .frames = 3, .record_id = 1};
  extra.data_crc = memo_replay_crc(UINT32_MAX, raw, 120) ^ UINT32_MAX;
  assert(run(&extra) == MEMO_EXPORT_OK && used == 99 + 2 * 68);
  extra.samples = 0;
  assert(run(&extra) == MEMO_EXPORT_INVALID && writes == 0);
  assert(memo_replay_export(NULL, read_data, write_data, raw) == MEMO_EXPORT_INVALID);
  puts("Replay Ogg export: CRC, exact granules, 120 s, I/O failures and bounded chunks PASS");
}
