#!/usr/bin/env bash
set -euo pipefail

PORT="${1:-$(arduino-cli board list | awk '/Serial Port \(USB\)/ {print $1; exit}')}"
if [[ -z "${PORT:-}" ]]; then
    echo "No USB serial port found. Is the Teensy plugged in and running firmware?" >&2
    exit 1
fi

echo "Connecting to $PORT @ 115200"
echo "To quit screen: Ctrl-A then k then y"
exec screen "$PORT" 115200
