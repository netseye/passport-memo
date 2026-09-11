**简体中文** · [English](attributions.md)

# 来源与许可证

| 内容 | 来源与许可 |
| --- | --- |
| Passport BSP、像素主题、构建测试基础 | [FoloToy/ai-passport](https://github.com/folotoy/ai-passport)，MIT；[LICENSE](../LICENSE) 保留原版权 |
| Passport Memo 应用及文档 | Netseye 和贡献者，MIT；单独标注的素材与程序除外 |
| EE04 配套程序 | Netseye 和贡献者，[GPL-3.0-only](../companion/ee04/LICENSE)，独立构建 Arduino 应用 |
| Memo Pixel 16 字体 | 衍生自 GNU Unifont 17.0.05；[SIL OFL 1.1](../assets/fonts/OFL-1.1.txt) 及[原始声明](../assets/fonts/UNIFONT-LICENSE.txt) |
| 文档截图 | 项目真实界面配合演示数据生成 |
| ESP-IDF、托管组件、Arduino 库 | 由构建工具获取，保留各上游许可证 |

仓库包含[固定版本 Unifont 字形源码](../assets/fonts/unifont-17.0.05.hex.gz)及[生成器](../tools/build_memo_font.py)，衍生字体命名为 Memo Pixel 16。生成的 LVGL 字体仍采用 OFL，不改为应用的 MIT。

ESP32-C3 内存配置参考了 [xiaozhi-esp32 C3 默认设置](https://github.com/78/xiaozhi-esp32/blob/184a688cd04564c15f035cee09c0f61889fc8e9d/sdkconfig.defaults.esp32c3)及[通用设置](https://github.com/78/xiaozhi-esp32/blob/184a688cd04564c15f035cee09c0f61889fc8e9d/sdkconfig.defaults)。Passport 保持自身 IDF 版本、分区布局及编码器内存预算。

EE04 使用 GxEPD2（GPL v3）、Adafruit GFX、Adafruit BusIO、ArduinoJson 和 U8g2_for_Adafruit_GFX。后者的 BSD 适配器许可保存在 `companion/ee04/licenses/`；文泉驿字形保留上游 GPL v2 字体嵌入例外，见[字体归属](https://github.com/olikraus/u8g2/wiki/fntgrpwqy)及[字形源码](https://github.com/olikraus/U8g2_for_Adafruit_GFX)。根目录 MIT 不会重新授权这些独立内容。
