[简体中文](CHANGELOG.zh_CN.md) · **English**

# Changelog

## Original-audio replay — 2026-09-11

- Double-press OK on review or the latest recording's history note to replay through the built-in speaker.
- Playback progress, audio levels and volume; UP/DOWN adjusts volume and OK stops.
- Default replay volume 85%, adjustable to 100%; +6 dB software gain and soft limiting improve quiet playback.
- Separate Flash cache for one Opus recording up to 120 seconds, bound to its saved note and restorable after reboot.
- Integrity verification and exact removal of encoder delay/padding; existing credentials and note NVS layouts remain unchanged.
- Illustrated replay instructions, corrupt-cache tests and storage-layout checks.

## Initial public version — 2026-09-11

- Direct Volcengine streaming ASR with on-device 16 kbps Opus/Ogg.
- Chinese pixel UI, live transcription, review, 32-note history, completion and one unsaved draft.
- Temporary local setup portal, eight-digit device code, note editing/deletion and JSON export.
- ESP32-C3 memory profile for simultaneous encoding and TLS; protected `cardid` layout retained.
- Distinct connection and service-error explanations without raw provider payload logging.
- Optional separately licensed EE04 receiver with challenge/HMAC authentication.
- Bilingual illustrated documentation, independent source layout and build/host CI.

Hardware acceptance remains limited to the [documented short session](validation.md).
