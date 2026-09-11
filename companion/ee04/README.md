[简体中文](README.zh_CN.md) · **English**

# EE04 companion receiver

Optional, separate **GPL-3.0-only** firmware for the **EE04 / XIAO ESP32-S3** e-paper device. Passport works on its own; EE04 receives confirmed text and needs no Volcengine credentials. This receiver has host/build validation, but two-device physical synchronization is not yet tested.

## Build

Install Arduino CLI and use the pinned dependency installer. Run from this directory:

```sh
./scripts/setup.command
./tests/run.sh
./scripts/build.command
./scripts/flash.command
```

The setup script pins ESP32 Arduino 3.3.11, GxEPD2 1.6.9, Adafruit GFX 1.12.6, Adafruit BusIO 1.17.4, ArduinoJson 7.4.3 and U8g2_for_Adafruit_GFX 1.8.0. The flasher has a board/port selection procedure; verify XIAO ESP32-S3 before writing. Never use Passport's C3 image.

## Pair

1. Hold EE04 **K2 for 1.2 seconds** to open management. Join **EE04-Setup** with the password on its display and open **http://192.168.4.1** to configure router Wi-Fi. For access through its displayed LAN address, log in as `ee04` with the same display password.
2. Enable Passport linking. Copy the newly generated **32-character pairing code**; it is shown only when generated. Regeneration revokes the previous code.
3. In Passport settings, enter EE04's LAN IP and this code. Keep both devices on the same trusted LAN; reserve the EE04 IP in your router if necessary.
4. Save a Passport note to attempt sync. Hold DOWN on a selected history note to retry.

EE04 persists one current note, including completion status, on its existing note page. The other clock/weather/image pages remain selectable. A signed receipt means the note was persisted; e-paper refresh follows afterward. Web edits and completion changes require a manual retry to update EE04.

## Wire protocol

The receiver listens on port **8080** while enabled and connected, including after its temporary setup window closes. Disabling linking stops the receiver.

Obtain a 30-second, one-use nonce with `GET /v1/challenge`. Submit `POST /v1/memo` with `nonce`, `id`, `done`, `text` and lowercase hexadecimal `mac`. HMAC-SHA256 signs these exact UTF-8 bytes without a trailing newline:

```text
memo-v1
<nonce>
<decimal id>
<0 or 1>
<text>
```

Use the pairing code as an **ASCII string key**, not decoded hexadecimal. The reply signs `saved
<nonce>
<decimal id>`. Four outstanding challenges are retained. Authenticated persistence consumes the nonce; identical duplicate notes are acknowledged without rewriting Flash.

This provides authentication, not LAN encryption. See [data and security](../../docs/security.md) and [attributions](../../docs/attributions.md).
