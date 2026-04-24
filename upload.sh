#!/bin/bash
# OTA upload script for liftmaster-remote
#
# Splits compile + upload because PlatformIO's espressif32 platform blocks
# Python 3.14 (used by Homebrew esphome). The compile step tolerates it but
# the upload step re-runs the version check and aborts — so we compile with
# esphome, then upload the built firmware.ota.bin via esphome.espota2 directly.
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
DEVICE="${1:-liftmaster-remote.local}"
CONFIG="${2:-liftmaster-remote.yaml}"
SECRETS="${3:-secrets.h}"

if [[ ! -f "$SCRIPT_DIR/$SECRETS" ]]; then
    echo "Error: ${SECRETS} not found. Copy secrets.example.h to ${SECRETS} and fill in values."
    exit 1
fi

parse_secret() {
    grep "#define $1 " "$SCRIPT_DIR/$SECRETS" | sed 's/.*"\(.*\)"/\1/'
}

WIFI_PRIMARY_SSID=$(parse_secret WIFI_SSID_PRIMARY)
WIFI_PRIMARY_PASSWORD=$(parse_secret WIFI_PASSWORD_PRIMARY)
WIFI_SECONDARY_SSID=$(parse_secret WIFI_SSID_SECONDARY)
WIFI_SECONDARY_PASSWORD=$(parse_secret WIFI_PASSWORD_SECONDARY)
MQTT_BROKER=$(parse_secret MQTT_BROKER)
MQTT_USERNAME=$(parse_secret MQTT_USERNAME)
MQTT_PASSWORD=$(parse_secret MQTT_PASSWORD)
OTA_PASSWORD=$(parse_secret OTA_PASSWORD)

CONFIG_NAME="${CONFIG%.yaml}"
FIRMWARE="$SCRIPT_DIR/.esphome/build/$CONFIG_NAME/.pioenvs/$CONFIG_NAME/firmware.ota.bin"

cd "$SCRIPT_DIR"

echo "Compiling $CONFIG..."
esphome \
    -s wifi_primary_ssid "$WIFI_PRIMARY_SSID" \
    -s wifi_primary_password "$WIFI_PRIMARY_PASSWORD" \
    -s wifi_secondary_ssid "$WIFI_SECONDARY_SSID" \
    -s wifi_secondary_password "$WIFI_SECONDARY_PASSWORD" \
    -s mqtt_broker "$MQTT_BROKER" \
    -s mqtt_username "$MQTT_USERNAME" \
    -s mqtt_password "$MQTT_PASSWORD" \
    -s ota_password "$OTA_PASSWORD" \
    compile "$CONFIG"

if [[ ! -f "$FIRMWARE" ]]; then
    echo "Error: firmware not found at $FIRMWARE"
    exit 1
fi

echo "Uploading to $DEVICE via OTA..."
ESPHOME_PY="$(head -1 "$(command -v esphome)" | sed 's|^#!||')"
OTA_PASSWORD="$OTA_PASSWORD" DEVICE="$DEVICE" FIRMWARE="$FIRMWARE" \
"$ESPHOME_PY" - <<'PY'
import os, sys
from pathlib import Path
from esphome.espota2 import run_ota
rc, host = run_ota(os.environ["DEVICE"], 3232, os.environ["OTA_PASSWORD"], Path(os.environ["FIRMWARE"]))
if rc != 0:
    print(f"OTA failed (rc={rc})", file=sys.stderr)
    sys.exit(rc)
print(f"OTA complete — device at {host}")
PY
