**简体中文** · [English](build.md)

# 构建、测试与刷机

## Passport 工具链

使用 **ESP-IDF 5.5.3**，目标为 **ESP32-C3 / 8 MB Flash / 无 PSRAM**。按[官方安装说明](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32c3/get-started/index.html)安装并激活 `export.sh`。托管组件版本由 `dependencies.lock` 固定。

```sh
git clone https://github.com/netseye/passport-memo.git
cd passport-memo
. /path/to/esp-idf/export.sh
idf.py --version
./tools/validate.sh
```

完整检查包括文档与仓库检查、actionlint、主机测试（含 EE04）、全新配置编译及合并镜像校验，不读取本地已有的 `sdkconfig` 修改，产物为 `build/Passport-Memo-full.bin`。主机检查需要 Python 3、C/C++ 编译器和 Node.js；缺少 actionlint 时脚本会下载并校验其 SHA-256。

需要保留构建目录用于刷机时：

```sh
idf.py -B build/device -D SDKCONFIG=build/device/sdkconfig build
```

从旧配置迁移时使用新的构建目录。已有 sdkconfig 可能覆盖 Opus 与 TLS 所需的内存设置。

## 保留设备身份信息

| 分区 | 偏移 | 大小 | 用途 |
| --- | --- | --- | --- |
| factory | `0x10000` | `0x300000` | 应用，最多 3 MB |
| cardid | `0x356000` | `0x4000` | 受保护的出厂身份，不得覆盖 |
| memo | `0x360000` | `0x30000` | 配置、备忘录与草稿 |
| replay | `0x390000` | `0x40000` | 最近一段 Opus 原声 |

安装前核实芯片与 8 MB Flash，私下备份完整 Flash，并检查原有 `cardid` 之后的数据布局是否允许增加 `memo` 和 `replay` 分区。已写入出厂身份的 Passport 不要执行 `erase-flash`。

```sh
# 替换为已核实的 Passport 串口。
export MEMO_PORT=/dev/cu.usbmodemYOUR_PASSPORT
mkdir -p backups
umask 077
python -m esptool --chip esp32c3 --port "$MEMO_PORT" flash_id
python -m esptool --chip esp32c3 --port "$MEMO_PORT" read_flash 0 0x800000 backups/passport-private-backup.bin
idf.py -B build/device -p "$MEMO_PORT" flash monitor
```

`idf.py flash` 分段写入构建文件，避开身份区域，已配置设备优先使用此方式。合并镜像是供兼容刷机工具使用的开发产物；只有在确认文件结束位置早于 `cardid` 且原有数据布局兼容后，才能从 `0x0` 写入。备份含身份信息，也可能含密钥，请勿上传。

## 额外主机验证

```sh
# 需要 libopus 和 FFmpeg，以主机编码器验证固件 Ogg 封装。
python3 tests/test_memo_ogg_decode.py

# 需要 CMake，以及固件编译获取的托管 LVGL。
cmake -S tests/ui_preview -B build/ui-preview
cmake --build build/ui-preview
build/ui-preview/memo_preview 2 build/recording.ppm
MEMO_PREVIEW_MAX_TEXT=1 build/ui-preview/memo_preview 4 build/review-max.ppm
MEMO_PREVIEW_CONFIRM=1 build/ui-preview/memo_preview 9 build/delete-confirm.ppm
MEMO_PREVIEW_CLOCK_ROLLOVER=1 build/ui-preview/memo_preview 0 build/clock-next.ppm
```

[预览数据与图片生成](../tests/ui_preview/README.zh_CN.md)不使用真实设备数据。固件、缓存和备份均被 Git 忽略。

CI 在推送及 Pull Request 上运行相同检查，上传合并开发镜像，不会刷机、打标签或自动发布 Release。[EE04 使用独立 Arduino 构建](../companion/ee04/README.zh_CN.md)，不能混刷两块板的固件。

升级原声回放版本时需同时更新分区表和应用；只刷应用会继续识别，但无法缓存或回放。不要擦除 `memo` 或 `cardid`。

## 本地网页预览

执行 `python3 tools/preview_portal.py`，打开 `http://127.0.0.1:8766`，输入公开演示码 `12345678`。只读预览提供虚构备忘录与 `tests/fixtures/demo-tone.ogg` 中的 440 Hz 合成音。可用 `python3 tests/test_memo_ogg_decode.py tests/fixtures/demo-tone.ogg` 重新生成（需要 libopus 与 FFmpeg）。

若已安装 replay 分区，网页预览更新只需升级应用；保留既有分区表、身份、备忘录及原声缓存。

主机检查还需 Node.js 20 或更新版本，用于运行网页控制器测试。

设备操作菜单与时钟更新沿用原有分区和 NVS 布局，已安装原声回放的设备只需更新应用。主机测试通过模拟 NVS 执行真实应用的按键逻辑，覆盖取消、提交失败及模拟重启；不接触硬件或用户记录。
