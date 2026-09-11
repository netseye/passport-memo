#pragma once
#include "memo_replay_format.h"

typedef bool (*memo_replay_read_t)(void *ctx, size_t offset, void *data, size_t bytes);
typedef bool (*memo_replay_write_t)(void *ctx, const void *data, size_t bytes);
typedef enum {
  MEMO_EXPORT_OK, MEMO_EXPORT_INVALID, MEMO_EXPORT_READ,
  MEMO_EXPORT_CRC, MEMO_EXPORT_WRITE
} memo_export_result_t;

/* Reader offsets are relative to the raw Opus payload. The caller excludes
 * cache mutation for the entire call. Verify CRC before emitting any bytes;
 * remux without decoding, allocating a whole clip, or changing the payload. */
memo_export_result_t memo_replay_export(const memo_replay_info_t *info,
                                       memo_replay_read_t read,
                                       memo_replay_write_t write, void *ctx);
