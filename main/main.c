#include <time.h>
#include <string.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "nvs_flash.h"
#include "esp_err.h"
#include "esp_log.h"

#include "board_pins.h"
#include "app_config.h"
#include "uart_debug.h"
#include "sim4g_a7680c.h"
#include "network_mrg.h"
#include "mqtt.h"
#include "json_builder.h"
#include "ble_client.h"

static const char *TAG = "MAIN";

/* ─────────────────────────────────────────────────────────────
 * MAC → BLE ID
 * "10:00:3b:cb:d8:b6" → "ble_10003bcbd8b6"
 * ───────────────────────────────────────────────────────────── */
static void mac_to_ble_id(const char *mac, char *out, size_t out_size)
{
    char clean[13] = {0};
    int j = 0;
    for (int i = 0; mac[i] && j < 12; i++) {
        if (mac[i] != ':') clean[j++] = mac[i];
    }
    snprintf(out, out_size, "ble_%s", clean);
}

/* ─────────────────────────────────────────────────────────────
 * Track BLE devices đã register với ThingsBoard
 * ───────────────────────────────────────────────────────────── */
#define MAX_BLE_DEVICES 10
#define BLE_ID_LEN      20

static char s_registered_ids[MAX_BLE_DEVICES][BLE_ID_LEN] = {0};
static int  s_registered_count = 0;

static bool is_ble_registered(const char *ble_id)
{
    for (int i = 0; i < s_registered_count; i++) {
        if (strcmp(s_registered_ids[i], ble_id) == 0) return true;
    }
    return false;
}

static void mark_ble_registered(const char *ble_id)
{
    if (s_registered_count >= MAX_BLE_DEVICES) return;
    strncpy(s_registered_ids[s_registered_count], ble_id, BLE_ID_LEN - 1);
    s_registered_ids[s_registered_count][BLE_ID_LEN - 1] = '\0';
    s_registered_count++;
}

/* Build chuỗi "ble_xxx,ble_yyy,..." từ danh sách đã register */
static void build_ble_ids_string(char *out, size_t out_size)
{
    out[0] = '\0';
    for (int i = 0; i < s_registered_count; i++) {
        if (i > 0) strncat(out, ",", out_size - strlen(out) - 1);
        strncat(out, s_registered_ids[i], out_size - strlen(out) - 1);
    }
    if (out[0] == '\0') strncpy(out, "none", out_size);
}

/* Global RSSI — đọc bởi rssi_task, dùng bởi publish_task */
static int s_modem_signal = 0;

