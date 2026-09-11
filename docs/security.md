[简体中文](security.zh_CN.md) · **English**

# Data and security

Passport encodes audio and sends it directly to Volcengine over certificate-validated TLS. A separate `replay` Flash partition retains the latest compressed Opus recording for offline speaker playback. A new recording or deletion of its note clears the previous cache. Audio is excluded from JSON export and is not relayed through EE04 or a self-hosted server. Provider-side handling is governed by your Volcengine service settings and terms.

Wi-Fi credentials, ASR credentials, notes and one finalized draft are stored in the device's `memo` NVS partition. This build does not enable Flash encryption. Physical Flash access or a backup can expose credentials, text and cached audio. The audio CRC detects corruption; it provides neither encryption nor authentication. Configuration is entered after flashing; no account credentials are embedded in the source or distributable image.

Setup requires physical access to hold OK. A ten-minute WPA2 hotspot and local HTTP portal use the current boot's random eight-digit code. The portal does not echo saved secrets; empty secret inputs preserve them. Use the current display code and a trusted environment. Do not expose the portal to the public internet.

EE04 synchronization authenticates writes and receipts using HMAC-SHA256 and short-lived one-use challenges. It uses LAN HTTP, so it does **not encrypt note contents on the LAN**. The receiver requires explicit pairing and does not receive ASR credentials.

Application logs intentionally omit credentials and transcripts. Review all lower-level logs before sharing: system logs may still identify a device or network. JSON exports and Flash backups are private user data.

For vulnerabilities use [private reporting](https://github.com/netseye/passport-memo/security/advisories/new); if unavailable, request a private contact without posting exploit details. See the [security policy](../.github/SECURITY.md).

Browser preview transfers the latest saved recording only after a user click, over the authenticated local setup connection. The code stays in the `X-Memo-Key` header; audio responses are `no-store`. The browser keeps a temporary in-page Blob, and explicit downloads remain on the phone/computer after device cache deletion. Close the page and manage downloaded copies separately when removing private recordings. No audio is sent to another service by preview or download.
