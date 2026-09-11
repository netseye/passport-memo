#include "memo_replay_export.h"
#include "memo_ogg.h"

memo_export_result_t memo_replay_export(const memo_replay_info_t *info,
                                       memo_replay_read_t read,
                                       memo_replay_write_t write, void *ctx) {
  if (!memo_replay_info_valid(info) || !read || !write)
    return MEMO_EXPORT_INVALID;
  uint8_t chunk[1360], packet[MEMO_REPLAY_PACKET_BYTES];
  size_t bytes = info->frames * MEMO_REPLAY_PACKET_BYTES;
  uint32_t crc = UINT32_MAX;
  for (size_t offset = 0; offset < bytes;) {
    size_t n = bytes - offset;
    if (n > sizeof(chunk)) n = sizeof(chunk);
    if (!read(ctx, offset, chunk, n)) return MEMO_EXPORT_READ;
    crc = memo_replay_crc(crc, chunk, n);
    offset += n;
  }
  if ((crc ^ UINT32_MAX) != info->data_crc) return MEMO_EXPORT_CRC;
  uint32_t serial = info->record_id ? info->record_id : 1;
  size_t used = memo_ogg_headers(chunk, sizeof(chunk), serial,
                                  MEMO_REPLAY_PRESKIP * 3);
  if (!used) return MEMO_EXPORT_INVALID;
  if (!write(ctx, chunk, used)) return MEMO_EXPORT_WRITE;
  used = 0;
  /* Omit any whole trailing padding frames, including interrupted captures. */
  uint32_t frames = (info->samples + MEMO_REPLAY_PRESKIP +
                     MEMO_REPLAY_FRAME_SAMPLES - 1) / MEMO_REPLAY_FRAME_SAMPLES;
  for (uint32_t i = 0; i < frames; i++) {
    if (!read(ctx, i * sizeof(packet), packet, sizeof(packet)))
      return MEMO_EXPORT_READ;
    bool last = i + 1 == frames;
    uint64_t granule = last ? (uint64_t)(info->samples + MEMO_REPLAY_PRESKIP) * 3
                            : (uint64_t)(i + 1) * 960;
    size_t n = memo_ogg_page(chunk + used, sizeof(chunk) - used, packet,
                             sizeof(packet), serial, i + 2, granule, last ? 4 : 0);
    if (!n) return MEMO_EXPORT_INVALID;
    used += n;
    if (used == sizeof(chunk) || last) {
      if (!write(ctx, chunk, used)) return MEMO_EXPORT_WRITE;
      used = 0;
    }
  }
  return MEMO_EXPORT_OK;
}
