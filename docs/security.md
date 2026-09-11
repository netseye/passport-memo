[简体中文](security.zh_CN.md) · **English**

# Data and security

Audio is held in RAM, encoded on Passport and sent directly to Volcengine over certificate-validated TLS. The firmware does not store audio files or send speech through an EE04 or self-hosted server. Provider-side handling is governed by your Volcengine service settings and terms.

Wi-Fi credentials, ASR credentials, notes and one finalized draft are stored in the device's `memo` NVS partition. This build does not enable Flash encryption. Physical Flash access or a backup can expose that data. Configuration is entered after flashing; no account credentials are embedded in the source or distributable image.

Setup requires physical access to hold OK. A ten-minute WPA2 hotspot and local HTTP portal use the current boot's random eight-digit code. The portal does not echo saved secrets; empty secret inputs preserve them. Use the current display code and a trusted environment. Do not expose the portal to the public internet.

EE04 synchronization authenticates writes and receipts using HMAC-SHA256 and short-lived one-use challenges. It uses LAN HTTP, so it does **not encrypt note contents on the LAN**. The receiver requires explicit pairing and does not receive ASR credentials.

Application logs intentionally omit credentials and transcripts. Review all lower-level logs before sharing: system logs may still identify a device or network. JSON exports and Flash backups are private user data.

For vulnerabilities use [private reporting](https://github.com/netseye/passport-memo/security/advisories/new); if unavailable, request a private contact without posting exploit details. See the [security policy](../.github/SECURITY.md).
