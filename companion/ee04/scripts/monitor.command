#!/bin/sh
set -eu
task_dir="$(CDPATH= cd "$(dirname "$0")" && pwd)"
task_cli="${ARDUINO_CLI:-arduino-cli}"
task_port="$(python3 "$task_dir/select_port.py" "$@")"
printf '%s\n' 'Serial 115200. ? = help, k = status, Ctrl-C = exit.'
"$task_cli" monitor --port "$task_port" --config baudrate=115200
