#ifndef JSON_BUILDER_H
#define JSON_BUILDER_H

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

const char *json_status_code_to_string(uint8_t status_code);

esp_err_t json_build_ble_telemetry(char *out,
                                   size_t out_size,
                                   const char *ble_id,
                                   const char *gateway_id,
                                   uint64_t ts_ms,
                                   uint16_t seq,
                                   int rssi,
                                   int spo2,
                                   int pulse,
                                   uint8_t status_code);

esp_err_t json_build_gateway_heartbeat(char *out,
                                       size_t out_size,
                                       const char *gateway_id,
                                       uint64_t ts_ms,
                                       const char *status,
                                       int modem_signal,
                                       int ble_count,
                                       const char *ble_ids);

esp_err_t json_build_gateway_connect(char *out,
                                     size_t out_size,
                                     const char *device_name,
                                     const char *device_type);

esp_err_t json_build_gateway_attributes(char *out, size_t out_size,
                                        const char *model,
                                        const char *fw_version,
                                        const char *deployment_site,
                                        const char *room);

esp_err_t json_build_ble_attributes(char *out,
                                    size_t out_size,
                                    const char *ble_id,
                                    const char *device_model,
                                    const char *fw_version);

esp_err_t json_build_handover_event(char *out,
                                    size_t out_size,
                                    const char *gateway_id,
                                    uint64_t ts_ms,
                                    const char *event,
                                    const char *ble_id,
                                    int rssi,
                                    const char *prev_gateway);

#ifdef __cplusplus
}
#endif

#endif