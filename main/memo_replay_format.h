#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MEMO_REPLAY_PARTITION_BYTES 0x40000u
#define MEMO_REPLAY_DATA_OFFSET 0x1000u
#define MEMO_REPLAY_HEADER_BYTES 64u
#define MEMO_REPLAY_PACKET_BYTES 40u
#define MEMO_REPLAY_FRAME_SAMPLES 320u
#define MEMO_REPLAY_PRESKIP 104u
#define MEMO_REPLAY_MAX_FRAMES 6001u
#define MEMO_REPLAY_MAX_SAMPLES (120u * 16000u)

typedef struct {
  uint32_t frames, samples, data_crc, record_id;
  uint8_t text_hash[32];
} memo_replay_info_t;

/* Incremental IEEE CRC32: start with UINT32_MAX, XOR UINT32_MAX at the end. */
uint32_t memo_replay_crc(uint32_t state, const void *data, size_t bytes);
bool memo_replay_info_valid(const memo_replay_info_t *info);
bool memo_replay_header_write(uint8_t out[MEMO_REPLAY_HEADER_BYTES],
                             const memo_replay_info_t *info);
bool memo_replay_header_read(const uint8_t *data, size_t bytes,
                            memo_replay_info_t *info);
/* Exact PCM slice after the Opus pre-skip and final padding are removed. */
size_t memo_replay_slice(const memo_replay_info_t *info, uint32_t frame,
                         size_t *skip);
