#include "bsp_display.h"
#include "lvgl.h"
#include "memo_app.h"
#include "ui_pixel.h"
#include <stdio.h>
#include <string.h>
LV_FONT_DECLARE(memo_font_16);
static lv_obj_t *screen, *status, *battery, *paper, *content, *caption, *hint, *mascot,
    *bars[18];
static uint32_t revision;
static memo_phase_t shown_phase = 99;
static uint32_t shown_id;
static int scroll_y;
static void set_text(lv_obj_t *label, const char *text) {
  // Audio levels revise the view at 10 Hz even when the transcript is unchanged.
  // Reuse LVGL's owned string to avoid reallocating and redrawing that text.
  if (strcmp(lv_label_get_text(label), text))
    lv_label_set_text(label, text);
}
static const char *phase_name(memo_phase_t p) {
  static const char *names[] = {"灵感收件箱", "正在连接",     "正在聆听",
                                "正在整理",   "确认这条备忘", "我的备忘录",
                                "设备设置",   "暂时遇到问题", "原声回放"};
  return p <= MEMO_PLAYBACK ? names[p] : "";
}
static void update(lv_timer_t *timer) {
  (void)timer;
  static memo_view_t v;
  memo_snapshot(&v);
  if (v.revision == revision)
    return;
  revision = v.revision;
  if (shown_phase != v.phase || shown_id != v.selected_id) {
    scroll_y = 0;
    shown_phase = v.phase;
    shown_id = v.selected_id;
  }
  char b[160];
  if (v.battery >= 0)
    snprintf(b, sizeof(b), "%d%%", v.battery);
  else
    snprintf(b, sizeof(b), "--%%");
  set_text(battery, b);
  snprintf(b, sizeof(b), "%s  %s", v.online ? "●" : "○", phase_name(v.phase));
  set_text(status, b);
  lv_obj_set_style_bg_color(
      paper, lv_color_hex(v.phase == MEMO_ERROR ? 0xFFE4D4 : UI_PAPER), 0);
  if (v.phase == MEMO_HOME) {
    snprintf(b, sizeof(b),
             "记录一句，留住灵感。\n\n确定键开始录音\n上下键查看历史\n\n已收藏 "
             "%d / 32 条",
             v.count);
    set_text(content, b);
    set_text(hint, "确定  开始录音");
  } else if (v.phase == MEMO_SETTINGS) {
    char settings[320];
    snprintf(settings, sizeof(settings),
             "热点 Passport-Memo\n密码\n%s\n192.168.4.1\n%s",
             v.setup_key[0] ? v.setup_key : "开启中…",
             v.portal ? "保存后按上键联网" : "长按确定开启热点");
    set_text(content, settings);
    set_text(hint, "上 / 确定  返回");
  } else {
    set_text(content, v.text[0]
                                   ? v.text
                                   : (v.phase == MEMO_CONNECTING
                                          ? "正在建立安全连接…\n\n准备好后再说话。"
                                      : v.phase == MEMO_RECORDING
                                          ? "说下此刻的想法。\n\n识别文字会出现在这里。"
                                          : v.message));
    if (v.phase == MEMO_HISTORY_PAGE)
      snprintf(b, sizeof(b), "%d/%d  %s  %s", v.count ? v.selected + 1 : 0, v.count,
               v.done ? "已完成" : "待办", v.synced ? "已同步" : "本机");
    else
      snprintf(b, sizeof(b), "%02lu:%02lu  %s", (unsigned long)v.seconds / 60,
               (unsigned long)v.seconds % 60,
               v.partial                   ? "草稿"
               : v.phase == MEMO_RECORDING ? "OPUS"
                                           : "");
    set_text(hint, v.phase == MEMO_HISTORY_PAGE ? b
                            : v.phase == MEMO_PLAYBACK ? "确定停止  上下音量"
                            : memo_active(v.phase) ? "确定  停止录音"
                            : v.replay_available && v.text[0] ? "确定保存 双击回放"
                            : v.replay_available ? "双击确定 回放原声"
                            : v.text[0] ? "确定  保存备忘" : "长按上键 返回");
  }
  if (v.phase == MEMO_PLAYBACK)
    snprintf(b, sizeof(b), "%02u:%02u / %02u:%02u  音量 %u%%",
             v.playback_seconds / 60, v.playback_seconds % 60,
             v.playback_total / 60, v.playback_total % 60, v.playback_volume);
  else if (memo_active(v.phase))
    snprintf(b, sizeof(b), "%02lu:%02lu  %s", (unsigned long)v.seconds / 60,
             (unsigned long)v.seconds % 60,
             v.phase == MEMO_RECORDING   ? "OPUS · 16k"
             : v.phase == MEMO_FINISHING ? "整理中"
                                         : "连接中");
  else if ((v.phase == MEMO_HOME || v.phase == MEMO_SETTINGS) &&
           v.link != MEMO_LINK_READY)
    snprintf(b, sizeof(b), "%s", memo_link_message(v.link));
  else if (v.phase == MEMO_SETTINGS && v.configured)
    snprintf(b, sizeof(b), "保存后按上键返回并联网");
  else if (v.phase == MEMO_HISTORY_PAGE && v.replay_available)
    snprintf(b, sizeof(b), "双击确定回放 · %.120s", v.message);
  else
    snprintf(b, sizeof(b), "%s", v.message);
  set_text(caption, b);
  lv_obj_update_layout(content);
  int max_scroll = lv_obj_get_height(content) - lv_obj_get_content_height(paper);
  if (max_scroll < 0)
    max_scroll = 0;
  if (v.phase == MEMO_RECORDING || v.phase == MEMO_FINISHING)
    scroll_y = max_scroll;
  if (scroll_y > max_scroll)
    scroll_y = max_scroll;
  lv_obj_set_y(content, -scroll_y);
  for (int i = 0; i < 18; i++) {
    int h = 3;
    if (v.phase == MEMO_RECORDING || v.phase == MEMO_PLAYBACK)
      h = 3 + v.level * (4 + (i * 7) % 13) / 70;
    if (h > 24)
      h = 24;
    lv_obj_set_height(bars[i], h);
    lv_obj_set_y(bars[i], 238 - h / 2);
    lv_obj_set_style_bg_color(
        bars[i], lv_color_hex(v.phase == MEMO_RECORDING || v.phase == MEMO_PLAYBACK
                                  ? UI_YELLOW : UI_PAPER), 0);
  }
}
void memo_ui_scroll(int direction) {
  if (!bsp_lvgl_lock(500))
    return;
  int max_scroll = lv_obj_get_height(content) - lv_obj_get_content_height(paper);
  if (max_scroll < 0)
    max_scroll = 0;
  scroll_y += direction * 72;
  if (scroll_y < 0)
    scroll_y = 0;
  if (scroll_y > max_scroll)
    scroll_y = max_scroll;
  lv_obj_set_y(content, -scroll_y);
  bsp_lvgl_unlock();
}
static void auto_read(lv_timer_t *timer) {
  (void)timer;
  if (shown_phase != MEMO_HISTORY_PAGE)
    return;
  int max_scroll = lv_obj_get_height(content) - lv_obj_get_content_height(paper);
  if (max_scroll <= 0)
    return;
  scroll_y += 72;
  if (scroll_y > max_scroll + 72)
    scroll_y = 0;
  lv_obj_set_y(content, -(scroll_y > max_scroll ? max_scroll : scroll_y));
}
void memo_ui_start(void) {
  if (!bsp_lvgl_lock(1000))
    return;
  screen = ui_pixel_screen_create("MEMO");
  battery = ui_pixel_label(screen, "--%", &lv_font_montserrat_14, 0xffffff);
  lv_obj_set_pos(battery, 175, 30);
  status = ui_pixel_label(screen, "", &memo_font_16, 0xffffff);
  lv_obj_set_pos(status, 14, 54);
  paper = ui_pixel_panel_create(screen, 11, 78, 212, 140, UI_PAPER);
  lv_obj_set_style_clip_corner(paper, true, 0);
  content = ui_pixel_label(paper, "", &memo_font_16, UI_INK);
  lv_obj_set_width(content, 190);
  lv_label_set_long_mode(content, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_line_space(content, 3, 0);
  lv_obj_set_pos(content, 0, 0);
  for (int i = 0; i < 18; i++) {
    bars[i] = lv_obj_create(screen);
    lv_obj_remove_style_all(bars[i]);
    lv_obj_set_size(bars[i], 5, 3);
    lv_obj_set_pos(bars[i], 18 + i * 9, 237);
    lv_obj_set_style_bg_opa(bars[i], LV_OPA_COVER, 0);
  }
  mascot = ui_pixel_mascot_create(screen, 190, 230);
  hint = ui_pixel_label(screen, "", &memo_font_16, 0xffffff);
  lv_obj_set_pos(hint, 14, 260);
  lv_obj_set_width(hint, 173);
  caption = ui_pixel_label(screen, "", &memo_font_16, UI_INK);
  lv_obj_set_pos(caption, 12, 292);
  lv_obj_set_width(caption, 216);
  lv_label_set_long_mode(caption, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_screen_load(screen);
  lv_timer_create(update, 50, NULL);
  lv_timer_create(auto_read, 4500, NULL);
  bsp_lvgl_unlock();
}
