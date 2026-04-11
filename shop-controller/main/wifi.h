#pragma once

#include <stdint.h>

/* Initialize WiFi STA and block until connected. */
void wifi_init(void);

/* Resolve a hostname to IP string. Call BEFORE Zigbee starts.
 * Returns static buffer — copy if needed. */
const char *wifi_resolve_host(const char *hostname);

/* Get current WiFi info for diagnostics. */
const char *wifi_get_ip(void);
const char *wifi_get_ssid(void);
int8_t wifi_get_rssi(void);
