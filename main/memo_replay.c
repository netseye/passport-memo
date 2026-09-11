#include "memo_replay.h"
#include "memo_replay_format.h"
#include "memo_playback_pcm.h"
#include "bsp_audio.h"
#include "esp_log.h"
#include "esp_opus_dec.h"
#include "esp_partition.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "mbedtls/sha256.h"
#include <stdatomic.h>
#include <string.h>

static const esp_partition_t *partition;
static memo_replay_info_t info;
static bool ready, writing;
/* Reused for recording writes and pre-playback verification; never concurrently. */
static uint8_t buffer[4096];
static size_t buffered, written;
static uint32_t checksum;
static int64_t max_write_us;
static atomic_bool stopped;
static atomic_int volume = 85;

static bool hash_text(const char *text, uint8_t hash[32]) {
  return text && mbedtls_sha256((const uint8_t *)text, strlen(text), hash, 0) == 0;
}
static bool commit_header(void) {
  uint8_t header[MEMO_REPLAY_HEADER_BYTES];
  ready = false;
  if (!partition || !memo_replay_header_write(header, &info) ||
      esp_partition_write(partition, 0, header, sizeof(header)) != ESP_OK)
    return false;
  ready = true;
  return true;
}
void memo_replay_init(void) {
  partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, 0x40, "replay");
  if (!partition || partition->address != 0x390000 ||
      partition->size != MEMO_REPLAY_PARTITION_BYTES) {
    partition = NULL;
    ESP_LOGW("replay", "Replay partition unavailable; flash the updated partition table");
    return;
  }
  uint8_t header[MEMO_REPLAY_HEADER_BYTES];
  ready = esp_partition_read(partition, 0, header, sizeof(header)) == ESP_OK &&
          memo_replay_header_read(header, sizeof(header), &info);
}
bool memo_replay_begin(void) {
  ready = writing = false;
  buffered = written = 0;
  checksum = UINT32_MAX;
  max_write_us = 0;
  memset(&info, 0, sizeof(info));
  /* Erase before encoder/TLS allocation and microphone capture. */
  if (!partition || esp_partition_erase_range(partition, 0, partition->size) != ESP_OK) {
    ESP_LOGW("replay", "Replay cache unavailable; ASR will continue without playback");
    return false;
  }
  writing = true;
  return true;
}
static bool flush(void) {
  if (!writing || !buffered)
    return writing;
  int64_t start = esp_timer_get_time();
  esp_err_t error = esp_partition_write(partition, MEMO_REPLAY_DATA_OFFSET + written,
                                        buffer, buffered);
  int64_t duration = esp_timer_get_time() - start;
  if (duration > max_write_us)
    max_write_us = duration;
  if (error != ESP_OK) {
    writing = false;
    ESP_LOGW("replay", "Replay write failed; ASR remains available");
    return false;
  }
  written += buffered;
  buffered = 0;
  return true;
}
void memo_replay_append(const void *opus, size_t bytes, unsigned input_samples) {
  if (!writing)
    return;
  if (!opus || bytes != MEMO_REPLAY_PACKET_BYTES ||
      (input_samples != 0 && input_samples != MEMO_REPLAY_FRAME_SAMPLES) ||
      info.frames >= MEMO_REPLAY_MAX_FRAMES ||
      info.samples + input_samples > MEMO_REPLAY_MAX_SAMPLES) {
    writing = false;
    ESP_LOGW("replay", "Unsupported replay frame; disabling this cache");
    return;
  }
  checksum = memo_replay_crc(checksum, opus, bytes);
  const uint8_t *p = opus;
  while (bytes) {
    size_t n = sizeof(buffer) - buffered;
    if (n > bytes)
      n = bytes;
    memcpy(buffer + buffered, p, n);
    buffered += n;
    p += n;
    bytes -= n;
    if (buffered == sizeof(buffer) && !flush())
      return;
  }
  info.frames++;
  info.samples += input_samples;
}
bool memo_replay_finish(const char *text) {
  if (!writing || !info.frames || !info.samples || !flush()) {
    writing = false;
    return false;
  }
  writing = false;
  uint32_t decodable = info.frames * MEMO_REPLAY_FRAME_SAMPLES - MEMO_REPLAY_PRESKIP;
  if (info.samples > decodable)
    info.samples = decodable; // A failed capture may have no final padding frame.
  info.data_crc = checksum ^ UINT32_MAX;
  if (!hash_text(text, info.text_hash) || !commit_header())
    return false;
  ESP_LOGI("replay", "Cached frames=%lu samples=%lu bytes=%u max_write_us=%lld",
           (unsigned long)info.frames, (unsigned long)info.samples, (unsigned)written,
           max_write_us);
  return true;
}
bool memo_replay_matches(const char *text, uint32_t record_id) {
  uint8_t hash[32];
  return ready && !writing && info.record_id == record_id &&
         (record_id || (hash_text(text, hash) &&
                        !memcmp(hash, info.text_hash, sizeof(hash))));
}
bool memo_replay_bind(uint32_t record_id) {
  if (!ready || !record_id || info.record_id)
    return false;
  ready = false;
  info.record_id = record_id;
  /* Audio is unchanged. A torn metadata update only disables replay. */
  return esp_partition_erase_range(partition, 0, MEMO_REPLAY_DATA_OFFSET) == ESP_OK &&
         commit_header();
}
void memo_replay_forget(uint32_t record_id) {
  if (partition && !writing && record_id && info.record_id == record_id) {
    ready = false;
    // Invalidate metadata first, even if erasing the payload later fails.
    uint32_t invalid = 0;
    esp_partition_write(partition, 0, &invalid, sizeof(invalid));
    if (esp_partition_erase_range(partition, 0, partition->size) != ESP_OK)
      ESP_LOGW("replay", "Replay erasure failed");
  }
}
void memo_replay_stop(void) { atomic_store(&stopped, true); }
void memo_replay_volume_step(int delta) {
  int next = atomic_load(&volume) + delta;
  atomic_store(&volume, next < 10 ? 10 : next > 100 ? 100 : next);
}
unsigned memo_replay_volume(void) { return atomic_load(&volume); }

