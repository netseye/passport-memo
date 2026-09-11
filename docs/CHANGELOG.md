[简体中文](CHANGELOG.zh_CN.md) · **English**

# Changelog

## Device note actions and clock — 2026-09-12

- Hold DOWN in history for deletion, e-paper sync and return actions. Deletion shows a note excerpt and defaults to Keep; other notes and drafts are retained.
- Show a Beijing-time (UTC+8) clock on every page, with an unsynchronized placeholder and updates independent of transcript changes.
- Add host coverage for real application button routing, deletion/storage failures and recovery, clock boundaries, UI bounds and minute rollover; include an illustrated deletion guide.

## Simpler setup labels — 2026-09-12

- Remove the password-length hint from the device setup screen and portal copy, keeping the password display and login behavior intact.

## Note header spacing — 2026-09-12

- Separate the date/status row from the text field by 12 px, center the items and allow wrapping on narrow screens.

## Browser audio preview — 2026-09-12

- Preview the latest saved recording in the local portal, with native play/pause, seeking and download controls.
- Keep loaded audio playable in the current page after a hotspot disconnect; explain absent and replaced recordings.
- Authenticate audio fetches using the existing header-based code and stream CRC-verified Ogg without a device decoder or full-clip RAM allocation.
- Add bounded export/failure tests, reference decoding, phone-width browser checks and illustrated instructions.

## First-text latency — 2026-09-11

- Enable moderate first-text acceleration while retaining cumulative output and second-pass corrections; early words may be revised.
- Check UI state every 50 ms and skip allocation/redraw for unchanged label text.
- Log first-result timing and interim-update counts without transcript contents.

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
