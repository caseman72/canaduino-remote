#pragma once

#include <stdbool.h>

/* ── Door configuration ─────────────────────────────────────────────── */

#define NUM_DOORS       5
#define PULSE_DURATION_MS 503

/* GPIO assignments — left side of ESP32-C6-DevKitC-1
 * Relays are active-HIGH but C6 GPIOs boot HIGH, so we invert:
 *   drive LOW = relay off (boot-safe), drive HIGH = relay on */
#define GPIO_RELAY_SHOP_1   4
#define GPIO_RELAY_SHOP_2   0
#define GPIO_RELAY_SHOP_3   1
#define GPIO_RELAY_SHOP_4   10
#define GPIO_RELAY_BARN     5
#define GPIO_DS18B20        11

#define RELAY_ON_LEVEL      0   /* inverted: LOW = energize relay */
#define RELAY_OFF_LEVEL     1   /* inverted: HIGH = relay off     */

/* ── MQTT topics ────────────────────────────────────────────────────── */

#define MQTT_TOPIC_PREFIX   "shop-controller"
#define MQTT_AVAIL_TOPIC    MQTT_TOPIC_PREFIX "/status"

/* Pairing commands */
#define MQTT_CMD_PAIR       MQTT_TOPIC_PREFIX "/command/pair"
#define MQTT_CMD_CLEAR      MQTT_TOPIC_PREFIX "/command/clear"

/* ── Zigbee ─────────────────────────────────────────────────────────── */

#define ZB_COORD_ENDPOINT       1
#define ZB_PERMIT_JOIN_SECONDS  180

/* ── Hardcoded sensor IEEE → door mappings ───────────────────────────
 * Fill in as sensors are paired. Set to all zeros for unassigned.
 * IEEE addresses are LSB-first (reversed from log output). */
#define SENSOR_IEEE_SHOP_1  {0x87, 0xcc, 0x01, 0x06, 0x0e, 0xb4, 0xff, 0xff}
#define SENSOR_IEEE_SHOP_2  {0xf7, 0xd4, 0x01, 0x06, 0x0e, 0xb4, 0xff, 0xff}
#define SENSOR_IEEE_SHOP_3  {0xf2, 0x1c, 0x06, 0x06, 0x0e, 0xb4, 0xff, 0xff}
#define SENSOR_IEEE_SHOP_4  {0xe8, 0x2f, 0x06, 0x06, 0x0e, 0xb4, 0xff, 0xff}
#define SENSOR_IEEE_BARN    {0x59, 0xd7, 0x01, 0x06, 0x0e, 0xb4, 0xff, 0xff}

/* ── Door table ─────────────────────────────────────────────────────── */

typedef struct {
    const char *id;         /* "shop_door_1" … "barn_door" */
    const char *name;       /* "Shop Door 1" … "Barn Door" */
    int         relay_gpio; /* GPIO pin for relay output */
    const char *icon;
} door_config_t;

static const door_config_t DOORS[NUM_DOORS] = {
    { "shop_door_1", "Shop Door 1", GPIO_RELAY_SHOP_1, "mdi:garage" },
    { "shop_door_2", "Shop Door 2", GPIO_RELAY_SHOP_2, "mdi:garage" },
    { "shop_door_3", "Shop Door 3", GPIO_RELAY_SHOP_3, "mdi:garage" },
    { "shop_door_4", "Shop Door 4", GPIO_RELAY_SHOP_4, "mdi:garage" },
    { "barn_door",   "Barn Door",   GPIO_RELAY_BARN,   "mdi:barn"   },
};