typedef struct {
  SemaphoreHandle_t done;
  memo_replay_progress_t progress;
  esp_err_t result;
} playback_t;

static esp_err_t verify_audio(void) {
  size_t remaining = info.frames * MEMO_REPLAY_PACKET_BYTES;
  uint32_t crc = UINT32_MAX;
  for (size_t offset = 0; offset < remaining;) {
    if (atomic_load(&stopped))
      return ESP_ERR_INVALID_STATE;
    size_t n = remaining - offset;
    if (n > sizeof(buffer))
      n = sizeof(buffer);
    if (esp_partition_read(partition, MEMO_REPLAY_DATA_OFFSET + offset, buffer, n) != ESP_OK)
      return ESP_FAIL;
    crc = memo_replay_crc(crc, buffer, n);
    offset += n;
    vTaskDelay(1);
  }
  return (crc ^ UINT32_MAX) == info.data_crc ? ESP_OK : ESP_ERR_INVALID_CRC;
}
static void playback(void *arg) {
  playback_t *job = arg;
  void *decoder = NULL;
  esp_err_t error = verify_audio();
  uint8_t packet[MEMO_REPLAY_PACKET_BYTES];
  int16_t pcm[MEMO_REPLAY_FRAME_SAMPLES];
  unsigned played = 0, applied_volume = memo_replay_volume();
  int64_t decode_max = 0;
  if (error != ESP_OK) {
    if (!atomic_load(&stopped))
      ready = false;
    goto end;
  }
  esp_opus_dec_cfg_t cfg = ESP_OPUS_DEC_CONFIG_DEFAULT();
  cfg.sample_rate = 16000;
  cfg.channel = 1;
  cfg.frame_duration = ESP_OPUS_DEC_FRAME_DURATION_20_MS;
  cfg.self_delimited = false;
  if (esp_opus_dec_open(&cfg, sizeof(cfg), &decoder) != ESP_AUDIO_ERR_OK) {
    error = ESP_ERR_NO_MEM;
    goto end;
  }
  bsp_audio_set_volume(applied_volume);
  for (uint32_t i = 0; i < info.frames && !atomic_load(&stopped); i++) {
    if (esp_partition_read(partition, MEMO_REPLAY_DATA_OFFSET + i * sizeof(packet),
                            packet, sizeof(packet)) != ESP_OK) {
      error = ESP_FAIL;
      break;
    }
    esp_audio_dec_in_raw_t raw = {.buffer = packet, .len = sizeof(packet)};
    esp_audio_dec_out_frame_t frame = {.buffer = (uint8_t *)pcm, .len = sizeof(pcm)};
    esp_audio_dec_info_t decoded = {0};
    int64_t start = esp_timer_get_time();
    esp_audio_err_t result = esp_opus_dec_decode(decoder, &raw, &frame, &decoded);
    int64_t elapsed = esp_timer_get_time() - start;
    if (elapsed > decode_max)
      decode_max = elapsed;
    if (result != ESP_AUDIO_ERR_OK || raw.consumed != sizeof(packet) ||
        frame.decoded_size != sizeof(pcm) || decoded.sample_rate != 16000 ||
        decoded.channel != 1 || decoded.bits_per_sample != 16) {
      error = ESP_ERR_INVALID_RESPONSE;
      break;
    }
    unsigned wanted_volume = memo_replay_volume();
    if (applied_volume != wanted_volume) {
      applied_volume = wanted_volume;
      bsp_audio_set_volume(applied_volume);
    }
    size_t skip, samples = memo_replay_slice(&info, i, &skip);
    if (!samples)
      break;
    unsigned level = memo_playback_prepare(pcm + skip, samples);
    if (bsp_audio_write(pcm + skip, samples * sizeof(pcm[0])) != ESP_OK) {
      error = ESP_FAIL;
      break;
    }
    played += samples;
    if (i % 5 == 0 || played == info.samples) {
      job->progress(played / 16000, (info.samples + 15999) / 16000,
                    level);
    }
  }
  /* Replace queued TX data with silence and let the six DMA buffers drain. */
  memset(pcm, 0, sizeof(pcm));
  for (unsigned i = 0; i < 6; i++)
    if (bsp_audio_write(pcm, sizeof(pcm)) != ESP_OK)
      break;
  vTaskDelay(pdMS_TO_TICKS(100));
end:
  if (decoder)
    esp_opus_dec_close(decoder);
  if (error == ESP_OK && atomic_load(&stopped))
    error = ESP_ERR_INVALID_STATE;
  ESP_LOGI("replay", "Playback result=%s samples=%u stopped=%d max_decode_us=%lld stack_free=%u",
           esp_err_to_name(error), played, atomic_load(&stopped), decode_max,
           (unsigned)uxTaskGetStackHighWaterMark(NULL));
  job->result = error;
  xSemaphoreGive(job->done);
  vTaskDelete(NULL);
}
esp_err_t memo_replay_play(memo_replay_progress_t progress) {
  if (!ready || !progress)
    return ESP_ERR_INVALID_STATE;
  playback_t job = {.done = xSemaphoreCreateBinary(), .progress = progress,
                     .result = ESP_FAIL};
  if (!job.done)
    return ESP_ERR_NO_MEM;
  atomic_store(&stopped, false);
  progress(0, (info.samples + 15999) / 16000, 0);
  /* This stack exists only after the encoder and TLS session have been freed. */
  if (xTaskCreate(playback, "memo_playback", 24576, &job, 5, NULL) == pdPASS)
    xSemaphoreTake(job.done, portMAX_DELAY);
  else
    job.result = ESP_ERR_NO_MEM;
  vSemaphoreDelete(job.done);
  return job.result;
}
