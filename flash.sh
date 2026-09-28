#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

FQBN="teensy:avr:teensy31:usb=serialmidi"
SKETCH="${1:-PedalController}"

PORT="$(arduino-cli board list | awk '/teensy:avr:teensy31/ {print $1; exit}')"
if [[ -z "${PORT:-}" ]]; then
    echo "No Teensy detected. Plug it in (and press the button if it's running firmware)." >&2
    exit 1
fi

arduino-cli compile --fqbn "$FQBN" "$SKETCH"
arduino-cli upload --fqbn "$FQBN" -p "$PORT" "$SKETCH"
