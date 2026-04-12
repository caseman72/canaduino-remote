#include "zigbee.h"
#include "config.h"
#include "mqtt.h"

#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_zigbee_core.h"

static const char *TAG = "zigbee";

/* ── Sensor mapping: IEEE address → door index ──────────────────────── */

typedef struct {
    esp_zb_ieee_addr_t ieee;   /* 8-byte IEEE address */
    bool               assigned;
} sensor_entry_t;

static sensor_entry_t s_sensors[NUM_DOORS];

/* NVS namespace for sensor mappings */
#define NVS_NAMESPACE "zb_sensors"

static void save_sensor_map(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return;

    for (int i = 0; i < NUM_DOORS; i++) {
        char key[12];
        snprintf(key, sizeof(key), "sensor_%d", i);
        if (s_sensors[i].assigned) {
            nvs_set_blob(h, key, s_sensors[i].ieee, sizeof(esp_zb_ieee_addr_t));
        } else {
            nvs_erase_key(h, key);
        }
    }
    nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "Sensor map saved to NVS");
}

static const uint8_t HARDCODED_IEEE[NUM_DOORS][8] = {
    SENSOR_IEEE_SHOP_1,
    SENSOR_IEEE_SHOP_2,
    SENSOR_IEEE_SHOP_3,
    SENSOR_IEEE_SHOP_4,
    SENSOR_IEEE_BARN,
};

static bool is_zero_ieee(const uint8_t *ieee)
{
    for (int i = 0; i < 8; i++) {
        if (ieee[i] != 0) return false;
    }
    return true;
}

static void load_sensor_map(void)
{
    /* Load hardcoded mappings first */
    for (int i = 0; i < NUM_DOORS; i++) {
        if (!is_zero_ieee(HARDCODED_IEEE[i])) {
            memcpy(s_sensors[i].ieee, HARDCODED_IEEE[i], sizeof(esp_zb_ieee_addr_t));
            s_sensors[i].assigned = true;
            ESP_LOGI(TAG, "Hardcoded sensor %d -> %s: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
                     i, DOORS[i].id,
                     s_sensors[i].ieee[7], s_sensors[i].ieee[6],
                     s_sensors[i].ieee[5], s_sensors[i].ieee[4],
                     s_sensors[i].ieee[3], s_sensors[i].ieee[2],
                     s_sensors[i].ieee[1], s_sensors[i].ieee[0]);
        }
    }

    /* Then fill remaining from NVS (auto-paired sensors) */
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) return;

    for (int i = 0; i < NUM_DOORS; i++) {
        if (s_sensors[i].assigned) continue; /* skip hardcoded */
        char key[12];
        snprintf(key, sizeof(key), "sensor_%d", i);
        size_t len = sizeof(esp_zb_ieee_addr_t);
        if (nvs_get_blob(h, key, s_sensors[i].ieee, &len) == ESP_OK) {
            s_sensors[i].assigned = true;
            ESP_LOGI(TAG, "NVS sensor %d -> %s: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
                     i, DOORS[i].id,
                     s_sensors[i].ieee[7], s_sensors[i].ieee[6],
                     s_sensors[i].ieee[5], s_sensors[i].ieee[4],
                     s_sensors[i].ieee[3], s_sensors[i].ieee[2],
                     s_sensors[i].ieee[1], s_sensors[i].ieee[0]);
        }
    }
    nvs_close(h);
}

/* Find door index by IEEE address.  Returns -1 if not found. */
static int find_door_by_ieee(const esp_zb_ieee_addr_t ieee)
{
    for (int i = 0; i < NUM_DOORS; i++) {
        if (s_sensors[i].assigned &&
            memcmp(s_sensors[i].ieee, ieee, sizeof(esp_zb_ieee_addr_t)) == 0) {
            return i;
        }
    }
    return -1;
}

