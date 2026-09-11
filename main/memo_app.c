#include "memo_app.h"
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_display.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static SemaphoreHandle_t lock;
static QueueHandle_t keys;
static nvs_handle_t db;
static bool storage_ok;
static memo_config_t config;
static memo_view_t view;
static uint32_t ids[MEMO_HISTORY], next_id = 1;
static atomic_int phase;
static uint32_t last_activity;
static bool have_battery;
static void take(void) { xSemaphoreTake(lock, portMAX_DELAY); }
static void give(void) { xSemaphoreGive(lock); }
static void changed(void) {
  view.revision++;
  atomic_store(&phase, view.phase);
}
static void msg(const char *s) {
  snprintf(view.message, sizeof(view.message), "%s", s);
  changed();
}
static bool configured(void) {
  return config.ssid[0] &&
         (config.api_key[0] || (config.app_id[0] && config.access_key[0]));
}
static void record_key(char key[12], uint32_t id) {
  snprintf(key, 12, "n%02lu", (unsigned long)(id % MEMO_HISTORY));
}
static bool read_record(uint32_t id, memo_record_t *r) {
  char key[12];
  record_key(key, id);
  size_t n = sizeof(*r);
  return storage_ok && nvs_get_blob(db, key, r, &n) == ESP_OK && n == sizeof(*r) &&
         r->version == 1 && r->id == id && memchr(r->text, 0, sizeof(r->text)) &&
         memo_text_valid(r->text, NULL);
}
static bool write_record(const memo_record_t *r) {
  char key[12];
  record_key(key, r->id);
  return storage_ok && nvs_set_blob(db, key, r, sizeof(*r)) == ESP_OK &&
         nvs_commit(db) == ESP_OK;
}
static void select_record(void) {
  memo_record_t r;
  if (view.count && read_record(ids[view.selected], &r)) {
    snprintf(view.text, sizeof(view.text), "%s", r.text);
    view.seconds = r.seconds;
    view.selected_id = r.id;
    view.done = r.done;
    view.synced = r.synced;
    view.partial = r.partial;
    msg(r.synced ? "已同步到墨水屏" : "保存在本机");
  } else {
    view.text[0] = 0;
    view.selected_id = 0;
    msg("还没有备忘录");
  }
}
void memo_snapshot(memo_view_t *v) {
  take();
  *v = view;
  give();
}
void memo_message(const char *s) {
  take();
  msg(s);
  give();
}
void memo_network_status(bool online, const char *ip, memo_link_state_t state) {
  take();
  view.online = online;
  view.link = state;
  snprintf(view.ip, sizeof(view.ip), "%s", ip);
  changed();
  give();
}
void memo_network_clock_ready(void) {
  take();
  if (view.online && view.link != MEMO_LINK_READY) {
    view.link = MEMO_LINK_READY;
    changed();
  }
  give();
}
void memo_portal_state(bool open, const char *key) {
  take();
  view.portal = open;
  snprintf(view.setup_key, sizeof(view.setup_key), "%s", key ? key : "");
  changed();
  give();
}
bool memo_config_get(memo_config_t *out) {
  take();
  *out = config;
  give();
  return true;
}
static bool config_valid(const memo_config_t *c) {
#define TERMINATED(field) memchr(c->field, 0, sizeof(c->field))
  if (!TERMINATED(ssid) || !TERMINATED(password) || !TERMINATED(api_key) ||
      !TERMINATED(app_id) || !TERMINATED(access_key) || !TERMINATED(resource) ||
      !TERMINATED(ee04_host) || !TERMINATED(pair_key))
    return false;
#undef TERMINATED
  if (!memchr(c->ssid, 0, sizeof(c->ssid)) || !c->ssid[0] || strlen(c->password) > 63 ||
      !memo_header_value(c->api_key, 256) || !memo_header_value(c->app_id, 64) ||
      !memo_header_value(c->access_key, 256) || !memo_header_value(c->resource, 64) ||
      !c->resource[0] || c->brightness < 10 || c->brightness > 100)
    return false;
  return true;
}
bool memo_config_save(const memo_config_t *c) {
  if (!config_valid(c))
    return false;
  take();
  if (memo_active(view.phase) || !storage_ok) {
    give();
    return false;
  }
  bool ok =
      nvs_set_blob(db, "config", c, sizeof(*c)) == ESP_OK && nvs_commit(db) == ESP_OK;
  if (ok) {
    config = *c;
    view.configured = configured();
    msg(view.portal ? "已保存，按上键退出设置并联网" : "配置已保存，正在连接");
  }
  give();
  if (ok) {
    memo_network_reconfigure(c);
    bsp_display_backlight(c->brightness);
  }
  return ok;
}
bool memo_records_get(memo_record_t out[MEMO_HISTORY], size_t *count) {
  take();
  *count = 0;
  if (memo_active(view.phase)) {
    give();
    return false;
  }
  for (int i = 0; i < view.count; i++)
    if (read_record(ids[i], &out[*count]))
      (*count)++;
  give();
  return true;
}
bool memo_record_edit(uint32_t id, const char *text, bool done, bool remove) {
  if (!text || !text[0] || !memo_text_valid(text, NULL))
    return false;
  take();
  memo_record_t r;
  if (memo_active(view.phase) || !read_record(id, &r)) {
    give();
    return false;
  }
  bool ok;
  if (remove) {
    char key[12];
    record_key(key, id);
    ok = nvs_erase_key(db, key) == ESP_OK && nvs_commit(db) == ESP_OK;
    if (ok) {
      for (int i = 0; i < view.count; i++)
        if (ids[i] == id) {
          memmove(ids + i, ids + i + 1, (view.count - i - 1) * sizeof(*ids));
          view.count--;
          break;
        }
    }
  } else {
    snprintf(r.text, sizeof(r.text), "%s", text);
    r.done = done;
    r.synced = 0;
    ok = write_record(&r);
  }
  if (ok) {
    view.selected = 0;
    if (view.phase == MEMO_HISTORY_PAGE)
      select_record();
    else
      changed();
  }
  give();
  return ok;
}
void memo_asr_update(memo_phase_t p, const char *text, const char *message,
                     unsigned seconds, int level, bool partial) {
  take();
  view.phase = p;
  if (text)
    snprintf(view.text, sizeof(view.text), "%s", text);
  if (message)
    snprintf(view.message, sizeof(view.message), "%s", message);
  view.seconds = seconds;
  view.level = level;
  if (text)
    view.partial = partial;
  changed();
  give();
}
void memo_key(bsp_btn_t key, bsp_btn_ev_t ev, void *user) {
  (void)user;
  if (ev != BSP_BTN_CLICK && ev != BSP_BTN_LONG)
    return;
  if (memo_active(atomic_load(&phase))) {
    if (key == BSP_BTN_OK)
      memo_asr_stop(ev == BSP_BTN_LONG);
    return;
  }
  int command = (int)key + (ev == BSP_BTN_LONG ? 10 : 0);
  xQueueSend(keys, &command, 0);
}
static void sound(void) {
  take();
  bool enabled = config.sounds && view.audio_ok;
  give();
  if (!enabled)
    return;
  int16_t pcm[640];
  for (int i = 0; i < 640; i++) {
    int envelope = (i < 80 ? i : 640 - i);
    if (envelope > 80)
      envelope = 80;
    pcm[i] = ((i / 8) % 2 ? 1 : -1) * envelope * 15;
  }
  bsp_audio_set_volume(35);
  bsp_audio_write(pcm, sizeof(pcm));
}
static void save_draft(void) {
  take();
  if (storage_ok && view.text[0]) {
    nvs_set_str(db, "draft", view.text);
    nvs_commit(db);
  }
  give();
}
static void save_current(void) {
  take();
  if (!view.text[0]) {
    msg("没有可保存的文字");
    give();
    return;
  }
  if (view.count >= MEMO_HISTORY) {
    msg("已存满32条，请在管理页删除旧记录");
    give();
    return;
  }
  memo_record_t r = {.version = 1,
                     .seconds = view.seconds,
                     .created = time(NULL),
                     .partial = view.partial};
  if (r.created < 1704067200)
    r.created = 0;
  // Find an unoccupied slot; deleting an old record may leave holes in the
  // ring.
  for (unsigned attempt = 0; attempt < MEMO_HISTORY; attempt++) {
    r.id = next_id++;
    bool used = false;
    for (int i = 0; i < view.count; i++)
      if (ids[i] % MEMO_HISTORY == r.id % MEMO_HISTORY)
        used = true;
    if (!used)
      break;
  }
  snprintf(r.text, sizeof(r.text), "%s", view.text);
  if (!write_record(&r)) {
    msg("保存失败，草稿仍保留");
    give();
    return;
  }
  memmove(ids + 1, ids, view.count * sizeof(*ids));
  ids[0] = r.id;
  view.count++;
  nvs_erase_key(db, "draft");
  nvs_commit(db);
  view.phase = MEMO_HISTORY_PAGE;
  view.selected = 0;
  select_record();
  memo_config_t c = config;
  give();
  sound();
  if (c.ee04_host[0] && c.pair_key[0]) {
    bool ok = memo_sync_record(&r, &c);
    take();
    if (ok) {
      r.synced = 1;
      write_record(&r);
      view.synced = true;
      msg("已保存，并同步到墨水屏");
    } else
      msg("已保存，墨水屏未连接；长按下键重试");
    give();
  }
}
static void process(int k) {
  take();
  last_activity = (uint32_t)(esp_timer_get_time() / 1000);
  bsp_display_backlight(config.brightness);
  memo_phase_t p = view.phase;
  if (k == 12) {
    view.phase = MEMO_SETTINGS;
    msg("连接屏幕热点，打开 192.168.4.1");
    give();
    memo_portal_start();
    return;
  }
  if (p == MEMO_HOME) {
    if (k == 10) {
      size_t len = sizeof(view.text);
      if (storage_ok && nvs_get_str(db, "draft", view.text, &len) == ESP_OK &&
          view.text[0] && memo_text_valid(view.text, NULL)) {
        view.phase = MEMO_REVIEW;
        view.seconds = 0;
        view.partial = true;
        msg("草稿已恢复，确定保存");
      } else {
        view.text[0] = 0;
        msg("没有未保存的草稿");
      }
    }
    if (k == BSP_BTN_OK) {
      if (!configured()) {
        view.phase = MEMO_SETTINGS;
        msg("先配置 Wi-Fi 和火山 ASR");
        give();
        memo_portal_start();
        return;
      }
      if (!storage_ok) {
        msg("存储未就绪，请检查分区表");
        give();
        return;
      }
      if (!view.audio_ok) {
        msg("麦克风初始化失败，请重启检查");
        give();
        return;
      }
      if (!memo_network_ready()) {
        msg(memo_link_message(view.link));
        give();
        return;
      }
      memo_config_t c = config;
      view.text[0] = 0;
      view.seconds = 0;
      view.partial = false;
      view.phase = MEMO_CONNECTING;
      changed();
      give();
      memo_portal_stop();
      sound();
      memo_asr_run(&c);
      save_draft();
      return;
    }
    if (k == BSP_BTN_DOWN || k == BSP_BTN_UP) {
      view.phase = MEMO_HISTORY_PAGE;
      view.selected = 0;
      select_record();
    }
  } else if (p == MEMO_REVIEW || p == MEMO_ERROR) {
    if (k == BSP_BTN_OK && view.text[0]) {
      give();
      save_current();
      return;
    }
    if (k == 10) {
      view.phase = MEMO_HOME;
      msg("草稿保留；新录音将替换草稿");
    } else if (k == BSP_BTN_UP || k == BSP_BTN_DOWN) {
      give();
      memo_ui_scroll(k == BSP_BTN_UP ? -1 : 1);
      return;
    }
  } else if (p == MEMO_HISTORY_PAGE) {
    if (k == 10) {
      view.phase = MEMO_HOME;
      view.text[0] = 0;
      msg("按确定，说下你的灵感");
    } else if (k == BSP_BTN_UP || k == BSP_BTN_DOWN) {
      if (view.count) {
        view.selected =
            (view.selected + (k == BSP_BTN_UP ? -1 : 1) + view.count) % view.count;
        select_record();
      }
    } else if (k == BSP_BTN_OK && view.count) {
      memo_record_t r;
      if (read_record(ids[view.selected], &r)) {
        r.done = !r.done;
        r.synced = 0;
        if (write_record(&r))
          select_record();
      }
    } else if (k == 11 && view.count) {
      memo_record_t r;
      memo_config_t c = config;
      bool ok = read_record(ids[view.selected], &r);
      give();
      if (ok && memo_sync_record(&r, &c)) {
        take();
        r.synced = 1;
        write_record(&r);
        select_record();
        give();
      } else
        memo_message("同步失败，请检查墨水屏地址和配对码");
      return;
    }
  } else if (p == MEMO_SETTINGS && (k == BSP_BTN_UP || k == BSP_BTN_OK)) {
    view.phase = MEMO_HOME;
    msg("按确定，说下你的灵感");
    give();
    memo_portal_stop();
    return;
  }
  changed();
  give();
}
static void worker(void *arg) {
  (void)arg;
  uint32_t battery_at = 0;
  for (;;) {
    int k;
    if (xQueueReceive(keys, &k, pdMS_TO_TICKS(250)))
      process(k);
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    if (have_battery && now - battery_at > 30000) {
      int soc = bsp_battery_soc();
      take();
      view.battery = soc;
      changed();
      give();
      battery_at = now;
    }
    take();
    if (!memo_active(view.phase) && !view.portal && now - last_activity > 60000)
      bsp_display_backlight(10);
    give();
  }
}
void memo_app_start(bool audio_ok, bool battery_ok) {
  lock = xSemaphoreCreateMutex();
  keys = xQueueCreate(12, sizeof(int));
  if (!lock || !keys)
    abort();
  have_battery = battery_ok;
  view.battery = battery_ok ? bsp_battery_soc() : -1;
  view.audio_ok = audio_ok;
  view.phase = MEMO_HOME;
  config.sounds = true;
  config.brightness = 75;
  snprintf(config.resource, sizeof(config.resource), "volc.bigasr.sauc.duration");
  esp_err_t e = nvs_flash_init(); // Never erase existing identity or
                                  // provisioning on failure.
  if (e == ESP_OK)
    e = nvs_flash_init_partition("memo");
  if (e == ESP_OK)
    e = nvs_open_from_partition("memo", "voice", NVS_READWRITE, &db);
  storage_ok = e == ESP_OK;
  if (storage_ok) {
    size_t len = sizeof(config);
    memo_config_t saved;
    if (nvs_get_blob(db, "config", &saved, &len) == ESP_OK && len == sizeof(saved) &&
        config_valid(&saved)) {
      config = saved;
    }
    for (unsigned i = 0; i < MEMO_HISTORY; i++) {
      char key[12];
      snprintf(key, sizeof(key), "n%02u", i);
      memo_record_t r;
      len = sizeof(r);
      if (nvs_get_blob(db, key, &r, &len) == ESP_OK && len == sizeof(r) &&
          r.version == 1 && r.id && r.id % MEMO_HISTORY == i &&
          memchr(r.text, 0, sizeof(r.text)) && memo_text_valid(r.text, NULL)) {
        ids[view.count++] = r.id;
        if (r.id >= next_id)
          next_id = r.id + 1;
      }
    }
    for (int i = 0; i < view.count; i++)
      for (int j = i + 1; j < view.count; j++)
        if (ids[j] > ids[i]) {
          uint32_t t = ids[j];
          ids[j] = ids[i];
          ids[i] = t;
        }
    len = sizeof(view.text);
    if (nvs_get_str(db, "draft", view.text, &len) == ESP_OK && view.text[0] &&
        memo_text_valid(view.text, NULL)) {
      view.phase = MEMO_REVIEW;
      view.partial = true;
      msg("上次草稿已恢复，请确认后保存");
    } else {
      view.text[0] = 0;
      msg("按确定，说下你的灵感");
    }
  } else
    msg("存储未就绪，请检查分区表");
  view.configured = configured();
  bsp_display_backlight(config.brightness);
  if (memo_network_start(&config) != ESP_OK)
    msg("网络初始化失败，请重启检查");
  if (audio_ok && bsp_audio_set_format(16000, 16, 1) != ESP_OK) {
    view.audio_ok = false;
    msg("音频格式初始化失败，请重启检查");
  }
  memo_ui_start();
  if (!configured()) {
    take();
    view.phase = MEMO_SETTINGS;
    changed();
    give();
    memo_portal_start();
  }
  if (xTaskCreate(worker, "memo_worker", 8192, NULL, 4, NULL) != pdPASS)
    abort();
  bsp_button_init(memo_key, NULL);
}
