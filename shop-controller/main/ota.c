#include "ota.h"

#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_server.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ota";

static esp_err_t upload_handler(httpd_req_t *req)
{
    const esp_partition_t *update = esp_ota_get_next_update_partition(NULL);
    if (!update) {
        ESP_LOGE(TAG, "No OTA partition available");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                            "No OTA partition");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "OTA upload started — partition: %s, size: %d",
             update->label, req->content_len);

    esp_ota_handle_t handle = 0;
    esp_err_t err = esp_ota_begin(update, OTA_WITH_SEQUENTIAL_WRITES, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                            "OTA begin failed");
        return ESP_FAIL;
    }

    char buf[1024];
    int received;
    int total = 0;

    while ((received = httpd_req_recv(req, buf, sizeof(buf))) > 0) {
        err = esp_ota_write(handle, buf, received);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(err));
            esp_ota_abort(handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                "OTA write failed");
            return ESP_FAIL;
        }
        total += received;
    }

    if (received < 0) {
        ESP_LOGE(TAG, "httpd_req_recv error: %d", received);
        esp_ota_abort(handle);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                            "Receive error");
        return ESP_FAIL;
    }

    err = esp_ota_end(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                            "OTA validation failed");
        return ESP_FAIL;
    }

    err = esp_ota_set_boot_partition(update);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s",
                 esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                            "Set boot partition failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "OTA successful (%d bytes) — rebooting", total);
    httpd_resp_sendstr(req, "OK — rebooting\n");

    /* Brief delay then reboot */
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();

    return ESP_OK; /* unreachable */
}

void ota_init(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 8080;
    config.stack_size = 8192;

    httpd_handle_t server = NULL;
    esp_err_t err = httpd_start(&server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP server start failed: %s", esp_err_to_name(err));
        return;
    }

    const httpd_uri_t upload_uri = {
        .uri     = "/upload",
        .method  = HTTP_POST,
        .handler = upload_handler,
    };
    httpd_register_uri_handler(server, &upload_uri);

    ESP_LOGI(TAG, "OTA server listening on port 8080");
}
