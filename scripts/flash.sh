#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
PORT="${PORT:-$(ls /dev/cu.usbmodem* 2>/dev/null | head -1)}"
FQBN="m5stack:esp32:m5stack_stopwatch:PartitionScheme=app3M_fat9M_16MB"
ls backups/badge-full-*.bin >/dev/null 2>&1 || { echo "refusing to flash: run scripts/backup.sh first"; exit 1; }
scripts/build.sh
arduino-cli upload -p "$PORT" --fqbn "$FQBN" --input-dir build/badge-pet firmware/badge-pet
