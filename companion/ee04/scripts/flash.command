#!/bin/sh
set -eu
task_dir="$(CDPATH= cd "$(dirname "$0")" && pwd)"
task_root="$(CDPATH= cd "$task_dir/.." && pwd)"
task_cli="${ARDUINO_CLI:-arduino-cli}"
command -v "$task_cli" >/dev/null 2>&1 || { echo 'arduino-cli not found. Install it or set ARDUINO_CLI.' >&2; exit 1; }
[ -f "$task_root/build/firmware/EE04_Demo.ino.bin" ] || { echo 'Firmware missing. Run ./scripts/build.command first.' >&2; exit 1; }
task_port="$(python3 "$task_dir/select_port.py" "$@")"
printf 'Uploading EE04_Demo v1.7-refresh to %s\n' "$task_port"
"$task_cli" upload \
  --fqbn esp32:esp32:XIAO_ESP32S3:CDCOnBoot=default,USBMode=hwcdc \
  --port "$task_port" --input-dir "$task_root/build/firmware" "$task_root/firmware/EE04_Demo"
printf '%s\n' 'Upload complete. Serial Monitor: 115200 baud.'
