#include "mqtt.h"
#include "config.h"
#include "secrets.h"
#include "relay.h"
#include "wifi.h"

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "mqtt_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

static const char *TAG = "mqtt";

/* CA certificate for TLS — embedded via CMakeLists.txt */
extern const uint8_t mqtt_ca_pem_start[] asm("_binary_mqtt_ca_pem_start");
extern const uint8_t mqtt_ca_pem_end[]   asm("_binary_mqtt_ca_pem_end");

static esp_mqtt_client_handle_t s_client = NULL;
static EventGroupHandle_t s_mqtt_event_group = NULL;
#define MQTT_CONNECTED_BIT BIT0

/* Current door states — true = closed, false = open.
 * Assume closed on boot (safe default for garage doors). */
static bool s_door_closed[NUM_DOORS] = { true, true, true, true, true };

/* ── HA MQTT Discovery ──────────────────────────────────────────────── */

static void publish_discovery(void)
{
    for (int i = 0; i < NUM_DOORS; i++) {
        const door_config_t *d = &DOORS[i];

        char disc_topic[128];
        snprintf(disc_topic, sizeof(disc_topic),
                 "homeassistant/cover/shop-controller/%s/config", d->id);

        /* Build JSON — payload_open/close/stop are all the same for toggle
         * doors.  Shop Door 1 is close-only (no open/stop). */
        char payload[768];
        snprintf(payload, sizeof(payload),
            "{"
                "\"unique_id\":\"shop-controller-cover-%s\","
                "\"name\":\"%s\","
                "\"device_class\":\"garage\","
                "\"state_topic\":\"" MQTT_TOPIC_PREFIX "/cover/%s/state\","
                "\"command_topic\":\"" MQTT_TOPIC_PREFIX "/cover/%s/command\","
                "\"payload_open\":\"OPEN\","
                "\"payload_close\":\"CLOSE\","
                "\"payload_stop\":\"STOP\","
                "\"state_open\":\"open\","
                "\"state_closed\":\"closed\","
                "\"optimistic\":false,"
                "\"availability_topic\":\"" MQTT_AVAIL_TOPIC "\","
                "\"payload_available\":\"online\","
                "\"payload_not_available\":\"offline\","
                "\"icon\":\"%s\","
                "\"device\":{"
                    "\"ids\":[\"shop-controller\"],"
                    "\"name\":\"Shop Controller\","
                    "\"mf\":\"Espressif\","
                    "\"mdl\":\"ESP32-C6\""
                "}"
            "}",
            d->id, d->name, d->id, d->id, d->icon);

        esp_mqtt_client_publish(s_client, disc_topic, payload, 0, 1, 1);
    }
    ESP_LOGI(TAG, "Published HA cover discovery for %d doors", NUM_DOORS);
}

/* ── State publishing ───────────────────────────────────────────────── */

void mqtt_publish_door_state(int door_index, bool closed)
{
    if (!s_client || door_index < 0 || door_index >= NUM_DOORS) return;

    s_door_closed[door_index] = closed;

    char topic[96];
    snprintf(topic, sizeof(topic),
             MQTT_TOPIC_PREFIX "/cover/%s/state", DOORS[door_index].id);

    const char *state = closed ? "closed" : "open";
    esp_mqtt_client_publish(s_client, topic, state, 0, 1, 1);
    ESP_LOGI(TAG, "%s → %s", DOORS[door_index].name, state);
}

void mqtt_publish_all_states(void)
{
    for (int i = 0; i < NUM_DOORS; i++) {
        mqtt_publish_door_state(i, s_door_closed[i]);
    }
}

void mqtt_publish_sensor_battery(int door_index, int battery_pct)
{
    if (!s_client || door_index < 0 || door_index >= NUM_DOORS) return;

    char topic[96];
    snprintf(topic, sizeof(topic),
             MQTT_TOPIC_PREFIX "/sensor/%s/battery", DOORS[door_index].id);

    char payload[8];
    snprintf(payload, sizeof(payload), "%d", battery_pct);
    esp_mqtt_client_publish(s_client, topic, payload, 0, 0, 1);
}

/* ── Diagnostics ────────────────────────────────────────────────────── */

static void publish_rssi(void);

static void publish_diagnostics(void)
{
    if (!s_client) return;

    esp_mqtt_client_publish(s_client,
        MQTT_TOPIC_PREFIX "/sensor/ip_address", wifi_get_ip(), 0, 0, 1);
    esp_mqtt_client_publish(s_client,
        MQTT_TOPIC_PREFIX "/sensor/wifi_network", wifi_get_ssid(), 0, 0, 1);

    publish_rssi();
}

static void publish_rssi(void)
{
    if (!s_client) return;

    char rssi_str[8];
    snprintf(rssi_str, sizeof(rssi_str), "%d", wifi_get_rssi());
    esp_mqtt_client_publish(s_client,
        MQTT_TOPIC_PREFIX "/sensor/wifi_rssi", rssi_str, 0, 0, 1);
}

