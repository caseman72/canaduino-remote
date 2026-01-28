# Liftmaster Remote

ESPHome-based controller for Liftmaster garage door openers using a Canaduino PLC and Arduino Nano ESP32.

## Hardware

- **Controller**: [Canaduino MEGA328 PLC](https://www.universal-solder.ca/product/canaduino-mega328-plc-100-v2-smd-for-arduino-nano/) with Arduino Nano ESP32
- **Remote**: [Liftmaster 4-button remote](https://www.amazon.com/dp/B08H7YR72Y) - relay contacts soldered to button pads
- **Temperature Sensor**: DS18B20 on 1-Wire bus (GPIO 11 / Canaduino SDA/A4 terminal)

## Features

- 4 garage door buttons via MQTT for Home Assistant
  - Shop Door 2 (Relay 1 / D2 / GPIO 5)
  - Shop Door 3 (Relay 2 / D3 / GPIO 6)
  - Shop Door 4 (Relay 3 / D4 / GPIO 7)
  - Barn Door (Relay 4 / D5 / GPIO 8)
- 503ms pulse duration (Oregon area code)
- Dual WiFi network support with runtime switching (P3 button)
- Remote restart capability (P5 button)
- Panel temperature monitoring (DS18B20)
- WiFi diagnostics (RSSI, IP, connected SSID)

## MQTT

Connects to HiveMQ Cloud via TLS (port 8883). Topic prefix: `liftmaster`

Home Assistant auto-discovery enabled.

## Setup

1. Copy `secrets.example.h` to `secrets.h` and fill in your credentials
2. Flash via USB: `esphome run liftmaster-remote.yaml`
3. OTA updates: `./upload.sh`

## Secrets

The `secrets.h` file is gitignored and contains:
- Primary and secondary WiFi credentials
- MQTT broker URL and credentials
- OTA password

See `secrets.example.h` for the template.

## Wiring

### Relay Outputs (to Liftmaster remote button pads)
| Relay | Canaduino | Nano ESP32 | GPIO | Door |
|-------|-----------|------------|------|------|
| 1 | D2 | D2 | 5 | Shop Door 2 |
| 2 | D3 | D3 | 6 | Shop Door 3 |
| 3 | D4 | D4 | 7 | Shop Door 4 |
| 4 | D5 | D5 | 8 | Barn Door |

### DS18B20 Temperature Sensor
| Wire | Canaduino Terminal | Notes |
|------|-------------------|-------|
| Data | SDA/A4 (GPIO 11) | 4.7kΩ pull-up to VCC required |
| VCC | 5V | |
| GND | GND | |

## License

MIT
