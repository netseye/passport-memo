#include "memo_ogg.h"
#include <string.h>
static void le32(uint8_t *p, uint32_t n) {
  for (int i = 0; i < 4; i++)
    p[i] = n >> (i * 8);
}
static uint32_t crc(const uint8_t *p, size_t n) {
  uint32_t c = 0;
  for (size_t i = 0; i < n; i++) {
    c ^= (uint32_t)p[i] << 24;
    for (int b = 0; b < 8; b++)
      c = (c << 1) ^ ((c & 0x80000000) ? 0x04c11db7 : 0);
  }
  return c;
}
size_t memo_ogg_page(uint8_t *out, size_t cap, const uint8_t *packet, size_t n,
                     uint32_t serial, uint32_t seq, uint64_t granule, uint8_t flags) {
  size_t segments = n / 255 + 1, head = 27 + segments;
  if (!out || !packet || !n || segments > 255 || head > cap || n > cap - head)
    return 0;
  memset(out, 0, head);
  memcpy(out, "OggS", 4);
  out[5] = flags;
  for (int i = 0; i < 8; i++)
    out[6 + i] = granule >> (8 * i);
  le32(out + 14, serial);
  le32(out + 18, seq);
  out[26] = segments;
  for (size_t i = 0; i < segments; i++)
    out[27 + i] = i + 1 == segments ? n % 255 : 255;
  memcpy(out + head, packet, n);
  le32(out + 22, crc(out, head + n));
  return head + n;
}
size_t memo_ogg_headers(uint8_t *out, size_t cap, uint32_t serial, uint16_t preskip) {
  uint8_t head[19] = {'O', 'p', 'u',  's',  'H', 'e', 'a', 'd', 1, 1,
                      0,   0,   0x80, 0x3e, 0,   0,   0,   0,   0};
  head[10] = preskip;
  head[11] = preskip >> 8;
  const uint8_t tags[24] = {'O', 'p', 'u', 's', 'T', 'a', 'g', 's', 8, 0, 0, 0,
                            'P', 'a', 's', 's', 'p', 'o', 'r', 't', 0, 0, 0, 0};
  size_t a = memo_ogg_page(out, cap, head, sizeof(head), serial, 0, 0, 2);
  if (!a)
    return 0;
  size_t b = memo_ogg_page(out + a, cap - a, tags, sizeof(tags), serial, 1, 0, 0);
  return b ? a + b : 0;
}
