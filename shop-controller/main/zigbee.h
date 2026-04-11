#pragma once

/* Start the Zigbee coordinator in a dedicated FreeRTOS task.
 * Call after WiFi is connected (coexistence requires WiFi first). */
void zigbee_init(void);

/* Open network for joining (ZB_PERMIT_JOIN_SECONDS). */
void zigbee_permit_join(void);

/* Clear all sensor mappings from NVS and reset. */
void zigbee_clear_sensors(void);
