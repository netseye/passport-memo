#include "bsp_audio.h"
#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_opus_enc.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_websocket_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "memo_app.h"
#include "memo_ogg.h"
#include "memo_replay.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if CONFIG_WS_BUFFER_SIZE < 8192
#error "Passport ASR requires an 8 KB WebSocket HTTP upgrade buffer"
#endif

#define CONNECTED 1
#define FAILED 2
#define FINAL 4
#define CAPTURE_DONE 8
#define ENCODER_READY 16
#define PCM_BYTES 640
#define WIRE_BYTES 512
#define CAPTURE_STACK_BYTES 40960
typedef struct {
  uint16_t size;
  uint8_t data[WIRE_BYTES];
} audio_chunk_t;
static atomic_bool stop_requested, cancel_requested, failure_claimed;
static atomic_uint elapsed_ms, peak;
static atomic_int current_phase;
static EventGroupHandle_t events;
static QueueHandle_t audio_queue;
static uint8_t *rx;
static memo_stream_t stream;
static bool limited;
static char transcript[MEMO_TEXT_BYTES], failure[120];
static void fail(const char *message) {
  bool expected = false;
  if (!atomic_compare_exchange_strong(&failure_claimed, &expected, true))
    return;
  snprintf(failure, sizeof(failure), "%s", message);
  xEventGroupSetBits(events, FAILED);
  atomic_store(&stop_requested, true);
}
void memo_asr_stop(bool cancel) {
  atomic_store(&stop_requested, true);
  if (cancel)
    atomic_store(&cancel_requested, true);
}
static void publish(const char *message) {
  memo_asr_update(atomic_load(&current_phase), transcript, message,
                  atomic_load(&elapsed_ms) / 1000, atomic_load(&peak), limited);
}
static void receive_packet(void) {
  memo_packet_t p;
  if (!memo_response(rx, stream.used, &p)) {
    fail("识别协议异常，已保留文字");
    return;
  }
  if (p.error) {
    char message[120];
    const char *reason = memo_asr_service_message(p.error, p.payload, p.size);
    ESP_LOGW("memo", "ASR rejected: code=%lu reason=%s", (unsigned long)p.error,
             reason);
    snprintf(message, sizeof(message), "%lu %s", (unsigned long)p.error, reason);
    fail(message);
    return;
  }
  cJSON *root = cJSON_ParseWithLength((const char *)p.payload, p.size);
  if (!root) {
    fail("识别结果无法解析");
    return;
  }
  cJSON *code = cJSON_GetObjectItem(root, "code");
  if (cJSON_IsNumber(code) && code->valuedouble != 0 && code->valuedouble != 20000000) {
    cJSON_Delete(root);
    fail("火山返回失败，请检查资源权限");
    return;
  }
  cJSON *result = cJSON_GetObjectItem(root, "result");
  if (cJSON_IsArray(result))
    result = cJSON_GetArrayItem(result, 0);
  cJSON *text = cJSON_GetObjectItem(result, "text");
  if (cJSON_IsString(text) && text->valuestring) {
    size_t n =
        memo_text_prefix(text->valuestring, MEMO_MAX_CHARS, sizeof(transcript) - 1);
    memcpy(transcript, text->valuestring, n);
    transcript[n] = 0;
    if (n < strlen(text->valuestring)) {
      limited = true;
      atomic_store(&stop_requested, true);
    }
    publish(limited ? "已达240字，正在结束识别" : "正在听，请继续说…");
  }
  cJSON_Delete(root);
  // Definite utterances are not the final session result. Only frame flags end
  // it.
  if (p.final)
    xEventGroupSetBits(events, FINAL);
}
static void websocket_event(void *arg, esp_event_base_t base, int32_t id,
                            void *event_data) {
  (void)arg;
  (void)base;
  if (id == WEBSOCKET_EVENT_CONNECTED) {
    xEventGroupSetBits(events, CONNECTED);
    return;
  }
  if (id == WEBSOCKET_EVENT_DISCONNECTED || id == WEBSOCKET_EVENT_ERROR) {
    if (!(xEventGroupGetBits(events) & FINAL)) {
      esp_websocket_event_data_t *d = event_data;
      int status = d ? d->error_handle.esp_ws_handshake_status_code : 0;
      // The client only initializes TLS details for TCP_TRANSPORT events.
      bool tls_error =
          d && d->error_handle.error_type == WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT &&
          (d->error_handle.esp_tls_stack_err ||
           d->error_handle.esp_tls_cert_verify_flags);
      bool connected = xEventGroupGetBits(events) & CONNECTED;
      ESP_LOGW("memo", "ASR connection failed: http=%d tls=%d connected=%d", status,
               tls_error, connected);
      // ERROR can precede initialization of the transport error details.
      // DISCONNECTED then carries the actual TLS/HTTP failure; do not let an
      // early generic message hide it. A missing event is bounded by timeout.
      if (id == WEBSOCKET_EVENT_ERROR && !status && !tls_error && !connected)
        return;
      fail(memo_asr_connection_message(status, tls_error, connected));
    }
    return;
  }
  if (id != WEBSOCKET_EVENT_DATA)
    return;
  esp_websocket_event_data_t *d = event_data;
  if (d->op_code != 2 && d->op_code != 0)
    return;
  if (d->data_len < 0 || d->payload_len < 0 || d->payload_offset < 0) {
    fail("识别分片异常");
    return;
  }
  int result =
      memo_stream_feed(&stream, rx, MEMO_RX_BYTES, d->op_code, d->payload_offset,
                       d->payload_len, d->fin, d->data_ptr, d->data_len);
  if (result < 0)
    fail("识别响应过大或分片异常");
  else if (result == 1)
    receive_packet();
}
static void capture(void *arg) {
  (void)arg;
  uint8_t pcm[PCM_BYTES];
  unsigned ms = 0;
  int64_t encode_total = 0, encode_max = 0;
  unsigned encoded_frames = 0;
  void *encoder = NULL;
  uint8_t *raw = NULL;
  audio_chunk_t *chunk = calloc(1, sizeof(*chunk));
  uint32_t serial = esp_random(), sequence = 2;
  uint64_t granule = 0;
  esp_opus_enc_config_t cfg = ESP_OPUS_ENC_CONFIG_DEFAULT();
  cfg.sample_rate = 16000;
  cfg.channel = 1;
  cfg.bits_per_sample = 16;
  cfg.bitrate = 16000;
  cfg.frame_duration = ESP_OPUS_ENC_FRAME_DURATION_20_MS;
  cfg.complexity = 0;
  cfg.enable_vbr = false;
  cfg.enable_dtx = false;
  int in_bytes = 0, out_bytes = 0;
  if (!chunk || esp_opus_enc_open(&cfg, sizeof(cfg), &encoder) != ESP_AUDIO_ERR_OK) {
    fail("Opus 编码器内存不足");
    goto end;
  }
  if (esp_opus_enc_get_frame_size(encoder, &in_bytes, &out_bytes) != ESP_AUDIO_ERR_OK ||
      in_bytes != PCM_BYTES || out_bytes > 1275 || out_bytes < 1) {
    fail("Opus 帧格式不匹配");
    goto end;
  }
  raw = malloc(out_bytes);
  if (!raw) {
    fail("Opus 输出缓冲不足");
    goto end;
  }
  ESP_LOGI("memo", "Opus ready; heap=%lu largest=%lu",
           (unsigned long)esp_get_free_heap_size(),
           (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
  // Allocate the stack and encoder state before TLS fragments the C3 heap.
  // No microphone read or audio upload occurs until the request is accepted.
  xEventGroupSetBits(events, ENCODER_READY);
  if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY) != 1)
    goto end;
  chunk->size = memo_ogg_headers(chunk->data, sizeof(chunk->data), serial, 312);
  if (xQueueSend(audio_queue, chunk, pdMS_TO_TICKS(200)) != pdTRUE) {
    fail("音频队列未就绪");
    goto end;
  }
  chunk->size = 0;
  for (int i = 0; i < 6; i++)
    if (bsp_audio_read(pcm, sizeof(pcm)) != ESP_OK) {
      fail("麦克风读取失败");
      goto end;
    }
  atomic_store(&current_phase, MEMO_RECORDING);
  memo_asr_update(MEMO_RECORDING, "", "正在听，请开始说话…", 0, 0, false);
  bool padding = false;
  for (;;) {
    padding = atomic_load(&stop_requested) || ms >= MEMO_MAX_SECONDS * 1000;
    if (padding)
      memset(pcm, 0, sizeof(pcm));
    else if (bsp_audio_read(pcm, sizeof(pcm)) != ESP_OK) {
      fail("麦克风读取失败，已保留文字");
      break;
    }
    esp_audio_enc_in_frame_t in = {.buffer = pcm, .len = sizeof(pcm)};
    esp_audio_enc_out_frame_t out = {.buffer = raw, .len = out_bytes};
    int64_t encode_start = esp_timer_get_time();
    if (esp_opus_enc_process(encoder, &in, &out) != ESP_AUDIO_ERR_OK) {
      fail("Opus 编码失败");
      break;
    }
    int64_t encode_us = esp_timer_get_time() - encode_start;
    encode_total += encode_us;
    if (encode_us > encode_max)
      encode_max = encode_us;
    encoded_frames++;
    memo_replay_append(raw, out.encoded_bytes, padding ? 0 : PCM_BYTES / 2);
    granule += 960;
    size_t n =
        memo_ogg_page(chunk->data + chunk->size, sizeof(chunk->data) - chunk->size, raw,
                      out.encoded_bytes, serial, sequence++,
                      padding ? (uint64_t)ms * 48 + 312 : granule, padding ? 4 : 0);
    if (!n) {
      fail("Ogg 封装缓冲不足");
      break;
    }
    chunk->size += n;
    if (padding || ms % 100 == 80) {
      if (xQueueSend(audio_queue, chunk, pdMS_TO_TICKS(150)) != pdTRUE) {
        fail("网络发送过慢，录音已停止");
        break;
      }
      chunk->size = 0;
    }
    if (padding)
      break;
    uint32_t sum = 0;
    const int16_t *samples = (const int16_t *)pcm;
    for (size_t i = 0; i < sizeof(pcm) / 2; i++) {
      int32_t v = samples[i];
      sum += (v < 0 ? -v : v);
    }
    unsigned level = sum / (sizeof(pcm) / 2) / 70;
    if (level > 100)
      level = 100;
    ms += 20;
    atomic_store(&elapsed_ms, ms);
    atomic_store(&peak, level);
    if (ms % 100 == 0)
      memo_asr_update(MEMO_RECORDING, NULL, NULL, ms / 1000, level, false);
  }
end:
  ESP_LOGI("memo", "Opus frames=%u average_us=%lld max_us=%lld stack_free=%u",
           encoded_frames, encoded_frames ? encode_total / encoded_frames : 0,
           encode_max, (unsigned)uxTaskGetStackHighWaterMark(NULL));
  if (encoder)
    esp_opus_enc_close(encoder);
  free(raw);
  free(chunk);
  xEventGroupSetBits(events, CAPTURE_DONE);
  vTaskDelete(NULL);
}
void memo_asr_run(const memo_config_t *config) {
  atomic_store(&stop_requested, false);
  atomic_store(&cancel_requested, false);
  atomic_store(&failure_claimed, false);
  atomic_store(&elapsed_ms, 0);
  atomic_store(&peak, 0);
  atomic_store(&current_phase, MEMO_CONNECTING);
  transcript[0] = failure[0] = 0;
  memset(&stream, 0, sizeof(stream));
  limited = false;
  memo_replay_begin();
  events = xEventGroupCreate();
  audio_queue = NULL;
  rx = NULL;
  uint8_t *packet = NULL;
  char *headers = NULL;
  esp_websocket_client_handle_t client = NULL;
  TaskHandle_t capture_task = NULL;
  bool started = false, capturing = false, capture_started = false, successful = false;
  if (!events) {
    snprintf(failure, sizeof(failure), "事件内存不足，请重启后重试");
    goto cleanup;
  }
  ESP_LOGI("memo", "Capture reserve: heap=%lu largest=%lu stack=%u",
           (unsigned long)esp_get_free_heap_size(),
           (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
           CAPTURE_STACK_BYTES);
  if (xTaskCreate(capture, "memo_capture", CAPTURE_STACK_BYTES, NULL, 6,
                  &capture_task) != pdPASS) {
    fail("录音内存不足，请重启后重试");
    goto cleanup;
  }
  capturing = true;
  if (xEventGroupWaitBits(events, ENCODER_READY | FAILED, pdFALSE, pdFALSE,
                          portMAX_DELAY) &
      FAILED)
    goto cleanup;
  // 16 kbps CBR / 20 ms = 40 bytes; five Ogg pages need only 340 bytes.
  audio_queue = xQueueCreate(4, sizeof(audio_chunk_t));
  rx = malloc(MEMO_RX_BYTES);
  packet = malloc(WIRE_BYTES + 8);
  headers = calloc(1, 1200);
  if (!audio_queue || !rx || !packet || !headers) {
    snprintf(failure, sizeof(failure), "内存不足，请重启后重试");
    goto cleanup;
  }
  char uuid[37];
  uint8_t b[16];
  esp_fill_random(b, sizeof(b));
  b[6] = (b[6] & 15) | 0x40;
  b[8] = (b[8] & 63) | 0x80;
  snprintf(uuid, sizeof(uuid),
           "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x", b[0],
           b[1], b[2], b[3], b[4], b[5], b[6], b[7], b[8], b[9], b[10], b[11], b[12],
           b[13], b[14], b[15]);
  if (config->api_key[0])
    snprintf(headers, 1200, "X-Api-Key: %s\r\n", config->api_key);
  else
    snprintf(headers, 1200, "X-Api-App-Key: %s\r\nX-Api-Access-Key: %s\r\n",
             config->app_id, config->access_key);
  size_t n = strlen(headers);
  snprintf(headers + n, 1200 - n,
           "X-Api-Resource-Id: %s\r\nX-Api-Connect-Id: %s\r\nX-Api-Request-Id: "
           "%s\r\nX-Api-Sequence: -1\r\n",
           config->resource, uuid, uuid);
  esp_websocket_client_config_t ws = {
      .uri = "wss://openspeech.bytedance.com/api/v3/sauc/bigmodel_async",
      .headers = headers,
      .disable_auto_reconnect = true,
      .crt_bundle_attach = esp_crt_bundle_attach,
      .task_stack = 6144,
      .buffer_size = 1024,
      .network_timeout_ms = 5000,
      .ping_interval_sec = 15,
  };
  client = esp_websocket_client_init(&ws);
  if (!client) {
    fail("无法创建安全连接");
    goto cleanup;
  }
  esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY, websocket_event, NULL);
  if (esp_websocket_client_start(client) != ESP_OK) {
    fail("连接启动失败");
    goto cleanup;
  }
  started = true;
  for (int i = 0; i < 150; i++) {
    EventBits_t bits = xEventGroupWaitBits(events, CONNECTED | FAILED, pdFALSE, pdFALSE,
                                           pdMS_TO_TICKS(100));
    if (bits & FAILED || atomic_load(&stop_requested))
      goto cleanup;
    if (bits & CONNECTED)
      break;
  }
  if (!(xEventGroupGetBits(events) & CONNECTED)) {
    fail("连接超时，请检查网络");
    goto cleanup;
  }
  const char *request =
      "{\"user\":{\"uid\":\"passport-memo\"},\"audio\":{\"format\":\"ogg\","
      "\"codec\":\"opus\",\"rate\":16000,\"bits\":16,\"channel\":1},"
      "\"request\":{\"model_name\":\"bigmodel\",\"enable_itn\":true,\"enable_"
      "punc\":true,\"enable_ddc\":false,\"enable_nonstream\":true,\"show_"
      "utterances\":false,\"result_type\":\"full\"}}";
  n = memo_request(packet, WIRE_BYTES + 8, request, strlen(request), false, false);
  if (esp_websocket_client_send_bin(client, (char *)packet, n, pdMS_TO_TICKS(3000)) !=
      (int)n) {
    fail("识别请求发送失败");
    goto cleanup;
  }
  ESP_LOGI("memo", "ASR ready: heap=%lu largest=%lu",
           (unsigned long)esp_get_free_heap_size(),
           (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
  capture_started = true;
  xTaskNotify(capture_task, 1, eSetValueWithOverwrite);
  for (;;) {
    if (xEventGroupGetBits(events) & (FAILED | FINAL) || atomic_load(&cancel_requested))
      break;
    if (xQueueReceive(audio_queue, packet + 6, pdMS_TO_TICKS(100))) {
      uint16_t encoded_size;
      memcpy(&encoded_size, packet + 6, sizeof(encoded_size));
      n = memo_request(packet, WIRE_BYTES + 8, packet + 8, encoded_size, true, false);
      if (esp_websocket_client_send_bin(client, (char *)packet, n,
                                        pdMS_TO_TICKS(1500)) != (int)n) {
        fail("音频发送失败，已保留文字");
        break;
      }
    } else if (xEventGroupGetBits(events) & CAPTURE_DONE)
      break;
  }
  if ((xEventGroupGetBits(events) & FINAL) && !(xEventGroupGetBits(events) & FAILED)) {
    successful = true;
    goto cleanup;
  }
  if (!(xEventGroupGetBits(events) & FAILED) && !atomic_load(&cancel_requested)) {
    atomic_store(&current_phase, MEMO_FINISHING);
    memo_asr_update(MEMO_FINISHING, NULL, "正在整理最后一句…",
                    atomic_load(&elapsed_ms) / 1000, 0, false);
    n = memo_request(packet, WIRE_BYTES + 8, NULL, 0, true, true);
    if (esp_websocket_client_send_bin(client, (char *)packet, n, pdMS_TO_TICKS(2000)) !=
        (int)n)
      fail("结束请求发送失败");
    EventBits_t bits = xEventGroupWaitBits(events, FINAL | FAILED, pdFALSE, pdFALSE,
                                           pdMS_TO_TICKS(10000));
    successful = (bits & FINAL) && !(bits & FAILED);
    if (!successful && !(bits & FAILED))
      fail("最终结果超时，请检查草稿");
  }
cleanup:
  atomic_store(&stop_requested, true);
  if (capturing) {
    if (!capture_started && !(xEventGroupGetBits(events) & CAPTURE_DONE))
      xTaskNotify(capture_task, 2, eSetValueWithOverwrite);
    xEventGroupWaitBits(events, CAPTURE_DONE, pdFALSE, pdTRUE, portMAX_DELAY);
  }
  if (client) {
    if (started)
      esp_websocket_client_stop(client);
    esp_websocket_client_destroy(client);
  }
  if (headers) {
    memset(headers, 0, 1200);
    free(headers);
  }
  free(packet);
  free(rx);
  rx = NULL;
  if (audio_queue)
    vQueueDelete(audio_queue);
  audio_queue = NULL;
  if (events)
    vEventGroupDelete(events);
  events = NULL;
  memo_replay_finish(transcript);
  bool partial = !successful || limited;
  ESP_LOGI("memo", "ASR finished: success=%d text_bytes=%u partial=%d heap=%lu",
           successful, (unsigned)strlen(transcript), partial,
           (unsigned long)esp_get_free_heap_size());
  memo_asr_update(transcript[0] ? MEMO_REVIEW : MEMO_ERROR, transcript,
                  transcript[0]
                      ? (successful ? (limited ? "仅保留前240字，请确认后保存"
                                               : "识别完成，确定保存")
                                    : (failure[0] ? failure : "已停止，文字保留为草稿"))
                      : (failure[0] ? failure : "未识别到语音，长按上键返回"),
                  atomic_load(&elapsed_ms) / 1000, 0, partial);
}
