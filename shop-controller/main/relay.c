#include "relay.h"
#include "config.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "relay";

void relay_init(void)
{
    for (int i = 0; i < NUM_DOORS; i++) {
        int pin = DOORS[i].relay_gpio;
        if (pin < 0) continue;

        /* Drive OFF level immediately, before configuring as output,
         * so the pin never glitches HIGH during boot. */
        gpio_set_level(pin, RELAY_OFF_LEVEL);

        gpio_config_t cfg = {
            .pin_bit_mask = 1ULL << pin,
            .mode         = GPIO_MODE_OUTPUT,
            .pull_up_en   = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type    = GPIO_INTR_DISABLE,
        };
        gpio_config(&cfg);
        ESP_LOGI(TAG, "Relay GPIO %d initialized (door %s)", pin, DOORS[i].id);
    }
}

/* ── Pulse task ─────────────────────────────────────────────────────── */

static void pulse_task(void *arg)
{
    int door_index = (int)(intptr_t)arg;
    int pin = DOORS[door_index].relay_gpio;

    ESP_LOGI(TAG, "Pulsing GPIO %d (%s) for %d ms",
             pin, DOORS[door_index].id, PULSE_DURATION_MS);

    gpio_set_level(pin, RELAY_ON_LEVEL);
    vTaskDelay(pdMS_TO_TICKS(PULSE_DURATION_MS));
    gpio_set_level(pin, RELAY_OFF_LEVEL);

    ESP_LOGI(TAG, "Pulse complete on GPIO %d", pin);
    vTaskDelete(NULL);
}

void relay_pulse(int door_index)
{
    if (door_index < 0 || door_index >= NUM_DOORS) return;
    if (DOORS[door_index].relay_gpio < 0) return;

    xTaskCreate(pulse_task, "pulse", 2048,
                (void *)(intptr_t)door_index, 5, NULL);
}
