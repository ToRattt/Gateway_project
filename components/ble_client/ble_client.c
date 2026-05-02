#include "ble_client.h"

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_common_api.h"

#define TAG "BLE_CLIENT"

#define PROFILE_APP_ID 0
#define REMOTE_DEVICE_NAME "PulseOx_Node"

#define PULSEOX_SERVICE_UUID   0xFF00
#define PULSEOX_MEAS_CHAR_UUID 0xFF01

static esp_gatt_if_t client_if;
static uint16_t conn_id;
static esp_bd_addr_t remote_bda;

static uint16_t service_start_handle = 0;
static uint16_t service_end_handle = 0;
static uint16_t meas_char_handle = 0;
static bool service_found = false;
static ble_sensor_data_t latest_data;
static bool latest_data_valid = false;
static int latest_rssi = 0;

typedef struct __attribute__((packed)) {
    uint16_t seq;
    uint8_t  spo2;
    uint8_t  pulse;
    uint8_t  status;
} ble_tel_t;

static esp_bt_uuid_t meas_char_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = PULSEOX_MEAS_CHAR_UUID}
};

static esp_bt_uuid_t notify_descr_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG}
};

/* GAP */
static void gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {

    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
        esp_ble_gap_start_scanning(30);
        break;

    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
        esp_ble_gap_cb_param_t *scan = param;

        if (scan->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT) {

            uint8_t len;
            uint8_t *name = esp_ble_resolve_adv_data(
                scan->scan_rst.ble_adv,
                ESP_BLE_AD_TYPE_NAME_CMPL,
                &len
            );

            if (name && strncmp((char *)name, REMOTE_DEVICE_NAME, len) == 0) {

                latest_rssi = scan->scan_rst.rssi;   // ← thêm dòng này

                ESP_LOGI(TAG, "Found device, RSSI=%d, connecting...", latest_rssi);

                esp_ble_gap_stop_scanning();

                memcpy(remote_bda, scan->scan_rst.bda, sizeof(esp_bd_addr_t));

                esp_ble_gattc_open(
                    client_if,
                    scan->scan_rst.bda,
                    scan->scan_rst.ble_addr_type,
                    true
                );
            }
        }
        break;
    }

    default:
        break;
    }
}

/* GATTC */
static void gattc_cb(esp_gattc_cb_event_t event,
                     esp_gatt_if_t gattc_if,
                     esp_ble_gattc_cb_param_t *param)
{
    switch (event) {

    case ESP_GATTC_REG_EVT:
        client_if = gattc_if;

        esp_ble_gap_set_scan_params(&(esp_ble_scan_params_t){
            .scan_type = BLE_SCAN_TYPE_ACTIVE,
            .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
            .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
            .scan_interval = 0x50,
            .scan_window = 0x30
        });
        break;

    case ESP_GATTC_CONNECT_EVT:
        ESP_LOGI(TAG, "Connected");
        conn_id = param->connect.conn_id;

        esp_ble_gattc_search_service(gattc_if, conn_id, NULL);
        break;

    case ESP_GATTC_SEARCH_RES_EVT:
        ESP_LOGI(TAG, "Service found: %04x",
                 param->search_res.srvc_id.uuid.uuid.uuid16);

        if (param->search_res.srvc_id.uuid.len == ESP_UUID_LEN_16 &&
            param->search_res.srvc_id.uuid.uuid.uuid16 == PULSEOX_SERVICE_UUID) {

            ESP_LOGI(TAG, "PulseOx service found");

            service_found = true;
            service_start_handle = param->search_res.start_handle;
            service_end_handle = param->search_res.end_handle;
        }
        break;

    case ESP_GATTC_SEARCH_CMPL_EVT: {
        ESP_LOGI(TAG, "Search complete");

        if (!service_found) {
            ESP_LOGE(TAG, "Service not found");
            break;
        }

        uint16_t count = 0;

        esp_ble_gattc_get_attr_count(
            gattc_if,
            conn_id,
            ESP_GATT_DB_CHARACTERISTIC,
            service_start_handle,
            service_end_handle,
            0,
            &count
        );

        esp_gattc_char_elem_t *char_elem =
            malloc(sizeof(esp_gattc_char_elem_t) * count);

        esp_ble_gattc_get_char_by_uuid(
            gattc_if,
            conn_id,
            service_start_handle,
            service_end_handle,
            meas_char_uuid,
            char_elem,
            &count
        );

        meas_char_handle = char_elem[0].char_handle;

        ESP_LOGI(TAG, "Char handle = 0x%04x", meas_char_handle);

        esp_ble_gattc_register_for_notify(
            gattc_if,
            remote_bda,
            meas_char_handle
        );

        free(char_elem);
        break;
    }

    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
        ESP_LOGI(TAG, "Notify registered");

        uint16_t notify_en = 1;

        esp_ble_gattc_write_char_descr(
            gattc_if,
            conn_id,
            param->reg_for_notify.handle + 1,
            sizeof(notify_en),
            (uint8_t *)&notify_en,
            ESP_GATT_WRITE_TYPE_RSP,
            ESP_GATT_AUTH_REQ_NONE
        );
        break;
    }

    case ESP_GATTC_NOTIFY_EVT: {
        if (param->notify.value_len < sizeof(ble_tel_t)) {
            ESP_LOGW(TAG, "Invalid notify length: %d", param->notify.value_len);
            break;
        }

        ble_tel_t pkt;
        memcpy(&pkt, param->notify.value, sizeof(pkt));

        memset(&latest_data, 0, sizeof(latest_data));

        // TỰ ĐỘNG LẤY MAC ĐỊNH DANH (Thay cho việc hardcode "ble_02")
        snprintf(latest_data.ble_id, sizeof(latest_data.ble_id), 
                 "%02x:%02x:%02x:%02x:%02x:%02x",
                 remote_bda[0], remote_bda[1], remote_bda[2], 
                 remote_bda[3], remote_bda[4], remote_bda[5]);

        latest_data.seq = pkt.seq;
        latest_data.spo2 = pkt.spo2;
        latest_data.pulse = pkt.pulse;
        latest_data.status_code = pkt.status;
        latest_data.rssi = latest_rssi;

        latest_data_valid = true;

        ESP_LOGI(TAG,
                "BLE RX [%s]: seq=%d, spo2=%d, pulse=%d, status=%d, rssi=%d",
                latest_data.ble_id,
                latest_data.seq,
                latest_data.spo2,
                latest_data.pulse,
                latest_data.status_code,
                latest_data.rssi);
        break;
    }

    default:
        break;
    }
}

/* INIT */
esp_err_t ble_client_init(void)
{
    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_BLE);

    esp_bluedroid_init();
    esp_bluedroid_enable();

    esp_ble_gap_register_callback(gap_cb);
    esp_ble_gattc_register_callback(gattc_cb);
    esp_ble_gattc_app_register(PROFILE_APP_ID);

    ESP_LOGI(TAG, "BLE client init done");

    return ESP_OK;
}

bool ble_client_get_latest_data(ble_sensor_data_t *out_data)
{
    if (out_data == NULL) {
        return false;
    }

    if (!latest_data_valid) {
        return false;
    }

    memcpy(out_data, &latest_data, sizeof(ble_sensor_data_t));

    latest_data_valid = false;

    return true;
}