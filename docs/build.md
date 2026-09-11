[简体中文](build.zh_CN.md) · **English**

# Build, test and flash

## Passport toolchain

Use **ESP-IDF 5.5.3** for **ESP32-C3 / 8 MB Flash / no PSRAM**. Install it using the [official ESP-IDF instructions](https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32c3/get-started/index.html), then activate its `export.sh`. The tracked `dependencies.lock` pins managed components.

```sh
git clone https://github.com/netseye/passport-memo.git
cd passport-memo
. /path/to/esp-idf/export.sh
idf.py --version
./tools/validate.sh
```

The gate runs repository checks, actionlint, host tests (including EE04), a fresh firmware build and merged-image verification. It ignores local `sdkconfig` changes and produces `build/Passport-Memo-full.bin`. Host checks need Python 3, a C/C++ compiler and Node.js. The script downloads a checksum-verified actionlint if it is absent.

For a persistent build directory used for flashing:

```sh
idf.py -B build/device -D SDKCONFIG=build/device/sdkconfig build
```

Start with a new build directory when moving from older defaults. Existing sdkconfig values can override the RAM settings needed for Opus and TLS.

## Preserve the device identity

| Partition | Offset | Size | Purpose |
| --- | --- | --- | --- |
| factory | `0x10000` | `0x300000` | Application, at most 3 MB |
| cardid | `0x356000` | `0x4000` | Protected factory identity; never overwrite |
| memo | `0x360000` | `0x30000` | Device settings, notes and draft |
| replay | `0x390000` | `0x40000` | Latest Opus recording; separate from NVS |

Confirm the target chip and 8 MB Flash before installation. Back up all Flash privately and check that existing data after `cardid` can accommodate the new `memo` and `replay` partitions. Never run `erase-flash` on a provisioned Passport.

```sh
# Replace this placeholder with the verified Passport port.
export MEMO_PORT=/dev/cu.usbmodemYOUR_PASSPORT
mkdir -p backups
umask 077
python -m esptool --chip esp32c3 --port "$MEMO_PORT" flash_id
python -m esptool --chip esp32c3 --port "$MEMO_PORT" read_flash 0 0x800000 backups/passport-private-backup.bin
idf.py -B build/device -p "$MEMO_PORT" flash monitor
```

`idf.py flash` writes segmented build images and avoids the protected identity area. Prefer it for provisioned devices. The merged image is a development artifact for compatible flashers: use offset `0x0` only after verifying the image ends before `cardid` and your existing data layout is compatible. Backups contain identity and possibly credentials; never upload them.

## Additional host verification

```sh
# Requires libopus and FFmpeg; validates the firmware Ogg muxer with a host encoder.
python3 tests/test_memo_ogg_decode.py

# Requires CMake and managed LVGL fetched by the firmware build.
cmake -S tests/ui_preview -B build/ui-preview
cmake --build build/ui-preview
build/ui-preview/memo_preview 2 build/recording.ppm
MEMO_PREVIEW_MAX_TEXT=1 build/ui-preview/memo_preview 4 build/review-max.ppm
```

[Preview fixtures and image generation](../tests/ui_preview/README.md) use no real device data. Generated firmware, caches and backups are ignored by Git.

CI runs the same gates on pushes and pull requests. It uploads the merged development image but never flashes a device, creates a tag or publishes a release. [EE04 uses a separate Arduino build](../companion/ee04/README.md); never interchange the two boards' binaries.

Upgrading for replay requires both the partition table and application. Flashing only the app leaves ASR available but disables caching/playback. Do not erase `memo` or `cardid`.
