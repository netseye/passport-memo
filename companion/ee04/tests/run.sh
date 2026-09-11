#!/bin/sh
set -eu
task_root="$(CDPATH= cd "$(dirname "$0")/.." && pwd)"
task_tmp="$(mktemp -d "${TMPDIR:-/tmp}/ee04-tests.XXXXXX")"
trap 'rm -rf "$task_tmp"' EXIT HUP INT TERM
cd "$task_root"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror tests/test_image.cpp -o "$task_tmp/image"
"$task_tmp/image"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror tests/test_refresh.cpp -o "$task_tmp/refresh"
"$task_tmp/refresh"
node tests/test_image.js
python3 tests/test_port.py
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror "$task_root/tests/test_passport.cpp" -o "$task_tmp/test_passport"
"$task_tmp/test_passport"
