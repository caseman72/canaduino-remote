#pragma once

#include <stdbool.h>
#include "mqtt_client.h"

/* Initialize MQTT client (TLS) and connect to broker.
 * Publishes LWT, HA cover discovery, and subscribes to command topics. */
void mqtt_init(const char *resolved_ip);

/* Block until MQTT is connected (with timeout in ms). Returns true if connected. */
bool mqtt_wait_connected(int timeout_ms);

/* Publish door state for door_index (0–4).
 * closed = true → "closed", false → "open". */
void mqtt_publish_door_state(int door_index, bool closed);

/* Publish all current door states (call on reconnect). */
void mqtt_publish_all_states(void);

/* Forward-declared so zigbee.c can trigger state publishes. */
void mqtt_publish_sensor_battery(int door_index, int battery_pct);

/* Get the MQTT client handle (for temp.c etc). Returns NULL if not connected. */
esp_mqtt_client_handle_t mqtt_get_client(void);
