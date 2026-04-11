#include "wifi.h"
#include "secrets.h"

#include <stdio.h>
#include <string.h>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

static const char *TAG = "wifi";
static EventGroupHandle_t s_event_group;
#define CONNECTED_BIT BIT0

static char s_ip_str[16] = "";

static void event_handler(void *arg, esp_event_base_t base,
                           int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Disconnected -- reconnecting");
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
        snprintf(s_ip_str, sizeof(s_ip_str), IPSTR, IP2STR(&e->ip_info.ip));
        ESP_LOGI(TAG, "Connected -- IP: %s", s_ip_str);
        xEventGroupSetBits(s_event_group, CONNECTED_BIT);
    }
}

const char *wifi_resolve_host(const char *hostname)
{
    static char ip_buf[48];
    struct addrinfo hints = { .ai_family = AF_INET };
    struct addrinfo *res = NULL;

    ESP_LOGI(TAG, "Resolving %s ...", hostname);
    int err = getaddrinfo(hostname, NULL, &hints, &res);
    if (err != 0 || res == NULL) {
        ESP_LOGE(TAG, "DNS resolve failed for %s (err=%d)", hostname, err);
        return NULL;
    }

    struct sockaddr_in *addr = (struct sockaddr_in *)res->ai_addr;
    inet_ntoa_r(addr->sin_addr, ip_buf, sizeof(ip_buf));
    freeaddrinfo(res);
    ESP_LOGI(TAG, "Resolved %s -> %s", hostname, ip_buf);
    return ip_buf;
}

const char *wifi_get_ip(void)
{
    return s_ip_str;
}

const char *wifi_get_ssid(void)
{
    return WIFI_SSID;
}

int8_t wifi_get_rssi(void)
{
    wifi_ap_record_t ap;
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
        return ap.rssi;
    }
    return 0;
}

void wifi_init(void)
{
    s_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, event_handler, NULL, NULL));

    wifi_config_t wifi_cfg = {
        .sta = {
            .ssid     = WIFI_SSID,
            .password = WIFI_PASSWORD,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Waiting for connection to %s ...", WIFI_SSID);
    xEventGroupWaitBits(s_event_group, CONNECTED_BIT,
                        false, true, portMAX_DELAY);

    /* Disable power save — gives WiFi more radio time for coexistence */
    esp_wifi_set_ps(WIFI_PS_NONE);
    ESP_LOGI(TAG, "WiFi power save disabled (Zigbee coexistence)");

    /* Set Google DNS to avoid router DNS issues */
    ip_addr_t dns1;
    ipaddr_aton("8.8.8.8", &dns1);
    dns_setserver(0, &dns1);
    ESP_LOGI(TAG, "DNS set to 8.8.8.8");
}