// Hàm helper
static esp_err_t publish_with_reconnect(const char *topic, const char *payload, int qos) {
    for (int i = 0; i < 3; i++) {
        if (!mqtt_is_connected()) {
            ESP_LOGW(TAG, "MQTT lost, reconnecting... (%d/3)", i+1);
            mqtt_connect(APP_MQTT_HOST, APP_MQTT_PORT,
                         APP_MQTT_CLIENT_ID,
                         APP_MQTT_USERNAME, APP_MQTT_PASSWORD);
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
        if (mqtt_publish(topic, payload, qos, 0) == ESP_OK) return ESP_OK;
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    return ESP_FAIL;
}


/* ─────────────────────────────────────────────────────────────
 * PUBLISH TASK
 * ───────────────────────────────────────────────────────────── */
static void publish_task(void *pvParameters)
{
    char      payload[512];
    char      ble_ids_str[128];
    char      ble_id[BLE_ID_LEN];
    uint64_t  last_heartbeat_ts_ms = 0;
    esp_err_t err;

    while (1) {
        // --- BƯỚC 1: KIỂM TRA KẾT NỐI MQTT (QUAN TRỌNG NHẤT) ---
        if (!mqtt_is_connected()) {
            ESP_LOGW(TAG, "MQTT connection lost, attempting to reconnect...");
            if (mqtt_connect(APP_MQTT_HOST, APP_MQTT_PORT, APP_MQTT_CLIENT_ID, 
                             APP_MQTT_USERNAME, APP_MQTT_PASSWORD) == ESP_OK) {
                // Sau khi reconnect thành công, reset để đăng ký lại Attributes
                s_registered_count = 0;
                memset(s_registered_ids, 0, sizeof(s_registered_ids));
                ESP_LOGI(TAG, "Reconnect OK, registration reset.");
            } else {
                vTaskDelay(pdMS_TO_TICKS(5000)); // Đợi 5s thử lại
                continue; 
            }
        }

        uint64_t now_ms = ((uint64_t)time(NULL)) * 1000ULL;

        /* Kiểm tra timestamp hợp lệ (đã đồng bộ NTP chưa) */
        if (now_ms < 1704067200000ULL) {
            ESP_LOGW(TAG, "Timestamp invalid, waiting...");
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        /* --- BƯỚC 2: CẬP NHẬT RSSI MODEM (Cho bài toán định vị) --- */
        sim4g_get_signal_strength(&s_modem_signal);

        /* --- BƯỚC 3: XỬ LÝ DỮ LIỆU BLE --- */
        ble_sensor_data_t ble_data = {0};
        if (ble_client_get_latest_data(&ble_data)) {
            // Chuyển MAC sang format ble_id trước khi kiểm tra đăng ký
            mac_to_ble_id(ble_data.ble_id, ble_id, sizeof(ble_id));

            /* Kiểm tra đăng ký thiết bị (Connect & Attributes) */
            if (!is_ble_registered(ble_id)) {
                ESP_LOGI(TAG, "Registering new BLE device: %s", ble_id);

                // 3.1 Gửi CONNECT
                json_build_gateway_connect(payload, sizeof(payload), ble_id, APP_PROFILE_BLE);
                if (mqtt_publish(APP_MQTT_TOPIC_GW_CONNECT, payload, 1, 0) == ESP_OK) {
                    vTaskDelay(pdMS_TO_TICKS(3000)); // Đợi Server xử lý tạo Device

                    // 3.2 Gửi ATTRIBUTES (Chỉ khi Connect thành công)
                    if (mqtt_is_connected()) {
                        json_build_ble_attributes(payload, sizeof(payload), ble_id, "PulseOx_v1", "1.0.0");
                        mqtt_publish(APP_MQTT_TOPIC_GW_ATTRIBUTES, payload, 1, 0);
                        vTaskDelay(pdMS_TO_TICKS(2000)); // Nghỉ để server lưu Attributes
                        mark_ble_registered(ble_id);
                    }
                }
                continue; // Vòng lặp sau mới bắt đầu gửi Telemetry
            }

            /* 3.3 Gửi BLE Telemetry (Định vị RSSI & Cảm biến) */
            err = json_build_ble_telemetry(payload, sizeof(payload),
                                            ble_id, APP_GATEWAY_ID, now_ms,
                                            ble_data.seq, ble_data.rssi,
                                            ble_data.spo2, ble_data.pulse, ble_data.status_code);
            if (err == ESP_OK) {
                mqtt_publish(APP_MQTT_TOPIC_GW_TELEMETRY, payload, 0, 0);
                vTaskDelay(pdMS_TO_TICKS(1000)); 
            }
        }

        /* --- BƯỚC 4: GATEWAY HEARTBEAT --- */
        if ((now_ms - last_heartbeat_ts_ms) >= APP_HEARTBEAT_INTERVAL_MS || last_heartbeat_ts_ms == 0) {
            build_ble_ids_string(ble_ids_str, sizeof(ble_ids_str));
            err = json_build_gateway_heartbeat(payload, sizeof(payload),
                                               APP_GATEWAY_ID, now_ms, "online",
                                               s_modem_signal, s_registered_count, ble_ids_str);
            if (err == ESP_OK) {
                mqtt_publish(APP_MQTT_TOPIC_GW_TELEMETRY, payload, 0, 0);
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
            last_heartbeat_ts_ms = now_ms;
        }

        vTaskDelay(pdMS_TO_TICKS(APP_TELEMETRY_INTERVAL_MS));
    }
}

/* ─────────────────────────────────────────────────────────────
 * APP MAIN
 * ───────────────────────────────────────────────────────────── */
void app_main(void)
{
    /* NVS init */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    char payload[512];

    sim4g_config_t modem_cfg = {
        .uart_port          = MODEM_UART_PORT,
        .tx_pin             = MODEM_UART_TX_PIN,
        .rx_pin             = MODEM_UART_RX_PIN,
        .baud_rate          = MODEM_UART_BAUD_RATE,
        .rx_buffer_size     = MODEM_UART_RX_BUFFER_SIZE,
        .tx_buffer_size     = MODEM_UART_TX_BUFFER_SIZE,
        .default_timeout_ms = MODEM_DEFAULT_TIMEOUT_MS,
    };

    ESP_ERROR_CHECK(uart_debug_init());
    ESP_LOGI(TAG, "Gateway booting...");

    /* Hardware init */
    ESP_ERROR_CHECK(sim4g_init(&modem_cfg));
    ESP_ERROR_CHECK(sim4g_basic_check());
    ESP_ERROR_CHECK(network_mrg_init());
    ESP_ERROR_CHECK(network_mrg_wait_ready(APP_MODEM_APN, 10, 3000));

    /* Delay để mạng ổn định */
    ESP_LOGI(TAG, "Waiting for network to stabilize...");
    vTaskDelay(pdMS_TO_TICKS(3000));

    /* ── Time sync — thử NTP trước, fallback NITZ ─────────── */
    bool s_time_synced = false;

    for (int i = 0; i < 3 && !s_time_synced; i++) {
        ESP_LOGI(TAG, "NTP sync attempt %d/3...", i + 1);
        if (sim4g_sync_time_ntp("pool.ntp.org", 28) == ESP_OK) {
            uint64_t ts_ms = 0;
            if (sim4g_get_unix_time_ms(&ts_ms) == ESP_OK
                && ts_ms > 1704067200000ULL) {
                struct timeval tv = { .tv_sec = (time_t)(ts_ms / 1000ULL) };
                settimeofday(&tv, NULL);
                s_time_synced = true;
                ESP_LOGI(TAG, "Time synced via NTP: %llu ms",
                         (unsigned long long)ts_ms);
            }
        }
        if (!s_time_synced) {
            ESP_LOGW(TAG, "NTP attempt %d failed, retrying...", i + 1);
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
    }

    /* Fallback: NITZ từ operator nếu NTP fail */
    if (!s_time_synced) {
        ESP_LOGW(TAG, "NTP failed — trying NITZ from operator...");
        for (int i = 0; i < 5 && !s_time_synced; i++) {
            uint64_t ts_ms = 0;
            if (sim4g_get_unix_time_ms(&ts_ms) == ESP_OK
                && ts_ms > 1704067200000ULL) {
                struct timeval tv = { .tv_sec = (time_t)(ts_ms / 1000ULL) };
                settimeofday(&tv, NULL);
                s_time_synced = true;
                ESP_LOGI(TAG, "Time synced via NITZ: %llu ms",
                         (unsigned long long)ts_ms);
            } else {
                ESP_LOGW(TAG, "NITZ attempt %d/5 failed...", i + 1);
                vTaskDelay(pdMS_TO_TICKS(2000));
            }
        }
    }

    if (!s_time_synced) {
        ESP_LOGE(TAG, "Time sync failed — timestamp will be invalid!");
    }

    /* ── MQTT + BLE init ──────────────────────────────────── */
    ESP_ERROR_CHECK(mqtt_init());
    ESP_ERROR_CHECK(mqtt_connect(APP_MQTT_HOST, APP_MQTT_PORT,
                                 APP_MQTT_CLIENT_ID,
                                 APP_MQTT_USERNAME, APP_MQTT_PASSWORD));
    ESP_ERROR_CHECK(ble_client_init());

    /* ── Đăng ký Gateway ─────────────────────────────────── */
    json_build_gateway_attributes(payload, sizeof(payload),
        APP_GATEWAY_MODEL,
        APP_GATEWAY_FW_VERSION,
        APP_GATEWAY_DEPLOYMENT_SITE,
        APP_GATEWAY_ROOM);

    if (publish_with_reconnect(APP_MQTT_TOPIC_GW_SELF_ATTRIBUTES, payload, 1) == ESP_OK) {
        ESP_LOGI(TAG, "Gateway attributes OK");
    } else {
        ESP_LOGE(TAG, "Gateway attributes FAILED");
    }
    vTaskDelay(pdMS_TO_TICKS(2000));

    /* Start tasks — SAU KHI publish xong */
    xTaskCreate(publish_task, "publish_task", 4096, NULL, 5, NULL);

    /* Giữ app_main sống */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(60000));
    }
}