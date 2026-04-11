#include "esp_log.h"
#include "nvs_flash.h"

#include "config.h"
#include "relay.h"
#include "wifi.h"
#include "mqtt.h"
#include "zigbee.h"

static const char *TAG = "main";

void app_main(void)
{
    /* Initialize NVS (required by WiFi + Zigbee) */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "=== Shop Controller starting ===");

    /* 1. Relays first — ensure all OFF before anything else */
    relay_init();

    /* 2. WiFi — blocks until connected */
    wifi_init();

    /* 3. MQTT — start connecting (uses hostname, needs DNS before Zigbee) */
    mqtt_init(NULL);

    /* 4. Wait for MQTT TLS handshake to complete BEFORE Zigbee starts.
     *    Zigbee radio contention makes TLS handshakes unreliable. */
    ESP_LOGI(TAG, "Waiting for MQTT connection before starting Zigbee...");
    if (mqtt_wait_connected(30000)) {
        ESP_LOGI(TAG, "MQTT connected — starting Zigbee coordinator");
    } else {
        ESP_LOGW(TAG, "MQTT timeout — starting Zigbee anyway");
    }

    /* 5. Zigbee coordinator — runs in its own task */
    zigbee_init();

    ESP_LOGI(TAG, "=== All subsystems initialized ===");
}