/* Assign IEEE address to next available slot.  Returns door index or -1. */
static int assign_sensor(const esp_zb_ieee_addr_t ieee)
{
    /* Already assigned? */
    int existing = find_door_by_ieee(ieee);
    if (existing >= 0) return existing;

    /* Find first unassigned slot */
    for (int i = 0; i < NUM_DOORS; i++) {
        if (!s_sensors[i].assigned) {
            memcpy(s_sensors[i].ieee, ieee, sizeof(esp_zb_ieee_addr_t));
            s_sensors[i].assigned = true;
            save_sensor_map();
            ESP_LOGI(TAG, "Assigned sensor -> %s (slot %d)", DOORS[i].id, i);
            return i;
        }
    }

    ESP_LOGW(TAG, "All %d sensor slots full -- ignoring device", NUM_DOORS);
    return -1;
}

void zigbee_clear_sensors(void)
{
    memset(s_sensors, 0, sizeof(s_sensors));
    save_sensor_map();
    ESP_LOGI(TAG, "Cleared all sensor mappings");
}

/* ── Zigbee signal handler (network events) ─────────────────────────── */

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal)
{
    uint32_t *p_sg_p       = signal->p_app_signal;
    esp_err_t err          = signal->esp_err_status;
    esp_zb_app_signal_type_t sig = *p_sg_p;

    switch (sig) {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
        ESP_LOGI(TAG, "Zigbee stack initialized");
        esp_zb_bdb_start_top_level_commissioning(
            ESP_ZB_BDB_MODE_INITIALIZATION);
        break;

    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (err == ESP_OK) {
            if (esp_zb_bdb_is_factory_new()) {
                ESP_LOGI(TAG, "Factory new -- forming network");
                esp_zb_bdb_start_top_level_commissioning(
                    ESP_ZB_BDB_MODE_NETWORK_FORMATION);
            } else {
                ESP_LOGI(TAG, "Coordinator rebooted -- opening for joining");
                esp_zb_bdb_start_top_level_commissioning(
                    ESP_ZB_BDB_MODE_NETWORK_STEERING);
            }
        } else {
            ESP_LOGW(TAG, "Initialization failed (0x%x)", err);
        }
        break;

    case ESP_ZB_BDB_SIGNAL_FORMATION:
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Network formed -- opening for joining");
            esp_zb_bdb_start_top_level_commissioning(
                ESP_ZB_BDB_MODE_NETWORK_STEERING);
        } else {
            ESP_LOGW(TAG, "Formation failed (0x%x) -- retrying", err);
            esp_zb_bdb_start_top_level_commissioning(
                ESP_ZB_BDB_MODE_NETWORK_FORMATION);
        }
        break;

    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Network open for joining (%d sec)",
                     ZB_PERMIT_JOIN_SECONDS);
        }
        break;

    case ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE: {
        esp_zb_zdo_signal_device_annce_params_t *p =
            (esp_zb_zdo_signal_device_annce_params_t *)
                esp_zb_app_signal_get_params(p_sg_p);

        ESP_LOGI(TAG, "Device joined: short=0x%04x ieee=%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
                 p->device_short_addr,
                 p->ieee_addr[7], p->ieee_addr[6],
                 p->ieee_addr[5], p->ieee_addr[4],
                 p->ieee_addr[3], p->ieee_addr[2],
                 p->ieee_addr[1], p->ieee_addr[0]);

        int idx = assign_sensor(p->ieee_addr);
        if (idx >= 0) {
            ESP_LOGI(TAG, "Sensor mapped to %s", DOORS[idx].name);
        }
        break;
    }

    default:
        ESP_LOGD(TAG, "Zigbee signal: 0x%x status: 0x%x", sig, err);
        break;
    }
}

/* ── Zigbee action handler (ZCL commands/reports) ───────────────────── */

