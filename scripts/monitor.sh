#!/usr/bin/env bash
set -euo pipefail
PORT="${PORT:-$(ls /dev/cu.usbmodem* 2>/dev/null | head -1)}"
arduino-cli monitor -p "$PORT" -c baudrate=115200
