#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
FQBN="m5stack:esp32:m5stack_stopwatch:PartitionScheme=app3M_fat9M_16MB"
arduino-cli compile --fqbn "$FQBN" --warnings default --build-path build/badge-pet firmware/badge-pet "$@"
