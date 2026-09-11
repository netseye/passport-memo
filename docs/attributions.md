[简体中文](attributions.zh_CN.md) · **English**

# Sources and licenses

| Material | Source / license |
| --- | --- |
| Passport BSP, pixel theme and build/test foundations | [FoloToy/ai-passport](https://github.com/folotoy/ai-passport), MIT; copyright retained in [LICENSE](../LICENSE) |
| Passport Memo application and documentation | Netseye and contributors, MIT, except separately marked assets/programs |
| EE04 companion | Netseye and contributors, [GPL-3.0-only](../companion/ee04/LICENSE); separately built Arduino application |
| Memo Pixel 16 font | Derived from GNU Unifont 17.0.05; [SIL OFL 1.1](../assets/fonts/OFL-1.1.txt), [original notices](../assets/fonts/UNIFONT-LICENSE.txt) |
| Documentation screenshots | Actual project UI with demonstration fixtures; generated for this project |
| ESP-IDF, managed components and Arduino libraries | Fetched by build tools, retaining each upstream license |

The [pinned Unifont glyph source](../assets/fonts/unifont-17.0.05.hex.gz) and [generator](../tools/build_memo_font.py) are included. The derived font uses the name Memo Pixel 16. The generated LVGL font stays under OFL, not the application's MIT license.

The ESP32-C3 memory profile was informed by [xiaozhi-esp32 C3 defaults](https://github.com/78/xiaozhi-esp32/blob/184a688cd04564c15f035cee09c0f61889fc8e9d/sdkconfig.defaults.esp32c3) and its [shared defaults](https://github.com/78/xiaozhi-esp32/blob/184a688cd04564c15f035cee09c0f61889fc8e9d/sdkconfig.defaults). Passport keeps its own IDF version, partition layout and codec memory budget.

The EE04 program uses GxEPD2 (GPL v3), Adafruit GFX, Adafruit BusIO, ArduinoJson and U8g2_for_Adafruit_GFX. The latter's BSD adapter license is included in `companion/ee04/licenses/`. Its WenQuanYi glyphs retain the upstream GPL v2 font-embedding exception; see [font attribution](https://github.com/olikraus/u8g2/wiki/fntgrpwqy) and [font source](https://github.com/olikraus/U8g2_for_Adafruit_GFX). The root MIT license does not relicense these independent materials.
