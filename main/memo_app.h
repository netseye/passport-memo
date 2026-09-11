#pragma once
#include "bsp_button.h"
#include "esp_err.h"
#include "memo_core.h"
#include <stdbool.h>
typedef struct {
  char ssid[33], password[65], api_key[257], app_id[65], access_key[257];
  char resource[65], ee04_host[64], pair_key[65];
  bool sounds;
  uint8_t brightness;
} memo_config_t;
typedef struct {
  memo_phase_t phase;
  memo_link_state_t link;
  char text[MEMO_TEXT_BYTES], message[160], ip[48], setup_key[9];
  uint32_t revision, seconds, selected_id;
  unsigned playback_seconds, playback_total, playback_volume;
  int count, selected, battery, level;
  bool online, configured, portal, done, synced, partial, audio_ok, replay_available;
} memo_view_t;
void memo_app_start(bool audio_ok, bool battery_ok);
void memo_key(bsp_btn_t key, bsp_btn_ev_t event, void *user);
void memo_snapshot(memo_view_t *view);
void memo_ui_start(void);
void memo_ui_scroll(int direction);
/* Config/record access is serialized with the application worker. */
bool memo_config_get(memo_config_t *out);
bool memo_config_save(const memo_config_t *config);
bool memo_records_get(memo_record_t out[MEMO_HISTORY], size_t *count);
bool memo_record_edit(uint32_t id, const char *text, bool done, bool remove);
void memo_portal_start(void);
void memo_portal_stop(void);
void memo_portal_state(bool open, const char *key);
void memo_network_status(bool online, const char *ip, memo_link_state_t state);
void memo_network_clock_ready(void);
void memo_message(const char *text);
bool memo_sync_record(memo_record_t *record, const memo_config_t *config);
esp_err_t memo_network_start(const memo_config_t *config);
void memo_network_reconfigure(const memo_config_t *config);
bool memo_network_ready(void);
/* ASR lifecycle lives entirely on-device; callbacks never access LVGL. */
void memo_asr_run(const memo_config_t *config);
void memo_asr_stop(bool cancel);
void memo_asr_update(memo_phase_t phase, const char *text, const char *message,
                     unsigned seconds, int level, bool partial);
