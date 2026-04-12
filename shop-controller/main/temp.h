#pragma once

/* Initialize DS18B20 1-Wire sensor and start periodic reads.
 * Publishes temperature to MQTT every 60 seconds. */
void temp_init(void);
