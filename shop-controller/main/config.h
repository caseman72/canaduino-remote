#pragma once

#include <stdbool.h>

/* ── Door configuration ─────────────────────────────────────────────── */

#define NUM_DOORS       5
#define PULSE_DURATION_MS 503

/* GPIO assignments — left side of ESP32-C6-DevKitC-1
 * Relays are active-HIGH but C6 GPIOs boot HIGH, so we invert:
 *   drive LOW = relay off (boot-safe), drive HIGH = relay on */
#define GPIO_RELAY_SHOP_2   0
#define GPIO_RELAY_SHOP_3   1
#define GPIO_RELAY_SHOP_4   10
#define GPIO_RELAY_BARN     5
#define GPIO_DS18B20        6

#define RELAY_ON_LEVEL      0   /* inverted: LOW = energize relay */
#define RELAY_OFF_LEVEL     1   /* inverted: HIGH = relay off     */

/* ── MQTT topics ────────────────────────────────────────────────────── */

#define MQTT_TOPIC_PREFIX   "shop-controller"
#define MQTT_AVAIL_TOPIC    MQTT_TOPIC_PREFIX "/status"

/* Shop Door 1 close → forward to myq-mcp */
#define MYQ_SHOP1_CMD_TOPIC "myq/shop1/command"

/* Pairing commands */
#define MQTT_CMD_PAIR       MQTT_TOPIC_PREFIX "/command/pair"
#define MQTT_CMD_CLEAR      MQTT_TOPIC_PREFIX "/command/clear"

/* ── Zigbee ─────────────────────────────────────────────────────────── */

#define ZB_COORD_ENDPOINT       1
#define ZB_PERMIT_JOIN_SECONDS  180

/* ── Door table ─────────────────────────────────────────────────────── */

typedef struct {
    const char *id;         /* "shop_1" … "barn" */
    const char *name;       /* "Shop Door 1" … "Barn Door" */
    int         relay_gpio; /* -1 = no relay (shop_1) */
    const char *icon;
    bool        close_only; /* true → no open/stop actions */
} door_config_t;

static const door_config_t DOORS[NUM_DOORS] = {
    { "shop_door_1", "Shop Door 1", -1,               "mdi:garage", true  },
    { "shop_door_2", "Shop Door 2", GPIO_RELAY_SHOP_2, "mdi:garage", false },
    { "shop_door_3", "Shop Door 3", GPIO_RELAY_SHOP_3, "mdi:garage", false },
    { "shop_door_4", "Shop Door 4", GPIO_RELAY_SHOP_4, "mdi:garage", false },
    { "barn_door",   "Barn Door",   GPIO_RELAY_BARN,   "mdi:barn",   false },
};
