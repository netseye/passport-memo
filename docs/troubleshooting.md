[简体中文](troubleshooting.zh_CN.md) · **English**

# Troubleshooting

| Symptom | Check / action |
| --- | --- |
| Phone says wrong password / cannot join | Forget the saved Passport-Memo network. Re-enter the current eight-digit display code, keeping leading zeros. The code changes after reboot. |
| Phone says no internet | Stay connected; this hotspot only serves local setup. Open `http://192.168.4.1` manually. |
| Saved settings but nothing changes | Exit setup with device UP or OK. Router association and time synchronization start after the hotspot closes. |
| Web page says disconnected after exit | Expected: setup closes its local HTTP service and hotspot. Hold OK and reconnect to edit again. |
| Wi-Fi stays unavailable | Confirm a 2.4 GHz SSID and the router password, not the eight-digit device code. Check Home for missing-network/authentication status. |
| Time synchronization does not finish | Check that the router has internet access and permits network time. TLS requires correct time. |
| HTTP 401 / 403 | Check credential type, App ID / Access Token ownership, enabled service, and resource. A saved API Key takes priority over legacy credentials. |
| Error 45000292 or a quota message | Check enabled model version and duration/concurrency product and available quota. Do not infer a unique cause from this number alone. The successful test used `volc.bigasr.sauc.duration`. |
| WebSocket connected, then recognition fails | HTTP upgrade success only proves the handshake; resource permissions, quota and audio request may still fail afterward. |
| Recording task / Opus memory failure | Rebuild from tracked defaults into a fresh SDK configuration; retain the C3 memory settings. Include sanitized heap/stack statistics in a report. |
| No text / interrupted | Wait for listening before speaking, check the microphone and network, then stop and review any retained text. The firmware does not replay audio automatically. |
| Text changed or stops at 240 characters | Interim hypotheses can revise words; 240 Unicode characters is the current per-note limit. |
| No replay on double press | Only the latest recording has audio. Starting another recording replaces it. Upgrade the partition table with the app; old notes cannot gain missing audio. |
| Playback too quiet / loud | Press UP/DOWN during playback; default 85%, steps of 5%. The feedback-sounds toggle does not disable replay. |
| EE04 sync failed | Check same LAN, enabled receiver, IP and pairing code. Hold DOWN in history to retry. Passport retains its local copy. |

For a report, include the commit, board, recording duration, displayed error and sanitized application log. Never attach tokens, SSIDs/passwords, transcripts, NVS/Flash dumps or a screen containing your setup code. [Open an issue](https://github.com/netseye/passport-memo/issues/new/choose).
