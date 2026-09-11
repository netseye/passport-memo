[简体中文](architecture.zh_CN.md) · **English**

# Architecture

## Source map

| Path | Responsibility |
| --- | --- |
| `main/main.c` | Initialize I2C, display, audio and battery; start the app |
| `main/memo_app.c` | Worker events, application state, configuration, NVS notes and drafts |
| `main/memo_ui.c` | LVGL pages, text wrapping, scroll, status and microphone animation |
| `main/ui_pixel*` | Shared upstream pixel theme and layout helpers |
| `main/memo_asr.c` | Capture task, Opus encoding, TLS/WebSocket session and provider results |
| `main/memo_core.c` | Host-testable UTF-8, binary framing, stream reassembly and error classification |
| `main/memo_ogg.c` | Ogg headers, CRC, page sequencing and EOS |
| `main/memo_network.c` | Wi-Fi, clock, temporary portal and signed EE04 transport |
| `main/memo_portal.html` | Embedded, dependency-free local management page |
| `components/bsp/` | Upstream hardware support and pin definitions |
| `tests/`, `tools/` | Host tests, UI fixtures, font generation and build checks |
| `companion/ee04/` | Independently built GPL receiver, using only text over LAN |

Button callbacks enqueue events. The application worker serializes slow operations; application snapshots are protected by a mutex. Audio/network callbacks publish state rather than touching LVGL. Only the UI timer renders the view; non-LVGL callers use the BSP LVGL lock.

## Audio and ASR

Capture is 16 kHz, mono, signed 16-bit PCM. A 20 ms block (640 bytes) becomes a 40-byte Opus packet: 16 kbps CBR, VOIP, complexity 0, VBR/DTX disabled. Each packet gets one Ogg page, and about five pages are uploaded per 100 ms. The resulting audio payload is about 3.4 KB/s versus 32 KB/s PCM. Ogg adds CRC, sequence numbers, 48 kHz granules, a 312-sample pre-skip and final padding/EOS.

The endpoint is `wss://openspeech.bytedance.com/api/v3/sauc/bigmodel_async`. The [official v3 protocol](https://docs.volcengine.com/docs/6561/1354869?lang=zh) carries uncompressed JSON and Ogg audio. The request declares `format=ogg`, `codec=opus`, `rate=16000`, `bits=16`, `channel=1`, `result_type=full`, ITN/punctuation enabled, DDC/utterance metadata disabled, and second-pass processing enabled. Full text replaces the last hypothesis; only session-final frame flags complete the session.

There is no reconnect/replay queue. An interruption keeps recognized text for review. Queue pressure stops capture instead of silently dropping audio. Source logs expose fixed error categories, timing and memory, not raw provider payloads or transcripts.

## C3 RAM budget

| Allocation / setting | Current value |
| --- | --- |
| LVGL heap | 40 KB |
| Capture / encoder task stack | 40 KB, reserved before TLS |
| WebSocket task stack | 6 KB |
| Application worker stack | 8 KB |
| Audio upload queue | 4 slots, each 512 payload bytes |
| Accumulated response bound | 8 KB |
| HTTP upgrade buffer | 8 KB dynamic; separate from 1 KB WebSocket frame buffer |
| Wi-Fi RX | 3 static / 6 dynamic buffers; BA window 3 |

Opus is initialized before the TLS handshake; microphone capture waits for the recognition request to succeed. Wi-Fi fast paths use Flash instead of IRAM. IPv6, Bluetooth and TLS renegotiation are disabled. Dynamic TLS buffers release configuration and peer-certificate storage; server certificate validation and time synchronization stay enabled. Do not replace these defaults with a generic C3 profile.

The 40 KB capture stack retained 19,604 bytes in the one measured short session; this is not proof of worst-case headroom. See [validation](validation.md). A font change, extra task or larger network buffer must be evaluated against internal RAM, not just free Flash.

## Live text latency

The request retains `result_type=full` and `enable_nonstream=true`, and enables `enable_accelerate_text=true` with `accelerate_score=10` (documented range 0–20). Faster initial text can be less accurate; subsequent results can revise it and second-pass recognition remains enabled. `full` means cumulative output, not waiting for the final result. Interim text is not filtered by `definite`. See the [official parameters](https://docs.volcengine.com/docs/6561/1354869?lang=zh).

The UI checks state every 50 ms. Identical strings reuse LVGL's existing text, avoiding body text allocation and redraw on each 100 ms audio-level update. Following the bottom during recording, returning to the top for review and history scrolling every 4.5 seconds remain unchanged.

Logs record the first nonempty result's time from sending the first actual Opus audio batch, plus changed-text and non-final-update counts, without transcript contents. This interval includes networking, recognition and initial silence. It is neither mouth-to-LCD latency nor a before/after controlled comparison.

## Latest-recording playback

`memo_replay.c` owns a separate 256 KB Flash partition. `memo_replay_format.c` supplies host-testable CRC, metadata and exact sample trimming. The cache is erased before recording, then existing 40-byte Opus packets are written through a 4 KB buffer. A 120-second recording plus padding uses 240,040 bytes, with a separate 4 KB metadata sector. Metadata is committed at completion. An interrupted write or failed verification disables replay while recognized text remains saveable.

Drafts match a SHA-256 text digest; saving binds the cache to the note ID. Editing text retains the original audio and deleting the note clears it. Boot restores metadata and playback verifies the full payload CRC. Decoding removes the 104-sample pre-skip at 16 kHz and final padding, ending at the actual sample count.

`MEMO_PLAYBACK` marks configuration and record writes busy. A double press invokes replay; single presses retain existing actions. Button callbacks only change atomic stop/volume values. A temporary 24 KB decoder stack is created after encoder and TLS cleanup; the task serializes audio output, fills silence, then waits for I2S DMA to drain. The 4 KB static buffer is reused for verification; no full PCM recording is retained. Volume defaults to 85%, ranges from 10–100%, and changes by 5% per press. It resets on boot without changing the persisted NVS config layout.

`memo_playback_pcm.c` processes decoded playback PCM only: quiet samples receive 2× gain (about +6 dB), with a continuous soft knee above magnitude 24,000 approaching 32,000 without signed overflow. Tests exhaust the full 16-bit input range for monotonicity, symmetry and bounds. Stored Opus and uploaded audio remain unchanged. The ES8311 default volume curve maps 65% to −17.5 dB and 85% to −7.5 dB; the new default removes 10 dB of output attenuation.
