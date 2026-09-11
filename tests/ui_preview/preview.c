#include "lvgl.h"
#include "memo_app.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static memo_view_t view;
static uint16_t frame[240 * 320];
void memo_snapshot(memo_view_t *out) { *out = view; }
static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels) {
  uint16_t *p = (uint16_t *)pixels;
  for (int y = area->y1; y <= area->y2; y++)
    for (int x = area->x1; x <= area->x2; x++)
      frame[y * 240 + x] = *p++;
  lv_display_flush_ready(display);
}
int main(int argc, char **argv) {
  if (argc != 3)
    return 2;
  view.phase = atoi(argv[1]);
  view.revision = 1;
  view.battery = 78;
  view.online = true;
  view.link = MEMO_LINK_READY;
  view.configured = true;
  if (view.phase == MEMO_SETTINGS) {
    view.portal = true;
    view.online = false;
    view.link = MEMO_LINK_PROVISIONING;
  }
  view.audio_ok = true;
  view.count = 6;
  view.level = 64;
  view.seconds = 12;
  view.replay_available = true;
  view.playback_seconds = 5;
  view.playback_total = 12;
  view.playback_volume = 85;
  view.selected_id = 1;
  strcpy(view.message,
         view.phase == MEMO_REVIEW ? "识别完成，确定保存" : "按确定，说下你的灵感");
  strcpy(view.setup_key, "12345678");
  strcpy(view.ip, "http://192.168.1.100");
  if (view.phase == MEMO_RECORDING || view.phase == MEMO_REVIEW ||
      view.phase == MEMO_HISTORY_PAGE || view.phase == MEMO_PLAYBACK)
    strcpy(view.text, "今天的灵感：做一个不用打开手机，也能随时记录想法的小伙伴"
                      "。说完就能留在桌面的墨水屏上。");
  if (getenv("MEMO_PREVIEW_MAX_TEXT")) {
    view.text[0] = 0;
    for (int i = 0; i < MEMO_MAX_CHARS; i++)
      strcat(view.text, "中");
  }
  lv_init();
  lv_display_t *display = lv_display_create(240, 320);
  static uint16_t buffer[240 * 20];
  lv_display_set_buffers(display, buffer, NULL, sizeof(buffer),
                         LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(display, flush);
  memo_ui_start();
  for (int i = 0; i < 35; i++) {
    lv_tick_inc(10);
    lv_timer_handler();
  }
  lv_refr_now(display);
  FILE *f = fopen(argv[2], "wb");
  if (!f)
    return 3;
  fprintf(f, "P6\n240 320\n255\n");
  for (int i = 0; i < 240 * 320; i++) {
    uint16_t v = frame[i];
    uint8_t rgb[3] = {(v >> 11) * 255 / 31, ((v >> 5) & 63) * 255 / 63,
                      (v & 31) * 255 / 31};
    fwrite(rgb, 1, 3, f);
  }
  fclose(f);
  lv_mem_monitor_t memory;
  lv_mem_monitor(&memory);
  printf("LVGL peak %zu bytes / 40960; free %zu\n", memory.max_used, memory.free_size);
  lv_deinit();
  return 0;
}
