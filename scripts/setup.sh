#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
command -v arduino-cli >/dev/null || brew install arduino-cli
[ -f "$HOME/Library/Arduino15/arduino-cli.yaml" ] || arduino-cli config init
arduino-cli config add board_manager.additional_urls https://static-cdn.m5stack.com/resource/arduino/package_m5stack_index.json 2>/dev/null || true
arduino-cli core update-index
arduino-cli core install m5stack:esp32@3.3.9
arduino-cli lib install "M5Unified@0.2.25" "M5GFX@0.2.32" "ArduinoJson@7.4.2" "WebSockets@2.6.1"
python3 -m venv tools/.venv
tools/.venv/bin/pip install -q -r tools/requirements.txt
echo "setup ok"
