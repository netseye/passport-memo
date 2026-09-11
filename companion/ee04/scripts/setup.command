#!/bin/sh
set -eu
task_cli="${ARDUINO_CLI:-arduino-cli}"
command -v "$task_cli" >/dev/null 2>&1 || { echo 'Install Arduino CLI first: https://arduino.github.io/arduino-cli/' >&2; exit 1; }
task_index='https://espressif.github.io/arduino-esp32/package_esp32_index.json'
"$task_cli" core update-index --additional-urls "$task_index"
"$task_cli" core install esp32:esp32@3.3.11 --additional-urls "$task_index"
"$task_cli" lib install 'GxEPD2@1.6.9' 'Adafruit GFX Library@1.12.6' \
  'Adafruit BusIO@1.17.4' 'ArduinoJson@7.4.3' 'U8g2_for_Adafruit_GFX@1.8.0'
printf '%s\n' 'Dependencies installed. Run ./scripts/build.command next.'
