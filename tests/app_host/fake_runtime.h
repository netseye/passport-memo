#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM -2
#define ESP_ERR_INVALID_STATE -3
typedef enum { BSP_BTN_UP, BSP_BTN_DOWN, BSP_BTN_OK } bsp_btn_t;
typedef enum { BSP_BTN_PRESS, BSP_BTN_CLICK, BSP_BTN_DOUBLE, BSP_BTN_LONG } bsp_btn_ev_t;
typedef void (*bsp_btn_cb_t)(bsp_btn_t, bsp_btn_ev_t, void *);
typedef void *SemaphoreHandle_t;
typedef void *QueueHandle_t;
typedef unsigned nvs_handle_t;
#define NVS_READWRITE 1
#define portMAX_DELAY 0xffffffffu
#define pdMS_TO_TICKS(ms) (ms)
#define pdPASS 1
SemaphoreHandle_t xSemaphoreCreateMutex(void);
int xSemaphoreTake(SemaphoreHandle_t s, unsigned timeout);
int xSemaphoreGive(SemaphoreHandle_t s);
QueueHandle_t xQueueCreate(unsigned count, unsigned size);
int xQueueSend(QueueHandle_t q, const void *value, unsigned timeout);
int xQueueReceive(QueueHandle_t q, void *value, unsigned timeout);
int xTaskCreate(void (*fn)(void *), const char *name, unsigned stack, void *arg, unsigned priority, void *handle);
void vTaskDelay(unsigned ticks);
int64_t esp_timer_get_time(void);
esp_err_t nvs_flash_init(void);
esp_err_t nvs_flash_init_partition(const char *name);
esp_err_t nvs_open_from_partition(const char *partition, const char *name, int mode, nvs_handle_t *out);
esp_err_t nvs_get_blob(nvs_handle_t db, const char *key, void *out, size_t *size);
esp_err_t nvs_set_blob(nvs_handle_t db, const char *key, const void *data, size_t size);
esp_err_t nvs_get_str(nvs_handle_t db, const char *key, char *out, size_t *size);
esp_err_t nvs_set_str(nvs_handle_t db, const char *key, const char *value);
esp_err_t nvs_erase_key(nvs_handle_t db, const char *key);
esp_err_t nvs_commit(nvs_handle_t db);
void bsp_display_backlight(unsigned value);
bool bsp_lvgl_lock(unsigned timeout);
void bsp_lvgl_unlock(void);
int bsp_battery_soc(void);
esp_err_t bsp_audio_set_volume(int volume);
esp_err_t bsp_audio_write(void *data, size_t size);
esp_err_t bsp_audio_set_format(unsigned rate, unsigned bits, unsigned channels);
esp_err_t bsp_button_init(bsp_btn_cb_t cb, void *user);
