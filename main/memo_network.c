#include "cJSON.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_sntp.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "mbedtls/md.h"
#include "memo_app.h"
#include "memo_replay.h"
#include <ctype.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static atomic_bool online;
static atomic_bool portal_active;
static atomic_bool station_dirty;
static bool wifi_started;
static httpd_handle_t server;
static SemaphoreHandle_t portal_lock;
static char portal_key[9];
static int64_t portal_opened;
extern const char html_start[] asm("_binary_memo_portal_html_start");
extern const char html_end[] asm("_binary_memo_portal_html_end");
static bool equal(const char *a, const char *b) {
  size_t n = strlen(a);
  if (n != strlen(b))
    return false;
  unsigned v = 0;
  for (size_t i = 0; i < n; i++)
    v |= (unsigned char)a[i] ^ (unsigned char)b[i];
  return v == 0;
}
static bool authorized(httpd_req_t *r) {
  char key[40];
  if (httpd_req_get_hdr_value_str(r, "X-Memo-Key", key, sizeof(key)) != ESP_OK ||
      !portal_key[0] || !equal(key, portal_key)) {
    httpd_resp_send_err(r, HTTPD_401_UNAUTHORIZED, "请输入屏幕上的管理密码");
    return false;
  }
  httpd_resp_set_hdr(r, "Cache-Control", "no-store");
  return true;
}
static esp_err_t json_response(httpd_req_t *r, cJSON *j) {
  char *text = cJSON_PrintUnformatted(j);
  cJSON_Delete(j);
  if (!text)
    return httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "内存不足");
  httpd_resp_set_type(r, "application/json; charset=utf-8");
  esp_err_t e = httpd_resp_send(r, text, HTTPD_RESP_USE_STRLEN);
  free(text);
  return e;
}
static cJSON *body(httpd_req_t *r) {
  if (r->content_len <= 0 || r->content_len > 4096) {
    httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "请求过大");
    return NULL;
  }
  char *buf = calloc(1, r->content_len + 1);
  if (!buf) {
    httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "内存不足");
    return NULL;
  }
  int got = 0;
  while (got < r->content_len) {
    int n = httpd_req_recv(r, buf + got, r->content_len - got);
    if (n <= 0) {
      free(buf);
      httpd_resp_send_err(r, HTTPD_408_REQ_TIMEOUT, "请求超时");
      return NULL;
    }
    got += n;
  }
  cJSON *j = cJSON_ParseWithLength(buf, got);
  memset(buf, 0, got);
  free(buf);
  if (!j)
    httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "无效JSON");
  return j;
}
static esp_err_t index_page(httpd_req_t *r) {
  httpd_resp_set_type(r, "text/html; charset=utf-8");
  httpd_resp_set_hdr(r, "Cache-Control", "no-store");
  httpd_resp_set_hdr(r, "Content-Security-Policy",
                     "default-src 'self'; script-src 'unsafe-inline'; style-src "
                     "'unsafe-inline'; connect-src 'self'; media-src blob:; frame-ancestors 'none'");
  return httpd_resp_send(r, html_start, html_end - html_start - 1);
}
static esp_err_t state_page(httpd_req_t *r) {
  if (!authorized(r))
    return ESP_OK;
  memo_config_t c;
  memo_config_get(&c);
  memo_view_t v;
  memo_snapshot(&v);
  cJSON *j = cJSON_CreateObject();
  cJSON_AddStringToObject(j, "ssid", c.ssid);
  cJSON_AddStringToObject(j, "resource", c.resource);
  cJSON_AddStringToObject(j, "ee04_host", c.ee04_host);
  cJSON_AddStringToObject(j, "app_id", c.app_id);
  cJSON_AddBoolToObject(j, "has_api_key", c.api_key[0]);
  cJSON_AddBoolToObject(j, "has_access_key", c.access_key[0]);
  cJSON_AddBoolToObject(j, "has_pair_key", c.pair_key[0]);
  cJSON_AddBoolToObject(j, "sounds", c.sounds);
  cJSON_AddNumberToObject(j, "brightness", c.brightness);
  cJSON_AddBoolToObject(j, "online", v.online);
  cJSON_AddBoolToObject(j, "portal", v.portal);
  cJSON_AddBoolToObject(j, "configured", v.configured);
  cJSON_AddStringToObject(j, "connection_message", memo_link_message(v.link));
  cJSON_AddStringToObject(j, "ip", v.ip);
  cJSON_AddStringToObject(j, "message", v.message);
  cJSON_AddBoolToObject(j, "clock_ready", time(NULL) > 1704067200);
  memset(&c, 0, sizeof(c));
  return json_response(r, j);
}
static bool field(cJSON *j, const char *key, char *dest, size_t cap,
                  bool preserve_empty) {
  cJSON *v = cJSON_GetObjectItem(j, key);
  if (!v)
    return true;
  if (!cJSON_IsString(v) || strlen(v->valuestring) >= cap)
    return false;
  if (v->valuestring[0] || !preserve_empty)
    snprintf(dest, cap, "%s", v->valuestring);
  return true;
}
static bool host_ok(const char *s) {
  for (; *s; s++)
    if (!isalnum((unsigned char)*s) && *s != '.' && *s != '-')
      return false;
  return true;
}
static bool key_ok(const char *s) {
  if (!s[0])
    return true;
  if (strlen(s) != 32)
    return false;
  for (; *s; s++)
    if (!isxdigit((unsigned char)*s))
      return false;
  return true;
}
static esp_err_t configure_page(httpd_req_t *r) {
  if (!authorized(r))
    return ESP_OK;
  cJSON *j = body(r);
  if (!j)
    return ESP_OK;
  memo_config_t c;
  memo_config_get(&c);
  bool ok = field(j, "ssid", c.ssid, sizeof(c.ssid), false) &&
            field(j, "password", c.password, sizeof(c.password), true) &&
            field(j, "api_key", c.api_key, sizeof(c.api_key), true) &&
            field(j, "app_id", c.app_id, sizeof(c.app_id), false) &&
            field(j, "access_key", c.access_key, sizeof(c.access_key), true) &&
            field(j, "resource", c.resource, sizeof(c.resource), false) &&
            field(j, "ee04_host", c.ee04_host, sizeof(c.ee04_host), false) &&
            field(j, "pair_key", c.pair_key, sizeof(c.pair_key), true);
  cJSON *v = cJSON_GetObjectItem(j, "sounds");
  if (cJSON_IsBool(v))
    c.sounds = cJSON_IsTrue(v);
  v = cJSON_GetObjectItem(j, "brightness");
  if (cJSON_IsNumber(v)) {
    if (v->valueint < 10 || v->valueint > 100)
      ok = false;
    else
      c.brightness = v->valueint;
  }
  v = cJSON_GetObjectItem(j, "clear_credentials");
  if (cJSON_IsTrue(v)) {
    c.api_key[0] = c.app_id[0] = c.access_key[0] = 0;
  }
  v = cJSON_GetObjectItem(j, "open_wifi");
  if (cJSON_IsTrue(v))
    c.password[0] = 0;
  ok = ok && host_ok(c.ee04_host) && key_ok(c.pair_key) && memo_config_save(&c);
  memset(&c, 0, sizeof(c));
  cJSON_Delete(j);
  if (!ok)
    return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST,
                               "配置无效、设备忙碌或存储失败");
  return httpd_resp_sendstr(r, "{\"saved\":true}");
}
static esp_err_t notes_page(httpd_req_t *r) {
  if (!authorized(r))
    return ESP_OK;
  memo_record_t *records = calloc(MEMO_HISTORY, sizeof(*records));
  size_t count = 0;
  if (!records || !memo_records_get(records, &count)) {
    free(records);
    return httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "设备忙碌");
  }
  cJSON *j = cJSON_CreateArray();
  for (size_t i = 0; i < count; i++) {
    cJSON *n = cJSON_CreateObject();
    cJSON_AddNumberToObject(n, "id", records[i].id);
    cJSON_AddStringToObject(n, "text", records[i].text);
    cJSON_AddNumberToObject(n, "created", records[i].created);
    cJSON_AddNumberToObject(n, "seconds", records[i].seconds);
    cJSON_AddBoolToObject(n, "done", records[i].done);
    cJSON_AddBoolToObject(n, "synced", records[i].synced);
    cJSON_AddBoolToObject(n, "partial", records[i].partial);
    unsigned duration_ms = 0;
    cJSON_AddBoolToObject(n, "audio_available",
                          memo_replay_saved_info(records[i].id, &duration_ms));
    cJSON_AddNumberToObject(n, "audio_duration_ms", duration_ms);
    cJSON_AddItemToArray(j, n);
  }
  free(records);
  return json_response(r, j);
}
typedef struct {
  httpd_req_t *request;
  int64_t started;
  bool sent;
  size_t bytes;
} audio_response_t;
static bool audio_chunk(void *ctx, const void *data, size_t bytes) {
  audio_response_t *response = ctx;
  /* Bound setup exit latency even for a client reading very slowly. */
  if (esp_timer_get_time() - response->started > 15000000) return false;
  response->sent = true;
  if (httpd_resp_send_chunk(response->request, data, bytes) != ESP_OK) return false;
  response->bytes += bytes;
  return true;
}
static esp_err_t audio_page(httpd_req_t *r) {
  if (!authorized(r)) return ESP_OK;
  char query[32], value[11];
  uint64_t id = 0;
  if (httpd_req_get_url_query_str(r, query, sizeof(query)) != ESP_OK ||
      httpd_query_key_value(query, "id", value, sizeof(value)) != ESP_OK || !value[0])
    return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "无效记录编号");
  for (const char *p = value; *p; p++) {
    if (*p < '0' || *p > '9')
      return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "无效记录编号");
    id = id * 10 + (*p - '0');
  }
  if (!id || id > UINT32_MAX)
    return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "无效记录编号");
  memo_view_t view;
  memo_snapshot(&view);
  if (view.phase != MEMO_SETTINGS) {
    httpd_resp_set_status(r, "503 Service Unavailable");
    return httpd_resp_sendstr(r, "请保持设备在设置页");
  }
  if (!memo_replay_saved_info((uint32_t)id, NULL))
    return httpd_resp_send_err(r, HTTPD_404_NOT_FOUND, "原声已被替换或未保存");
  httpd_resp_set_type(r, "audio/ogg");
  httpd_resp_set_hdr(r, "X-Content-Type-Options", "nosniff");
  audio_response_t response = {.request = r, .started = esp_timer_get_time()};
  memo_export_result_t result = memo_replay_stream((uint32_t)id, audio_chunk, &response);
  ESP_LOGI("memo_network", "Audio preview result=%d bytes=%u elapsed_ms=%lld stack_free=%u",
           result, (unsigned)response.bytes, (esp_timer_get_time() - response.started) / 1000,
           (unsigned)uxTaskGetStackHighWaterMark(NULL));
  if (result == MEMO_EXPORT_OK) return httpd_resp_send_chunk(r, NULL, 0);
  /* An incomplete chunked response must close, never look like a valid clip. */
  if (response.sent) return ESP_FAIL;
  return httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "原声读取失败，文字仍保留");
}
static esp_err_t edit_page(httpd_req_t *r) {
  if (!authorized(r))
    return ESP_OK;
  cJSON *j = body(r);
  if (!j)
    return ESP_OK;
  cJSON *id = cJSON_GetObjectItem(j, "id"), *text = cJSON_GetObjectItem(j, "text");
  bool ok = cJSON_IsNumber(id) && id->valuedouble > 0 &&
            id->valuedouble <= UINT32_MAX && cJSON_IsString(text) &&
            memo_record_edit((uint32_t)id->valuedouble, text->valuestring,
                             cJSON_IsTrue(cJSON_GetObjectItem(j, "done")),
                             cJSON_IsTrue(cJSON_GetObjectItem(j, "remove")));
  cJSON_Delete(j);
  if (!ok)
    return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "保存失败，请检查240字限制");
  return httpd_resp_sendstr(r, "{\"saved\":true}");
}
// All radio changes and reconnect attempts hold portal_lock. HTTP handlers only
// mark the station configuration dirty, so saving cannot switch channels or
// deadlock against httpd_stop(), which waits for active handlers to finish.
static esp_err_t start_station(const memo_config_t *c) {
  atomic_store(&online, false);
  esp_err_t e = esp_wifi_stop();
  if (e != ESP_OK && e != ESP_ERR_WIFI_NOT_STARTED)
    return e;
  wifi_config_t station = {0};
  memcpy(station.sta.ssid, c->ssid, strlen(c->ssid));
  memcpy(station.sta.password, c->password, strlen(c->password));
  station.sta.pmf_cfg.capable = true;
  if ((e = esp_wifi_set_mode(WIFI_MODE_STA)) != ESP_OK ||
      (e = esp_wifi_set_config(WIFI_IF_STA, &station)) != ESP_OK)
    return e;
  atomic_store(&portal_active, false);
  memo_network_status(false, "",
                      c->ssid[0] ? MEMO_LINK_CONNECTING : MEMO_LINK_UNCONFIGURED);
  if ((e = esp_wifi_start()) == ESP_OK && c->ssid[0])
    e = esp_wifi_connect();
  if (e != ESP_OK) {
    ESP_LOGE("memo_network", "Station start failed: %s", esp_err_to_name(e));
    memo_network_status(false, "", MEMO_LINK_FAILED);
  }
  return e;
}
static void resume_station(void) {
  memo_config_t c;
  atomic_store(&station_dirty, false);
  memo_config_get(&c);
  esp_err_t e = start_station(&c);
  memset(&c, 0, sizeof(c));
  if (e != ESP_OK) {
    atomic_store(&portal_active, false);
    atomic_store(&station_dirty, true);
    memo_network_status(false, "", MEMO_LINK_FAILED);
  }
}
void memo_portal_start(void) {
  if (!wifi_started || !portal_lock) {
    memo_message("网络未就绪，请重启检查");
    return;
  }
  xSemaphoreTake(portal_lock, portMAX_DELAY);
  if (server) {
    portal_opened = esp_timer_get_time();
    xSemaphoreGive(portal_lock);
    return;
  }
  // Eight decimal digits meet the WPA2 minimum. Keep a code for this boot so
  // reopening settings does not invalidate the phone's saved hotspot password.
  if (!portal_key[0]) {
    uint32_t random;
    do {
      random = esp_random();
    } while (random >= UINT32_C(4200000000));
    snprintf(portal_key, sizeof(portal_key), "%08lu",
             (unsigned long)(random % UINT32_C(100000000)));
  }
  wifi_config_t ap = {0};
  snprintf((char *)ap.ap.ssid, sizeof(ap.ap.ssid), "Passport-Memo");
  snprintf((char *)ap.ap.password, sizeof(ap.ap.password), "%s", portal_key);
  ap.ap.ssid_len = strlen((char *)ap.ap.ssid);
  ap.ap.channel = 6;
  ap.ap.authmode = WIFI_AUTH_WPA2_PSK;
  ap.ap.pairwise_cipher = WIFI_CIPHER_TYPE_CCMP;
  ap.ap.max_connection = 2;
  ap.ap.beacon_interval = 100;
  ap.ap.dtim_period = 2;
  ap.ap.pmf_cfg.required = false;
  atomic_store(&portal_active, true);
  atomic_store(&online, false);
  memo_network_status(false, "", MEMO_LINK_PROVISIONING);
  // APSTA follows the station's channel during router connection attempts.
  // Stop STA entirely while provisioning so the phone gets a stable AP.
  esp_err_t e = esp_wifi_stop();
  if (e != ESP_OK || (e = esp_wifi_set_mode(WIFI_MODE_AP)) != ESP_OK ||
      (e = esp_wifi_set_config(WIFI_IF_AP, &ap)) != ESP_OK ||
      (e = esp_wifi_start()) != ESP_OK)
    goto failed;
  httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
  cfg.stack_size = 8192;
  cfg.max_open_sockets = 3;
  cfg.lru_purge_enable = true;
  cfg.recv_wait_timeout = 5;
  cfg.send_wait_timeout = 5;
  if ((e = httpd_start(&server, &cfg)) != ESP_OK)
    goto failed;
  const httpd_uri_t routes[] = {
      {.uri = "/", .method = HTTP_GET, .handler = index_page},
      {.uri = "/api/state", .method = HTTP_GET, .handler = state_page},
      {.uri = "/api/config", .method = HTTP_POST, .handler = configure_page},
      {.uri = "/api/notes", .method = HTTP_GET, .handler = notes_page},
      {.uri = "/api/audio", .method = HTTP_GET, .handler = audio_page},
      {.uri = "/api/note", .method = HTTP_POST, .handler = edit_page},
  };
  for (unsigned i = 0; i < sizeof(routes) / sizeof(*routes); i++)
    if ((e = httpd_register_uri_handler(server, &routes[i])) != ESP_OK)
      goto failed;
  portal_opened = esp_timer_get_time();
  memo_portal_state(true, portal_key);
  ESP_LOGI("memo_network", "Setup AP ready: channel=6, WPA2/CCMP, STA paused");
  xSemaphoreGive(portal_lock);
  return;
failed:
  ESP_LOGE("memo_network", "Setup AP failed: %s", esp_err_to_name(e));
  if (server) {
    httpd_stop(server);
    server = NULL;
  }
  memo_portal_state(false, "");
  resume_station();
  memo_message("热点启动失败，请重新进入设置");
  xSemaphoreGive(portal_lock);
}
void memo_portal_stop(void) {
  if (!portal_lock)
    return;
  xSemaphoreTake(portal_lock, portMAX_DELAY);
  if (server || atomic_load(&portal_active)) {
    if (server) {
      httpd_stop(server);
      server = NULL;
    }
    memo_portal_state(false, "");
    resume_station();
  }
  xSemaphoreGive(portal_lock);
}
static void network_event(void *arg, esp_event_base_t base, int32_t id, void *data) {
  (void)arg;
  if (base == WIFI_EVENT && id == WIFI_EVENT_AP_STACONNECTED)
    ESP_LOGI("memo_network", "AP client joined");
  if (base == WIFI_EVENT && id == WIFI_EVENT_AP_STADISCONNECTED) {
    wifi_event_ap_stadisconnected_t *event = data;
    ESP_LOGI("memo_network", "AP client left, reason=%u", event ? event->reason : 0);
  }
  if (base == IP_EVENT && id == IP_EVENT_AP_STAIPASSIGNED)
    ESP_LOGI("memo_network", "AP client DHCP address assigned");
  if (atomic_load(&portal_active))
    return;
  if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
    char ip[48];
    ip_event_got_ip_t *e = data;
    snprintf(ip, sizeof(ip), "http://" IPSTR, IP2STR(&e->ip_info.ip));
    atomic_store(&online, true);
    memo_network_status(true, ip,
                        time(NULL) > 1704067200 ? MEMO_LINK_READY : MEMO_LINK_SYNCING);
  }
  if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
    atomic_store(&online, false);
    wifi_event_sta_disconnected_t *event = data;
    unsigned reason = event ? event->reason : WIFI_REASON_UNSPECIFIED;
    memo_link_state_t state = MEMO_LINK_FAILED;
    switch (reason) {
    case WIFI_REASON_AUTH_EXPIRE:
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
      state = MEMO_LINK_AUTH_FAILED;
      break;
    case WIFI_REASON_NO_AP_FOUND:
    case WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY:
    case WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD:
      state = MEMO_LINK_NOT_FOUND;
      break;
    case WIFI_REASON_ASSOC_LEAVE:
      state = MEMO_LINK_CONNECTING;
      break;
    }
    ESP_LOGW("memo_network", "Wi-Fi disconnected, reason=%u", reason);
    memo_network_status(false, "", state);
  }
}
static void network_worker(void *arg) {
  (void)arg;
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(5000));
    xSemaphoreTake(portal_lock, portMAX_DELAY);
    if (!atomic_load(&portal_active)) {
      if (atomic_exchange(&station_dirty, false)) {
        resume_station();
      } else if (!atomic_load(&online)) {
        memo_config_t c;
        memo_config_get(&c);
        if (c.ssid[0])
          esp_wifi_connect();
        memset(&c, 0, sizeof(c));
      }
      if (memo_network_ready())
        memo_network_clock_ready();
    }
    bool expire = server && esp_timer_get_time() - portal_opened > 600000000;
    xSemaphoreGive(portal_lock);
    if (expire)
      memo_portal_stop();
  }
}
void memo_network_reconfigure(const memo_config_t *c) {
  (void)c;
  // Persistence has already completed. Defer all radio work to the worker or
  // portal exit; leave the AP and HTTP response intact during a browser save.
  atomic_store(&station_dirty, true);
}
esp_err_t memo_network_start(const memo_config_t *c) {
  portal_lock = xSemaphoreCreateMutex();
  if (!portal_lock)
    return ESP_ERR_NO_MEM;
  esp_err_t e = esp_netif_init();
  if (e != ESP_OK)
    return e;
  e = esp_event_loop_create_default();
  if (e != ESP_OK && e != ESP_ERR_INVALID_STATE)
    return e;
  if (!esp_netif_create_default_wifi_sta() || !esp_netif_create_default_wifi_ap())
    return ESP_ERR_NO_MEM;
  wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
  if ((e = esp_wifi_init(&init)) != ESP_OK ||
      (e = esp_wifi_set_storage(WIFI_STORAGE_RAM)) != ESP_OK ||
      (e = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, network_event,
                                      NULL)) != ESP_OK ||
      (e = esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, network_event,
                                      NULL)) != ESP_OK ||
      (e = start_station(c)) != ESP_OK)
    return e;
  wifi_started = true;
  esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
  esp_sntp_setservername(0, "time.cloudflare.com");
  esp_sntp_setservername(1, "pool.ntp.org");
  esp_sntp_init();
  return xTaskCreate(network_worker, "memo_network", 4096, NULL, 3, NULL) == pdPASS
             ? ESP_OK
             : ESP_ERR_NO_MEM;
}
bool memo_network_ready(void) {
  return atomic_load(&online) && time(NULL) > 1704067200;
}
static void hmac(const char *key, const char *data, char output[65]) {
  uint8_t sum[32];
  mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                  (const unsigned char *)key, strlen(key), (const unsigned char *)data,
                  strlen(data), sum);
  for (unsigned i = 0; i < 32; i++)
    sprintf(output + i * 2, "%02x", sum[i]);
  output[64] = 0;
}
typedef struct {
  char data[512];
  size_t used;
} response_t;
static esp_err_t on_response(esp_http_client_event_t *event) {
  response_t *r = event->user_data;
  if (event->event_id == HTTP_EVENT_ON_DATA) {
    if (event->data_len < 0 || (size_t)event->data_len >= sizeof(r->data) - r->used)
      return ESP_FAIL;
    memcpy(r->data + r->used, event->data, event->data_len);
    r->used += event->data_len;
    r->data[r->used] = 0;
  }
  return ESP_OK;
}
static bool request(const char *url, const char *payload, response_t *result) {
  memset(result, 0, sizeof(*result));
  esp_http_client_config_t cfg = {.url = url,
                                  .timeout_ms = 3500,
                                  .disable_auto_redirect = true,
                                  .event_handler = on_response,
                                  .user_data = result};
  esp_http_client_handle_t client = esp_http_client_init(&cfg);
  if (!client)
    return false;
  if (payload) {
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, payload, strlen(payload));
  }
  esp_err_t e = esp_http_client_perform(client);
  int status = esp_http_client_get_status_code(client);
  esp_http_client_cleanup(client);
  return e == ESP_OK && status == 200;
}
bool memo_sync_record(memo_record_t *record, const memo_config_t *c) {
  if (!memo_network_ready() || !c->ee04_host[0] || !c->pair_key[0] ||
      !host_ok(c->ee04_host))
    return false;
  response_t result;
  char url[160];
  snprintf(url, sizeof(url), "http://%s:8080/v1/challenge", c->ee04_host);
  if (!request(url, NULL, &result))
    return false;
  cJSON *j = cJSON_Parse(result.data);
  if (!j)
    return false;
  cJSON *v = cJSON_GetObjectItem(j, "nonce");
  char nonce[33];
  if (!cJSON_IsString(v) || strlen(v->valuestring) != 32) {
    cJSON_Delete(j);
    return false;
  }
  snprintf(nonce, sizeof(nonce), "%s", v->valuestring);
  cJSON_Delete(j);
  char canonical[MEMO_TEXT_BYTES + 128], signature[65];
  snprintf(canonical, sizeof(canonical), "memo-v1\n%s\n%lu\n%d\n%s", nonce,
           (unsigned long)record->id, record->done, record->text);
  hmac(c->pair_key, canonical, signature);
  j = cJSON_CreateObject();
  cJSON_AddStringToObject(j, "nonce", nonce);
  cJSON_AddNumberToObject(j, "id", record->id);
  cJSON_AddBoolToObject(j, "done", record->done);
  cJSON_AddStringToObject(j, "text", record->text);
  cJSON_AddStringToObject(j, "mac", signature);
  char *payload = cJSON_PrintUnformatted(j);
  cJSON_Delete(j);
  if (!payload)
    return false;
  snprintf(url, sizeof(url), "http://%s:8080/v1/memo", c->ee04_host);
  bool ok = request(url, payload, &result);
  free(payload);
  if (!ok)
    return false;
  j = cJSON_Parse(result.data);
  if (!j)
    return false;
  v = cJSON_GetObjectItem(j, "mac");
  snprintf(canonical, sizeof(canonical), "saved\n%s\n%lu", nonce,
           (unsigned long)record->id);
  hmac(c->pair_key, canonical, signature);
  ok = cJSON_IsString(v) && equal(v->valuestring, signature);
  cJSON_Delete(j);
  return ok;
}
