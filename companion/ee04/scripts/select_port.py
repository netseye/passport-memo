#!/usr/bin/env python3
"""Choose the EE04 USB port without opening or changing the serial device."""
import json
import os
import subprocess
import shutil
import sys

PREFERRED = os.environ.get('EE04_PORT')

def choose_port(ports, preferred=PREFERRED):
    candidates = []
    for entry in ports:
        port = entry.get('port', {})
        props = port.get('properties', {})
        if props.get('vid', '').lower() == '0x303a' and port.get('address', '').startswith(('/dev/cu.', '/dev/ttyACM', '/dev/ttyUSB')):
            candidates.append(port['address'])
    candidates = sorted(set(candidates))
    if preferred in candidates:
        return preferred
    if len(candidates) == 1:
        return candidates[0]
    if not candidates:
        raise ValueError('No ESP32 USB serial port found. Connect EE04 or pass a port as the first argument.')
    raise ValueError('Multiple ESP32 ports found: ' + ', '.join(candidates) + '. Pass the EE04 port explicitly.')

def main():
    if len(sys.argv) > 1:
        port = sys.argv[1]
        if not os.path.exists(port):
            raise ValueError('Serial port does not exist: ' + port)
    else:
        cli = os.environ.get('ARDUINO_CLI', 'arduino-cli')
        if not shutil.which(cli):
            raise ValueError('arduino-cli not found. Install it or set ARDUINO_CLI.')
        output = subprocess.check_output([cli, 'board', 'list', '--format', 'json'], text=True)
        port = choose_port(json.loads(output).get('detected_ports', []))
    print(port)

if __name__ == '__main__':
    try:
        main()
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
