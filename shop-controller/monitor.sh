#!/bin/bash
# Serial monitor for shop-controller
set -e

export IDF_PATH="${IDF_PATH:-$HOME/Projects/esp-idf-v5.5.4}"
source "$IDF_PATH/export.sh" > /dev/null 2>&1

DEVICE="${1:-/dev/cu.usbmodem11201}"

cd "$(dirname "$0")"
idf.py -p "$DEVICE" monitor
