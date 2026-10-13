#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
PORT="${PORT:-$(ls /dev/cu.usbmodem* 2>/dev/null | head -1)}"
ESPTOOL=tools/.venv/bin/esptool.py
mkdir -p backups
STAMP=$(date +%Y%m%d-%H%M%S)
OUT="backups/badge-full-$STAMP.bin"
$ESPTOOL --chip esp32s3 --port "$PORT" -b 921600 read_flash 0 0x1000000 "$OUT"
shasum -a 256 "$OUT" | tee "$OUT.sha256"
$ESPTOOL --chip esp32s3 --port "$PORT" read_flash 0x8000 0xC00 "backups/partitions-$STAMP.bin"
CORE=$(ls -d "$HOME"/Library/Arduino15/packages/m5stack/hardware/esp32/3.3.9)
python3 "$CORE/tools/gen_esp32part.py" "backups/partitions-$STAMP.bin"
