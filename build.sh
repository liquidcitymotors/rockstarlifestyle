#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
arduino-cli compile --fqbn teensy:avr:teensy31:usb=serialmidi PedalController "$@"
