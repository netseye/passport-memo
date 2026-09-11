#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace EE04Image {
constexpr size_t WIDTH = 296, HEIGHT = 128, BYTES = WIDTH * HEIGHT / 8;
// EI01 fixes dimensions and packing: landscape rows, MSB first, 1 = black.
// Explicit byte encoding avoids ABI/endianness dependencies in persisted data.
constexpr size_t HEADER = 12;
struct Record {
  std::array<uint8_t, HEADER + BYTES> bytes{};
  uint8_t* pixels() { return bytes.data() + HEADER; }
  const uint8_t* pixels() const { return bytes.data() + HEADER; }
};
inline uint32_t read32(const uint8_t* p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
inline void write32(uint8_t* p, uint32_t value) {
  for (int i = 0; i < 4; ++i) p[i] = uint8_t(value >> (i * 8));
}
inline uint32_t crc32(const uint8_t* data, size_t length) {
  uint32_t crc = 0xffffffff;
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (int b = 0; b < 8; ++b) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
  }
  return ~crc;
}
inline uint32_t revision(const Record& record) { return read32(record.bytes.data() + 4); }
inline void seal(Record& record, uint32_t rev) {
  auto* b = record.bytes.data();
  memcpy(b, "EI01", 4); write32(b + 4, rev ? rev : 1);
  write32(b + 8, crc32(record.pixels(), BYTES));
}
inline bool valid(const Record& record) {
  return memcmp(record.bytes.data(), "EI01", 4) == 0 && revision(record) != 0 &&
      read32(record.bytes.data() + 8) == crc32(record.pixels(), BYTES);
}
inline int nibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}
inline bool decodeHex(const char* hex, size_t length, Record& out) {
  if (length != BYTES * 2) return false;
  for (size_t i = 0; i < BYTES; ++i) {
    int hi = nibble(hex[i * 2]), lo = nibble(hex[i * 2 + 1]);
    if (hi < 0 || lo < 0) return false;
    out.pixels()[i] = uint8_t((hi << 4) | lo);
  }
  return true;
}
} // namespace EE04Image
