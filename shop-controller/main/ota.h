#pragma once

/* Start HTTP OTA server on port 8080.
 * Upload firmware: curl -X POST --data-binary @firmware.bin http://<ip>:8080/upload
 * Device reboots automatically after successful upload. */
void ota_init(void);
