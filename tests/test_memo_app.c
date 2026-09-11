/* Run the actual worker/state/NVS code against in-memory device adapters.
 * Tests never open a serial port or read real credentials or recordings. */
#include <assert.h>
#include "../main/memo_app.c"

static memo_record_t saved[MEMO_HISTORY];
static char saved_draft[MEMO_TEXT_BYTES];
static unsigned mutex_depth, erasures, forgets, syncs, plays;
static uint32_t clip_id, forgotten_id, synced_id;
static bool fail_erase, fail_commit;
static int queued = -1;

SemaphoreHandle_t xSemaphoreCreateMutex(void) { return (void *)1; }
int xSemaphoreTake(SemaphoreHandle_t s, unsigned timeout) { (void)s; (void)timeout; assert(!mutex_depth++); return 1; }
int xSemaphoreGive(SemaphoreHandle_t s) { (void)s; assert(mutex_depth-- == 1); return 1; }
QueueHandle_t xQueueCreate(unsigned count, unsigned size) { (void)count; assert(size == sizeof(int)); return (void *)1; }
int xQueueSend(QueueHandle_t q, const void *value, unsigned timeout) { (void)q; (void)timeout; queued = *(const int *)value; return 1; }
int xQueueReceive(QueueHandle_t q, void *value, unsigned timeout) { (void)q; (void)value; (void)timeout; return 0; }
int xTaskCreate(void (*fn)(void *), const char *name, unsigned stack, void *arg, unsigned priority, void *handle) {
  (void)fn; (void)name; (void)stack; (void)arg; (void)priority; (void)handle; return pdPASS;
}
void vTaskDelay(unsigned ticks) { (void)ticks; }
int64_t esp_timer_get_time(void) { return 1000000; }
esp_err_t nvs_flash_init(void) { return ESP_OK; }
esp_err_t nvs_flash_init_partition(const char *name) { assert(!strcmp(name, "memo")); return ESP_OK; }
esp_err_t nvs_open_from_partition(const char *partition, const char *name, int mode, nvs_handle_t *out) {
  (void)partition; (void)name; (void)mode; *out = 1; return ESP_OK;
}
static int slot(const char *key) { return key[0] == 'n' ? atoi(key + 1) : -1; }
esp_err_t nvs_get_blob(nvs_handle_t handle, const char *key, void *out, size_t *size) {
  (void)handle;
  int i = slot(key);
  if (i < 0 || i >= MEMO_HISTORY || !saved[i].id || *size < sizeof(saved[i])) return ESP_FAIL;
  memcpy(out, &saved[i], sizeof(saved[i])); *size = sizeof(saved[i]); return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t handle, const char *key, const void *data, size_t size) {
  (void)handle; int i = slot(key);
  if (i < 0 || i >= MEMO_HISTORY || size != sizeof(saved[i])) return ESP_FAIL;
  memcpy(&saved[i], data, size); return ESP_OK;
}
esp_err_t nvs_get_str(nvs_handle_t handle, const char *key, char *out, size_t *size) {
  (void)handle; assert(!strcmp(key, "draft"));
  if (!saved_draft[0] || *size <= strlen(saved_draft)) return ESP_FAIL;
  strcpy(out, saved_draft); *size = strlen(saved_draft) + 1; return ESP_OK;
}
esp_err_t nvs_set_str(nvs_handle_t handle, const char *key, const char *value) {
  (void)handle; assert(!strcmp(key, "draft")); strcpy(saved_draft, value); return ESP_OK;
}
esp_err_t nvs_erase_key(nvs_handle_t handle, const char *key) {
  (void)handle;
  if (fail_erase) { fail_erase = false; return ESP_FAIL; }
  if (!strcmp(key, "draft")) { saved_draft[0] = 0; return ESP_OK; }
  int i = slot(key); assert(i >= 0 && i < MEMO_HISTORY);
  memset(&saved[i], 0, sizeof(saved[i])); erasures++; return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t handle) {
  (void)handle;
  if (fail_commit) { fail_commit = false; return ESP_FAIL; }
  return ESP_OK;
}
void bsp_display_backlight(unsigned value) { (void)value; }
bool bsp_lvgl_lock(unsigned timeout) { (void)timeout; return true; }
void bsp_lvgl_unlock(void) {}
int bsp_battery_soc(void) { return 78; }
esp_err_t bsp_audio_set_volume(int volume) { (void)volume; return ESP_OK; }
esp_err_t bsp_audio_write(void *data, size_t size) { (void)data; (void)size; return ESP_OK; }
esp_err_t bsp_audio_set_format(unsigned rate, unsigned bits, unsigned channels) { (void)rate; (void)bits; (void)channels; return ESP_OK; }
esp_err_t bsp_button_init(bsp_btn_cb_t cb, void *user) { (void)cb; (void)user; return ESP_OK; }
void memo_ui_start(void) {}
void memo_ui_scroll(int direction) { (void)direction; }
void memo_portal_start(void) {}
void memo_portal_stop(void) {}
void memo_network_reconfigure(const memo_config_t *c) { (void)c; }
esp_err_t memo_network_start(const memo_config_t *c) { (void)c; return ESP_OK; }
bool memo_network_ready(void) { return true; }
void memo_asr_run(const memo_config_t *c) { (void)c; }
void memo_asr_stop(bool cancel) { (void)cancel; }
bool memo_sync_record(memo_record_t *r, const memo_config_t *c) { (void)c; syncs++; synced_id = r->id; return true; }
void memo_replay_init(void) {}
bool memo_replay_matches(const char *text, uint32_t id) { (void)text; return id && id == clip_id; }
bool memo_replay_bind(uint32_t id) { clip_id = id; return true; }
void memo_replay_forget(uint32_t id) { forgets++; forgotten_id = id; if (clip_id == id) clip_id = 0; }
void memo_replay_stop(void) {}
void memo_replay_volume_step(int delta) { (void)delta; }
unsigned memo_replay_volume(void) { return 85; }
esp_err_t memo_replay_play(memo_replay_progress_t progress) { (void)progress; plays++; return ESP_OK; }

