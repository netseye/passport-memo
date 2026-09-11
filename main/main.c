#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "esp_log.h"
#include "memo_app.h"
void app_main(void) {
  bsp_i2c_init();
  if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
    ESP_LOGE("memo", "Display initialization failed");
    return;
  }
  bool audio_ok = bsp_audio_init() == ESP_OK;
  bool battery_ok = bsp_battery_init() == ESP_OK;
  memo_app_start(audio_ok, battery_ok);
}
