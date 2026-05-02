#include "mqtt.h"
#include "sim4g_a7680c.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"

static const char *TAG = "MQTT_AT";
static bool s_mqtt_connected = false;

static esp_err_t wait_for_prompt(const char *expect, uint32_t timeout_ms)
{
    char resp[256] = {0};
    uint32_t start = xTaskGetTickCount();

    while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(timeout_ms)) {
        int len = sim4g_read(resp, sizeof(resp) - 1, 500); // Đọc dữ liệu từ module
        if (len > 0) {
            resp[len] = 0;
            ESP_LOGI(TAG, "<< %s", resp);

            // 1. KIỂM TRA MẤT KẾT NỐI (QUAN TRỌNG)
            if (strstr(resp, "+CMQTTCONNLOST")) {
                ESP_LOGW(TAG, "Detected MQTT Connection Lost!");
                s_mqtt_connected = false; // Cập nhật trạng thái ngay lập tức
                return ESP_FAIL; 
            }

            // 2. Kiểm tra phản hồi mong đợi (như ">" hoặc "OK")
            if (strstr(resp, expect)) {
                return ESP_OK;
            }
        }
    }
    return ESP_ERR_TIMEOUT;
}

esp_err_t mqtt_init(void)
{
    s_mqtt_connected = false;
    return ESP_OK;
}

esp_err_t mqtt_connect(const char *host,
                    int port,
                    const char *client_id,
                    const char *username,
                    const char *password)
{
    if (host == NULL || client_id == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    char cmd[256];
    esp_err_t err;

    ESP_LOGI(TAG, "Attempting MQTT connection to %s:%d with client_id: %s",
            host, port, client_id);

    /* dọn session cũ, bỏ qua lỗi */
    sim4g_send_cmd("AT+CMQTTDISC=0,60", "OK", 3000);
    sim4g_send_cmd("AT+CMQTTREL=0", "OK", 3000);
    sim4g_send_cmd("AT+CMQTTSTOP", "OK", 5000);
    vTaskDelay(pdMS_TO_TICKS(1000));

    /* start mqtt service */
    err = sim4g_send_cmd("AT+CMQTTSTART", "OK", 10000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "CMQTTSTART failed");
        return err;
    }

    vTaskDelay(pdMS_TO_TICKS(200));

    snprintf(cmd, sizeof(cmd), "AT+CMQTTACCQ=0,\"%s\"", client_id);
    err = sim4g_send_cmd(cmd, "OK", 5000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "CMQTTACCQ failed");
        return err;
    }

    vTaskDelay(pdMS_TO_TICKS(200));

    if (username != NULL && strlen(username) > 0) {
        if (password != NULL && strlen(password) > 0) {
            snprintf(cmd, sizeof(cmd),
                    "AT+CMQTTCONNECT=0,\"tcp://%s:%d\",60,1,\"%s\",\"%s\"",
                    host, port, username, password);
        } else {
            snprintf(cmd, sizeof(cmd),
                    "AT+CMQTTCONNECT=0,\"tcp://%s:%d\",60,1,\"%s\"",
                    host, port, username);
        }
    }

    err = sim4g_send_cmd(cmd, "+CMQTTCONNECT: 0,0", 15000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "CMQTTCONNECT failed");
        return err;
    }

    s_mqtt_connected = true;
    ESP_LOGI(TAG, "MQTT connected");
    return ESP_OK;
}

esp_err_t mqtt_publish(const char *topic, const char *payload, int qos, int retain)
{
    // Kiểm tra trạng thái kết nối trước khi thực hiện[cite: 3, 7]
    if (!s_mqtt_connected) {
        ESP_LOGE(TAG, "MQTT not connected, skipping publish");
        return ESP_ERR_INVALID_STATE;
    }

    if (sim4g_lock(10000) != ESP_OK) {
        ESP_LOGE(TAG, "Cannot acquire UART for publish");
        return ESP_ERR_TIMEOUT;
    }

    char cmd[128];


    snprintf(cmd, sizeof(cmd), "AT+CMQTTTOPIC=0,%d\r\n", (int)strlen(topic));
    sim4g_send_raw(cmd, strlen(cmd));
    if (wait_for_prompt(">", 5000) != ESP_OK) goto fail;

    sim4g_send_raw(topic, strlen(topic)); 
    if (wait_for_prompt("OK", 5000) != ESP_OK) goto fail;

    snprintf(cmd, sizeof(cmd), "AT+CMQTTPAYLOAD=0,%d\r\n", (int)strlen(payload));
    sim4g_send_raw(cmd, strlen(cmd));
    if (wait_for_prompt(">", 5000) != ESP_OK) goto fail;

    sim4g_send_raw(payload, strlen(payload)); 
    if (wait_for_prompt("OK", 5000) != ESP_OK) goto fail;


    snprintf(cmd, sizeof(cmd), "AT+CMQTTPUB=0,%d,60\r\n", qos);
    sim4g_send_raw(cmd, strlen(cmd));
    

    if (wait_for_prompt("+CMQTTPUB: 0,0", 15000) != ESP_OK) {
        goto fail;
    }

    sim4g_unlock(); 
    ESP_LOGI(TAG, "MQTT publish success");
    return ESP_OK;

fail:
    sim4g_unlock(); 
    ESP_LOGE(TAG, "MQTT publish failed");
    return ESP_FAIL;
}

esp_err_t mqtt_disconnect(void)
{
    esp_err_t ret = ESP_OK;
    esp_err_t err;

    if (s_mqtt_connected) {
        err = sim4g_send_cmd("AT+CMQTTDISC=0,60", "OK", 10000);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "CMQTTDISC failed");
            ret = err;
        }
    }

    err = sim4g_send_cmd("AT+CMQTTREL=0", "OK", 5000);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "CMQTTREL failed");
        ret = err;
    }

    err = sim4g_send_cmd("AT+CMQTTSTOP", "OK", 10000);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "CMQTTSTOP failed");
        ret = err;
    }

    s_mqtt_connected = false;
    return ret;
}

bool mqtt_is_connected(void)
{
    return s_mqtt_connected;
}