static void rssi_timer_cb(void *arg)
{
    publish_rssi();
}

/* ── Command handling ───────────────────────────────────────────────── */

static void handle_command(const char *topic, int topic_len,
                           const char *data,  int data_len)
{
    /* Match: shop-controller/cover/{id}/command */
    for (int i = 0; i < NUM_DOORS; i++) {
        char expected[96];
        snprintf(expected, sizeof(expected),
                 MQTT_TOPIC_PREFIX "/cover/%s/command", DOORS[i].id);

        if (topic_len == (int)strlen(expected) &&
            strncmp(topic, expected, topic_len) == 0) {

            /* All doors: any command pulses the relay (toggle door) */
            ESP_LOGI(TAG, "%s: command %.*s → pulse relay",
                     DOORS[i].name, data_len, data);
            relay_pulse(i);
            return;
        }
    }

    /* Pairing commands */
    if (topic_len == (int)strlen(MQTT_CMD_PAIR) &&
        strncmp(topic, MQTT_CMD_PAIR, topic_len) == 0) {
        ESP_LOGI(TAG, "Pair command received");
        /* Zigbee permit join is handled in zigbee.c via extern */
        extern void zigbee_permit_join(void);
        zigbee_permit_join();
        return;
    }

    if (topic_len == (int)strlen(MQTT_CMD_CLEAR) &&
        strncmp(topic, MQTT_CMD_CLEAR, topic_len) == 0) {
        ESP_LOGI(TAG, "Clear sensors command received");
        extern void zigbee_clear_sensors(void);
        zigbee_clear_sensors();
        return;
    }
}

/* ── MQTT event handler ─────────────────────────────────────────────── */

static void mqtt_event_handler(void *arg, esp_event_base_t base,
                                int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch (event->event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Connected to MQTT broker");
        if (s_mqtt_event_group) {
            xEventGroupSetBits(s_mqtt_event_group, MQTT_CONNECTED_BIT);
        }

        /* Birth message */
        esp_mqtt_client_publish(s_client, MQTT_AVAIL_TOPIC,
                                "online", 0, 1, 1);

        /* Subscribe to door command topics */
        for (int i = 0; i < NUM_DOORS; i++) {
            char topic[96];
            snprintf(topic, sizeof(topic),
                     MQTT_TOPIC_PREFIX "/cover/%s/command", DOORS[i].id);
            esp_mqtt_client_subscribe(s_client, topic, 1);
        }

        /* Subscribe to pairing commands */
        esp_mqtt_client_subscribe(s_client, MQTT_CMD_PAIR, 1);
        esp_mqtt_client_subscribe(s_client, MQTT_CMD_CLEAR, 1);

        /* Publish discovery + current states + diagnostics */
        publish_discovery();
        mqtt_publish_all_states();
        publish_diagnostics();
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "Disconnected from MQTT broker");
        break;

    case MQTT_EVENT_DATA:
        handle_command(event->topic, event->topic_len,
                       event->data,  event->data_len);
        break;

    default:
        break;
    }
}

/* ── Initialization ─────────────────────────────────────────────────── */

void mqtt_init(const char *resolved_ip)
{
    (void)resolved_ip; /* no longer used — kept for API compat */

    esp_mqtt_client_config_t cfg = {
        .broker = {
            .address.uri = MQTT_BROKER_URI,
            .verification.certificate = (const char *)mqtt_ca_pem_start,
        },
        .credentials = {
            .username = MQTT_USERNAME,
            .authentication.password = MQTT_PASSWORD,
            .client_id = "shop-controller",
        },
        .session = {
            .last_will = {
                .topic   = MQTT_AVAIL_TOPIC,
                .msg     = "offline",
                .msg_len = 7,
                .qos     = 1,
                .retain  = 1,
            },
        },
        .network = {
            .timeout_ms     = 30000,  /* 30s network timeout (TLS handshake) */
            .reconnect_timeout_ms = 10000,  /* 10s between reconnect attempts */
        },
    };

    s_mqtt_event_group = xEventGroupCreate();

    s_client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID,
                                    mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_client);

    /* RSSI update timer — every 30 seconds */
    const esp_timer_create_args_t timer_args = {
        .callback = rssi_timer_cb,
        .name = "rssi",
    };
    esp_timer_handle_t timer;
    esp_timer_create(&timer_args, &timer);
    esp_timer_start_periodic(timer, 30 * 1000 * 1000); /* 30s in microseconds */

    ESP_LOGI(TAG, "MQTT client started");
}

bool mqtt_wait_connected(int timeout_ms)
{
    if (!s_mqtt_event_group) return false;
    EventBits_t bits = xEventGroupWaitBits(s_mqtt_event_group,
        MQTT_CONNECTED_BIT, false, true,
        pdMS_TO_TICKS(timeout_ms));
    return (bits & MQTT_CONNECTED_BIT) != 0;
}

esp_mqtt_client_handle_t mqtt_get_client(void)
{
    return s_client;
}
