#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char ble_id[18];      
    uint8_t ble_addr[6];
    uint16_t seq;
    int rssi;
    int spo2;
    int pulse;
    uint8_t status_code;
} ble_sensor_data_t;

esp_err_t ble_client_init(void);

bool ble_client_get_latest_data(ble_sensor_data_t *out_data);

#ifdef __cplusplus
}
#endif