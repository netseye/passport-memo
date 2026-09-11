idf_component_register(
    SRCS "main.c" "ui_pixel.c" "ui_pixel_math.c"
         "memo_app.c" "memo_core.c" "memo_asr.c" "memo_ogg.c"
         "memo_replay.c" "memo_replay_format.c" "memo_playback_pcm.c"
         "memo_network.c" "memo_ui.c" "memo_font_16.c"
    INCLUDE_DIRS "."
    REQUIRES bsp esp_event esp_hw_support esp_netif esp_timer esp_wifi nvs_flash
             esp_http_server esp_http_client esp-tls mbedtls json esp_websocket_client esp_audio_codec
    EMBED_TXTFILES "memo_portal.html"
)
