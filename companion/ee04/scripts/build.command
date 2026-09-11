#!/bin/sh
set -eu
task_dir="$(CDPATH= cd "$(dirname "$0")" && pwd)"
task_root="$(CDPATH= cd "$task_dir/.." && pwd)"
task_cli="${ARDUINO_CLI:-arduino-cli}"
command -v "$task_cli" >/dev/null 2>&1 || { echo 'arduino-cli not found. Install it or set ARDUINO_CLI.' >&2; exit 1; }
"$task_cli" compile \
  --fqbn esp32:esp32:XIAO_ESP32S3:CDCOnBoot=default,USBMode=hwcdc \
  --build-path "$task_root/build/arduino" \
  --output-dir "$task_root/build/firmware" "$task_root/firmware/EE04_Demo"
printf '%s\n' 'Build complete. Run ./scripts/flash.command to upload.'
