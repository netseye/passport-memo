[简体中文](validation.zh_CN.md) · **English**

# Validation record

Recorded on **2026-09-11**. Build and host tests are reproducible through [the build guide](build.md); hardware results below are limited to the installed version tested that day.

## Automated scope

- Fresh ESP-IDF 5.5.3 / ESP32-C3 build, app-size limit, partition-table integrity and protected identity checks.
- UTF-8 limits, ASR binary framing, fragmented WebSocket reconstruction, connection/service-error classification and Ogg CRC/page tests.
- Replay cache: standard CRC32 vector, corrupt/truncated metadata, exact pre-skip/padding removal, 120-second bounds and protected storage partition placement.
- Host libopus + firmware muxer + FFmpeg: 20 / 100 / 1,000 ms streams, exact decoded duration and EOS.
- Actual LVGL UI rendered on the host, including all nine phases with 240-character text. Prior measured peak: 29,336 / 40,960 bytes.
- EE04 host tests: content/image/refresh behavior, nonce expiry, replay handling, rollover and signing bytes. EE04 firmware compiles separately for XIAO ESP32-S3.

Published-source build: Passport application **2,090,448 bytes / 3,145,728 limit**, merged image **2,155,984 bytes**; EE04 application **1,769,744 bytes**, static RAM **63,792 bytes**. Sizes can vary with the Git-derived version string.

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

The replay and publishing-cleanup build is now installed on Passport. Application/table readback matched, identity and the entire `memo` store were byte-for-byte unchanged, and boot showed no crash. Listening quality and the complete interaction flow remain under acceptance; the older short-session figures do not measure the new cache writes. CI artifacts remain development builds.

## Original-audio replay test

The cache-enabled build completed a **4.86-second** recording and original-audio playback with ASR `success=1 / partial=0`. The user heard audio but reported low initial volume. Its 244 Opus frames (including padding) occupied 9,760 bytes; replay emitted 77,760 samples, matching the captured duration.

- Encoder mean / maximum: 10,285 / 18,308 microseconds; capture stack minimum free 19,604 bytes.
- Maximum cache write: 4,807 microseconds; free heap after ASR connection 40,044 bytes.
- Maximum decode: 2,478 microseconds; decoder stack minimum free 18,244 bytes; normal completion without a crash.
- The actual cache also passed host libopus decoding and CRC checks. No audio or transcript is published.

The volume follow-up defaults to 85% and adds +6 dB playback gain with a soft limit. Tests exhaust all 16-bit PCM values for monotonicity, symmetry and overflow. Final listening quality still needs physical confirmation.

The volume update preserved cached audio, identity and settings byte-for-byte. After reboot, the earlier 4.86-second clip replayed successfully. Two subsequent 6.6- and 8.2-second recording/ASR/replay cycles both returned `success=1 / partial=0` with exact output sample counts. The updated build measured a 3,496-microsecond maximum decode, 18,244-byte minimum free decoder stack and 39,680-byte minimum free heap after ASR connection. No crash was observed; the user has not yet confirmed the revised volume or distortion.

## First-text acceleration device test

The build with `enable_accelerate_text=true / accelerate_score=10` and 50 ms UI checks is installed. Application verification passed and cached audio, identity and settings remained byte-for-byte unchanged. The complete local build, host tests and nine-state / 240-character UI stress rendering passed; LVGL peaked at 29,336 bytes.

In one **32.42-second** device session, the first nonempty result arrived **1,766 ms** after sending began for the first actual audio batch. It had `final=0`, about 30 seconds before recording ended. There were **36 text changes**, all in non-final packets. The session ended with `success=1 / partial=0`; replay emitted 518,720 samples and completed normally.

The timing includes networking, cloud processing and initial silence after the first audio send. It does not measure mouth-to-LCD latency. No controlled old-build baseline exists, so this does not establish a speedup percentage. The user's subjective comparison of first-text speed and early word errors remains unconfirmed.

## Still to verify on hardware

- A full 120-second recording and repeated start / stop / cancel / reconnect cycles.
- Save, power-cycle, history recovery and unsaved-draft recovery, including interrupted writes.
- Long text, uncommon glyphs, battery readings, idle dimming and audio quality across environments.
- Phone compatibility across multiple models and routers.
- Both boards together: pairing, persistence receipt, display refresh and retry behavior.

No raw logs, speech, credentials, USB identities or device backups are published with this record.
