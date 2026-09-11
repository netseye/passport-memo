[简体中文](validation.zh_CN.md) · **English**

# Validation record

Recorded on **2026-09-11**. Build and host tests are reproducible through [the build guide](build.md); hardware results below are limited to the installed version tested that day.

## Automated scope

- Fresh ESP-IDF 5.5.3 / ESP32-C3 build, app-size limit, partition-table integrity and protected identity checks.
- UTF-8 limits, ASR binary framing, fragmented WebSocket reconstruction, connection/service-error classification and Ogg CRC/page tests.
- Host libopus + firmware muxer + FFmpeg: 20 / 100 / 1,000 ms streams, exact decoded duration and EOS.
- Actual LVGL UI rendered on the host, including all eight phases with 240-character text. Prior measured peak: 29,184 / 40,960 bytes.
- EE04 host tests: content/image/refresh behavior, nonce expiry, replay handling, rollover and signing bytes. EE04 firmware compiles separately for XIAO ESP32-S3.

Published-source build: Passport application **2,047,616 bytes / 3,145,728 limit**, merged image **2,113,152 bytes**; EE04 application **1,769,744 bytes**, static RAM **63,792 bytes**. Sizes can vary with the Git-derived version string.

## Real Passport session

Hardware: ESP32-C3, 8 MB Flash, no PSRAM. Credentials: legacy App ID + Access Token. Resource: `volc.bigasr.sauc.duration`.

| Measurement | Result |
| --- | --- |
| Session | Approximately 22 seconds |
| Final result | `success=1`, `partial=0`; visible text confirmed by user |
| Encoded frames | 1,097 |
| Encoding time | 10,774 microseconds average; 20,960 maximum |
| Capture stack low-water free space | 19,604 bytes out of 40 KB |
| Heap after ASR connection | 44,664 free bytes; largest block 15,872 bytes |

The highest individual encode time exceeded one 20 ms frame; the session nevertheless completed. This short measurement does not establish sustained worst-case performance or recognition accuracy. Low-level close warnings were observed after completion; repeat-session behavior remains to be checked.

The installed version includes the C3 memory fixes. Later service-error explanations, formatting, source organization and build-name cleanup in this repository have **not** been reflashed for a new hardware run. CI artifacts are development builds, not a claim of device acceptance.

## Still to verify on hardware

- A full 120-second recording and repeated start / stop / cancel / reconnect cycles.
- Save, power-cycle, history recovery and unsaved-draft recovery, including interrupted writes.
- Long text, uncommon glyphs, battery readings, idle dimming and audio quality across environments.
- Phone compatibility across multiple models and routers.
- Both boards together: pairing, persistence receipt, display refresh and retry behavior.

No raw logs, speech, credentials, USB identities or device backups are published with this record.
