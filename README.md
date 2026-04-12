# Garage Door Controllers

Two garage door controllers for Home Assistant — the original Liftmaster Remote (ESPHome) and the new Shop Controller (ESP-IDF with Zigbee).

## Shop Controller (ESP-IDF)

ESP32-C6-DevKitC-1 running as Zigbee coordinator + WiFi/MQTT gateway for 5 garage doors at the shop. Replaces the need for a separate Zigbee bridge (pi-three is too far for Zigbee range).

### Hardware

- **Board**: ESP32-C6-DevKitC-1 (RISC-V, WiFi + Zigbee)
- **Relay Board**: 4-channel opto-isolated relay module (active-LOW)
- **Sensors**: 5x Third Reality Zigbee tilt sensors (IAS Zone)
- **Temperature**: DS18B20 on 1-Wire (GPIO 11)

### Features

- Zigbee coordinator — pairs tilt sensors directly (no external Z2M needed)
- 5 HA cover entities via MQTT discovery
  - Shop Door 1 (GPIO 4 → Relay)
  - Shop Door 2 (GPIO 0 → Relay)
  - Shop Door 3 (GPIO 1 → Relay)
  - Shop Door 4 (GPIO 10 → Relay)
  - Barn Door (GPIO 5 → Relay)
- WiFi/Zigbee coexistence (MQTT TLS connects before Zigbee starts)
- Hardcoded sensor IEEE → door mappings with NVS fallback for auto-pairing
- DS18B20 panel temperature (60s updates, Fahrenheit)
- HTTP OTA firmware updates on port 8080
- WiFi diagnostics (IP, SSID, RSSI every 30s)
- 503ms relay pulse duration

### MQTT

Connects to HiveMQ Cloud via TLS (port 8883). Topic prefix: `shop-controller`

Pairing commands:
- `shop-controller/command/pair` — open Zigbee network for joining
- `shop-controller/command/clear` — clear all sensor mappings

### Setup

1. Install [ESP-IDF v5.5](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/get-started/)
2. Copy `shop-controller/main/secrets.example.h` to `secrets.h` and fill in credentials
3. Build and flash via USB:
   ```
   source ~/Projects/esp-idf-v5.5.4/export.sh
   cd shop-controller
   idf.py set-target esp32c6
   idf.py build
   idf.py -p /dev/cu.usbmodem11201 flash
   ```
4. Monitor: `./shop-controller/monitor.sh`
5. OTA updates: `curl -X POST --data-binary @build/shop-controller.bin http://<ip>:8080/upload`

---

## Liftmaster Remote (ESPHome)

ESPHome-based controller for Liftmaster garage door openers using a Canaduino PLC and Arduino Nano ESP32.

### Hardware

- **Controller**: [Canaduino MEGA328 PLC](https://www.universal-solder.ca/product/canaduino-mega328-plc-100-v2-smd-for-arduino-nano/) with Arduino Nano ESP32
- **Remote**: [Liftmaster 4-button remote](https://www.amazon.com/dp/B08H7YR72Y) - relay contacts soldered to button pads
- **Temperature Sensor**: DS18B20 on 1-Wire bus (GPIO 11 / Canaduino SDA/A4 terminal)

### Features

- 4 garage door buttons via MQTT for Home Assistant
  - Shop Door 2 (Relay 1 / D2 / GPIO 5)
  - Shop Door 3 (Relay 2 / D3 / GPIO 6)
  - Shop Door 4 (Relay 3 / D4 / GPIO 7)
  - Barn Door (Relay 4 / D5 / GPIO 8)
- 503ms pulse duration (Oregon area code)
- Dual WiFi network support with runtime switching
- Panel temperature monitoring (DS18B20)
- WiFi diagnostics (RSSI, IP, connected SSID)

### MQTT

Connects to HiveMQ Cloud via TLS (port 8883). Topic prefix: `liftmaster`

### Setup

1. Copy `secrets.example.h` to `secrets.h` and fill in credentials
2. Flash via USB: `esphome run liftmaster-remote.yaml`
3. OTA updates: `./upload.sh`

### Wiring

#### Relay Outputs (to Liftmaster remote button pads)
| Relay | Canaduino | Nano ESP32 | GPIO | Door |
|-------|-----------|------------|------|------|
| 1 | D2 | D2 | 5 | Shop Door 2 |
| 2 | D3 | D3 | 6 | Shop Door 3 |
| 3 | D4 | D4 | 7 | Shop Door 4 |
| 4 | D5 | D5 | 8 | Barn Door |

#### DS18B20 Temperature Sensor
| Wire | Canaduino Terminal | Notes |
|------|-------------------|-------|
| Data | SDA/A4 (GPIO 11) | 4.7kΩ pull-up to VCC required |
| VCC | 5V | |
| GND | GND | |

---

## Secrets

Both projects use a `secrets.h` file (gitignored) for credentials. See `secrets.example.h` for the template.

## License

MIT
