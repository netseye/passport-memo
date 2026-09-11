#include "memo_core.h"
#include "memo_ogg.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void test_fragments(void) {
  const uint8_t data[] = {0x11, 0x92, 0x10, 0, 0, 0, 0, 2, '{', '}'};
  uint8_t buffer[32];
  for (size_t split = 1; split < sizeof(data); split++) {
    memo_stream_t stream = {0};
    assert(memo_stream_feed(&stream, buffer, sizeof(buffer), 2, 0, sizeof(data), true,
                            data, split) == 0);
    assert(memo_stream_feed(&stream, buffer, sizeof(buffer), 2, split, sizeof(data),
                            true, data + split, sizeof(data) - split) == 1);
    assert(stream.used == sizeof(data) && !memcmp(buffer, data, sizeof(data)));
    stream = (memo_stream_t){0};
    assert(memo_stream_feed(&stream, buffer, sizeof(buffer), 2, 0, split, false, data,
                            split) == 0);
    assert(memo_stream_feed(&stream, buffer, sizeof(buffer), 0, 0, sizeof(data) - split,
                            true, data + split, sizeof(data) - split) == 1);
    assert(!memcmp(buffer, data, sizeof(data)));
  }
  memo_stream_t stream = {0};
  assert(memo_stream_feed(&stream, buffer, sizeof(buffer), 0, 0, 1, true, data, 1) ==
         -1);
  stream = (memo_stream_t){0};
  assert(memo_stream_feed(&stream, buffer, 8, 2, 0, sizeof(data), true, data, 1) == -1);
  stream = (memo_stream_t){0};
  assert(memo_stream_feed(&stream, buffer, sizeof(buffer), 2, 0, 10, true, data, 1) ==
         0);
  assert(memo_stream_feed(&stream, buffer, sizeof(buffer), 2, 2, 10, true, data, 1) ==
         -1);
}
static void test_service_errors(void) {
  const uint8_t concurrent[] = "quota exceeded for types: concurrency";
  const uint8_t duration[] =
      "{\"message\":\"quota exceeded for types: audio_duration_lifetime\"}";
  assert(strstr(memo_asr_service_message(45000292, concurrent, sizeof(concurrent) - 1),
                "并发"));
  assert(strstr(memo_asr_service_message(45000292, duration, sizeof(duration) - 1),
                "时长"));
  // Never scan beyond the provider-declared length, or need a terminator.
  assert(strstr(memo_asr_service_message(45000292, concurrent, 5), "配置"));
  assert(strstr(memo_asr_service_message(45000292, concurrent, 14), "配额"));
  assert(strstr(memo_asr_service_message(45000292, NULL, 100), "配置"));
  assert(strstr(memo_asr_service_message(45000151, NULL, 0), "格式"));
  assert(strstr(memo_asr_service_message(45000002, NULL, 0), "音频"));
  assert(strstr(memo_asr_service_message(45000081, NULL, 0), "超时"));
  assert(strstr(memo_asr_service_message(55000031, NULL, 0), "繁忙"));
}
int main(void) {
  test_fragments();
  test_service_errors();
  // A known server rejection must not be misreported as a transport failure.
  assert(strstr(memo_asr_connection_message(401, true, false), "401"));
  assert(strstr(memo_asr_connection_message(403, false, false), "资源权限"));
  assert(strstr(memo_asr_connection_message(429, false, false), "限流"));
  assert(strstr(memo_asr_connection_message(503, false, false), "暂不可用"));
  assert(strstr(memo_asr_connection_message(0, true, false), "安全连接"));
  assert(strstr(memo_asr_connection_message(0, false, false), "网络"));
  assert(strstr(memo_asr_connection_message(101, false, true), "已有文字保留"));
  size_t count = 0;
  assert(memo_text_valid("你好，Opus!\n", &count) && count == 9);
  assert(!memo_text_valid("\xc0\x80", NULL));
  assert(!memo_text_valid("\xed\xa0\x80", NULL));
  assert(!memo_text_valid("\xf4\x90\x80\x80", NULL));
  assert(!memo_text_valid("a\x01", NULL));
  char max[MEMO_TEXT_BYTES];
  max[0] = 0;
  for (int i = 0; i < 240; i++)
    strcat(max, "中");
  assert(memo_text_valid(max, &count) && count == 240);
  strcat(max, "a");
  assert(!memo_text_valid(max, NULL));
  assert(memo_text_prefix("中文ok", 1, 100) == 3);
  assert(memo_text_prefix("中文ok", 99, 4) == 3);
  assert(!memo_header_value("key\r\nHost: other", 40));
  assert(memo_header_value("safe-key", 40));
  uint8_t out[2048];
  size_t n = memo_request(out, sizeof(out), "{}", 2, false, false);
  assert(n == 10 && out[0] == 0x11 && out[1] == 0x10 && out[2] == 0x10 && out[7] == 2);
  n = memo_request(out, sizeof(out), NULL, 0, true, true);
  assert(n == 8 && out[1] == 0x22 && out[7] == 0);
  assert(!memo_request(out, 7, NULL, 0, true, true));
  const uint8_t final[] = {0x11, 0x93, 0x10, 0, 0xff, 0xff, 0xff,
                           0xfe, 0,    0,    0, 2,    '{',  '}'};
  memo_packet_t p;
  assert(memo_response(final, sizeof(final), &p) && p.final && p.sequence == -2 &&
         p.size == 2);
  for (size_t i = 0; i < sizeof(final); i++)
    assert(!memo_response(final, i, &p));
  memcpy(out, final, sizeof(final));
  out[2] = 0x11;
  assert(!memo_response(out, sizeof(final), &p));
  const uint8_t noseq[] = {0x11, 0x92, 0x10, 0, 0, 0, 0, 2, '{', '}'};
  assert(memo_response(noseq, sizeof(noseq), &p) && p.final);
  const uint8_t error[] = {0x11, 0xf0, 0, 0, 0, 0, 0, 42, 0, 0, 0, 1, 'x'};
  assert(memo_response(error, sizeof(error), &p) && p.error == 42);
  uint8_t bytes[510] = {0};
  n = memo_ogg_page(out, sizeof(out), bytes, 255, 12, 3, 4800, 0);
  assert(n == 284 && out[26] == 2 && out[27] == 255 && out[28] == 0);
  assert(!memo_ogg_page(out, 283, bytes, 255, 12, 3, 4800, 0));
  n = memo_ogg_headers(out, sizeof(out), 12, 312);
  assert(n == 99 && !memcmp(out, "OggS", 4) && out[5] == 2 &&
         !memcmp(out + 28, "OpusHead", 8));
  assert(!memcmp(out + 47, "OggS", 4) && !memcmp(out + 75, "OpusTags", 8));
  assert(memo_active(MEMO_PLAYBACK));
  assert(memo_active(MEMO_CONNECTING) && memo_active(MEMO_RECORDING) && memo_active(MEMO_FINISHING));
  assert(!memo_active(MEMO_REVIEW) && !memo_active(MEMO_HISTORY_PAGE));
  puts("Memo protocol, UTF-8 and Ogg boundary tests: PASS");
  return 0;
}