static void seed(unsigned count) {
  assert(!mutex_depth);
  memset(saved, 0, sizeof(saved)); memset(&view, 0, sizeof(view)); memset(&config, 0, sizeof(config));
  memset(ids, 0, sizeof(ids)); strcpy(saved_draft, "unsaved test draft");
  erasures = forgets = syncs = plays = 0; fail_erase = fail_commit = false;
  lock = (void *)1; keys = (void *)1; db = 1; storage_ok = true; view.audio_ok = true;
  config.brightness = 75; view.phase = MEMO_HISTORY_PAGE; view.count = count;
  for (unsigned i = 0; i < count; i++) {
    uint32_t id = 100 + count - i; ids[i] = id;
    saved[id % MEMO_HISTORY] = (memo_record_t){.version = 1, .id = id, .seconds = 10};
    snprintf(saved[id % MEMO_HISTORY].text, MEMO_TEXT_BYTES, "synthetic note %u", id);
  }
  clip_id = count ? ids[0] : 0;
  select_record();
}
static void key(bsp_btn_t button, bsp_btn_ev_t event) {
  queued = -1; memo_key(button, event, NULL);
  if (queued >= 0) process(queued);
  assert(!mutex_depth);
}
static void open_confirm(void) {
  key(BSP_BTN_DOWN, BSP_BTN_LONG); assert(view.phase == MEMO_RECORD_ACTIONS);
  key(BSP_BTN_OK, BSP_BTN_CLICK); assert(view.actions.confirm_delete && view.actions.selected == 0);
}
static void confirm_delete(void) {
  key(BSP_BTN_DOWN, BSP_BTN_CLICK); key(BSP_BTN_OK, BSP_BTN_CLICK);
}
int main(void) {
  seed(3); memo_record_t unrelated = saved[ids[2] % MEMO_HISTORY];
  open_confirm();
  key(BSP_BTN_OK, BSP_BTN_CLICK); // Default keeps the note.
  assert(view.phase == MEMO_HISTORY_PAGE && view.count == 3 && !erasures && !forgets);
  open_confirm(); key(BSP_BTN_UP, BSP_BTN_LONG);
  assert(view.phase == MEMO_HISTORY_PAGE && !erasures);
  open_confirm(); key(BSP_BTN_OK, BSP_BTN_DOUBLE);
  assert(view.phase == MEMO_RECORD_ACTIONS && !erasures && !plays);
  key(BSP_BTN_OK, BSP_BTN_LONG);
  assert(view.phase == MEMO_SETTINGS && !view.actions.record_id && !erasures);

  seed(3); key(BSP_BTN_DOWN, BSP_BTN_CLICK); uint32_t middle = view.selected_id;
  open_confirm(); assert(!memo_record_edit(middle, "edited", false, true));
  confirm_delete();
  assert(view.count == 2 && !saved[middle % MEMO_HISTORY].id && forgotten_id == middle);
  assert(view.selected_id == unrelated.id && clip_id == ids[0]);
  assert(!memcmp(&saved[unrelated.id % MEMO_HISTORY], &unrelated, sizeof(unrelated)));
  assert(!strcmp(saved_draft, "unsaved test draft"));
  key(BSP_BTN_DOWN, BSP_BTN_LONG); key(BSP_BTN_DOWN, BSP_BTN_CLICK); key(BSP_BTN_OK, BSP_BTN_CLICK);
  assert(syncs == 1 && synced_id == unrelated.id && view.synced);

  seed(1); open_confirm(); confirm_delete();
  assert(!view.count && !view.selected_id && !view.text[0] && !view.replay_available && !clip_id);
  assert(view.phase == MEMO_HISTORY_PAGE && !strcmp(saved_draft, "unsaved test draft"));
  key(BSP_BTN_DOWN, BSP_BTN_LONG); assert(view.phase == MEMO_HISTORY_PAGE);

  for (unsigned failure = 0; failure < 2; failure++) {
    seed(2); uint32_t id = view.selected_id;
    open_confirm(); fail_erase = failure == 0; fail_commit = failure == 1; confirm_delete();
    assert(view.count == 2 && saved[id % MEMO_HISTORY].id == id && clip_id == id && !forgets);
  }
  seed(2); uint32_t pinned = view.selected_id;
  open_confirm(); view.selected = 1; confirm_delete();
  assert(!saved[pinned % MEMO_HISTORY].id && view.count == 1);
  seed(2); open_confirm(); view.actions.record_id = 999; confirm_delete();
  assert(view.count == 2 && !erasures && !forgets);
  seed(2); open_confirm(); saved[view.selected_id % MEMO_HISTORY].id += MEMO_HISTORY;
  confirm_delete(); assert(view.count == 2 && !erasures && !forgets);
  seed(2); uint32_t id = view.selected_id; view.phase = MEMO_SETTINGS;
  assert(memo_record_edit(id, "synthetic", false, true));
  assert(view.count == 1 && !clip_id); // Web deletion shares the same path.
  memset(&view, 0, sizeof(view)); memset(&config, 0, sizeof(config)); memset(ids, 0, sizeof(ids));
  next_id = 1;
  memo_app_start(true, false); // Re-open the same fake NVS after a simulated boot.
  assert(view.count == 1 && view.text[0] && !strcmp(view.text, saved_draft));
  assert(ids[0] != id && !saved[id % MEMO_HISTORY].id);
  puts("Actual app: delete/cancel, defaults, stale IDs, last note, storage failures, sync and unrelated data PASS");
}