static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id,
                                    const void *message)
{
    switch (callback_id) {
    case ESP_ZB_CORE_CMD_IAS_ZONE_ZONE_STATUS_CHANGE_NOT_ID: {
        const esp_zb_zcl_ias_zone_status_change_notification_message_t *msg = message;

        /* Look up which door this sensor belongs to */
        esp_zb_ieee_addr_t ieee;
        esp_zb_ieee_address_by_short(msg->info.src_address.u.short_addr, ieee);

        int idx = find_door_by_ieee(ieee);
        if (idx < 0) {
            ESP_LOGW(TAG, "IAS Zone report from unknown device 0x%04x",
                     msg->info.src_address.u.short_addr);
            break;
        }

        /* Alarm1 bit = contact sensor triggered.
         * For tilt sensors: alarm = tilted = door open.
         * Contact sensors: alarm = no contact = door open. */
        bool alarm = (msg->zone_status & 0x0001);
        bool closed = !alarm;

        ESP_LOGI(TAG, "%s: zone_status=0x%04x -> %s",
                 DOORS[idx].name, msg->zone_status,
                 closed ? "CLOSED" : "OPEN");

        mqtt_publish_door_state(idx, closed);
        break;
    }

    case ESP_ZB_CORE_REPORT_ATTR_CB_ID: {
        const esp_zb_zcl_report_attr_message_t *msg = message;

        /* Look up door by short address */
        esp_zb_ieee_addr_t ieee;
        esp_zb_ieee_address_by_short(msg->src_address.u.short_addr, ieee);
        int idx = find_door_by_ieee(ieee);
        if (idx < 0) break;

        /* Battery percentage from Power Configuration cluster */
        if (msg->cluster == ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG &&
            msg->attribute.id == 0x0021 &&
            msg->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U8) {
            uint8_t pct = *(uint8_t *)msg->attribute.data.value;
            /* Zigbee reports battery as 0-200 (0.5% steps) */
            mqtt_publish_sensor_battery(idx, pct / 2);
        }
        break;
    }

    default:
        ESP_LOGD(TAG, "Unhandled action callback: 0x%x", callback_id);
        break;
    }

    return ESP_OK;
}

/* ── Permit join ────────────────────────────────────────────────────── */

void zigbee_permit_join(void)
{
    esp_zb_bdb_start_top_level_commissioning(
        ESP_ZB_BDB_MODE_NETWORK_STEERING);
    ESP_LOGI(TAG, "Network open for joining (%d sec)", ZB_PERMIT_JOIN_SECONDS);
}

/* ── Zigbee task ────────────────────────────────────────────────────── */

static void zigbee_task(void *arg)
{
    /* Load saved sensor mappings */
    load_sensor_map();

    /* Configure as Zigbee Coordinator */
    esp_zb_cfg_t zb_cfg = {
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_COORDINATOR,
        .install_code_policy = false,
        .nwk_cfg = {
            .zczr_cfg = {
                .max_children = 10,
            },
        },
    };
    esp_zb_init(&zb_cfg);

    /* Create a minimal endpoint (HA CIE device) so sensors can enroll */
    esp_zb_ep_list_t *ep_list = esp_zb_ep_list_create();
    esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();

    /* Basic cluster (mandatory) */
    esp_zb_basic_cluster_cfg_t basic_cfg = {
        .zcl_version  = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,
        .power_source = 0x01, /* Mains */
    };
    esp_zb_cluster_list_add_basic_cluster(
        cluster_list,
        esp_zb_basic_cluster_create(&basic_cfg),
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    /* IAS CIE cluster (client role — we receive zone reports) */
    esp_zb_cluster_list_add_ias_zone_cluster(
        cluster_list,
        esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_IAS_ZONE),
        ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE);

    esp_zb_endpoint_config_t ep_cfg = {
        .endpoint       = ZB_COORD_ENDPOINT,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id  = ESP_ZB_HA_IAS_ZONE_ID,
        .app_device_version = 0,
    };
    esp_zb_ep_list_add_ep(ep_list, cluster_list, ep_cfg);
    esp_zb_device_register(ep_list);

    /* Register action handler for ZCL callbacks */
    esp_zb_core_action_handler_register(zb_action_handler);

    /* Set channel mask (all channels) */
    esp_zb_set_primary_network_channel_set(ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK);

    ESP_LOGI(TAG, "Starting Zigbee coordinator...");
    ESP_ERROR_CHECK(esp_zb_start(false));

    /* Run Zigbee main loop (never returns) */
    esp_zb_stack_main_loop();
}

void zigbee_init(void)
{
    xTaskCreate(zigbee_task, "zigbee", 8192, NULL, 5, NULL);
}
