#include "temp.h"
#include "config.h"
#include "mqtt.h"

#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ds18b20.h"
#include "onewire_bus.h"

static const char *TAG = "temp";

static ds18b20_device_handle_t s_ds18b20 = NULL;

static void temp_task(void *arg)
{
    while (1) {
        if (!s_ds18b20) {
            vTaskDelay(pdMS_TO_TICKS(60000));
            continue;
        }

        esp_err_t err = ds18b20_trigger_temperature_conversion(s_ds18b20);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Conversion trigger failed: %d", err);
            vTaskDelay(pdMS_TO_TICKS(60000));
            continue;
        }

        vTaskDelay(pdMS_TO_TICKS(800)); /* DS18B20 needs ~750ms for 12-bit */

        float temp_c = 0;
        err = ds18b20_get_temperature(s_ds18b20, &temp_c);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Read failed: %d", err);
            vTaskDelay(pdMS_TO_TICKS(60000));
            continue;
        }

        /* Convert to Fahrenheit */
        float temp_f = temp_c * 9.0f / 5.0f + 32.0f;

        char payload[16];
        snprintf(payload, sizeof(payload), "%.1f", temp_f);

        esp_mqtt_client_handle_t client = mqtt_get_client();
        if (client) {
            esp_mqtt_client_publish(client,
                MQTT_TOPIC_PREFIX "/sensor/panel_temperature", payload, 0, 0, 1);
        }

        ESP_LOGI(TAG, "Panel temperature: %s F", payload);
        vTaskDelay(pdMS_TO_TICKS(60000)); /* every 60 seconds */
    }
}

void temp_init(void)
{
    /* Initialize 1-Wire bus on GPIO_DS18B20 */
    onewire_bus_handle_t bus = NULL;
    onewire_bus_config_t bus_cfg = {
        .bus_gpio_num = GPIO_DS18B20,
    };
    onewire_bus_rmt_config_t rmt_cfg = {
        .max_rx_bytes = 10,
    };

    esp_err_t err = onewire_new_bus_rmt(&bus_cfg, &rmt_cfg, &bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "1-Wire bus init failed: %s", esp_err_to_name(err));
        return;
    }

    /* Find first DS18B20 on the bus */
    ds18b20_config_t ds_cfg = {};
    err = ds18b20_new_device_from_bus(bus, &ds_cfg, &s_ds18b20);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "No DS18B20 found on GPIO %d", GPIO_DS18B20);
        return;
    }

    ESP_LOGI(TAG, "DS18B20 found on GPIO %d", GPIO_DS18B20);

    /* Start periodic temperature reading task */
    xTaskCreate(temp_task, "temp", 4096, NULL, 3, NULL);
    ESP_LOGI(TAG, "Temperature monitoring started (60s interval)");
}
