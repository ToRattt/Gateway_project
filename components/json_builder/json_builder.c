#include "json_builder.h"

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "esp_log.h"


const char *json_status_code_to_string(uint8_t status_code)
{
    switch (status_code) {
        case 0x00: return "ok";
        case 0x01: return "low_battery";
        case 0x02: return "sensor_error";
        case 0x03: return "out_of_range";
        case 0xFF: return "unknown_error";
        default:   return "unknown_error";
    }
}

esp_err_t json_build_ble_telemetry(char *out, size_t out_size, const char *ble_id, const char *gateway_id, 
                                   uint64_t ts_ms, uint16_t seq, int rssi, int spo2, int pulse, uint8_t status_code)
{
    const char *status_str = json_status_code_to_string(status_code);
    
    // Đã loại bỏ device_ts, sử dụng ts của gateway/server làm mốc chính
    int written = snprintf(out, out_size,
        "{\"%s\":[{"
            "\"ts\":%" PRIu64 ","
            "\"values\":{"
                "\"gateway_id\":\"%s\","
                "\"seq\":%u,"
                "\"rssi\":%d,"
                "\"spo2\":%d,"
                "\"pulse\":%d,"
                "\"status\":\"%s\""
            "}"
        "}]}",
        ble_id, ts_ms, gateway_id, (unsigned int)seq, rssi, spo2, pulse, status_str);
        
    return (written > 0 && (size_t)written < out_size) ? ESP_OK : ESP_FAIL;
}

esp_err_t json_build_gateway_heartbeat(char *out,
                                       size_t out_size,
                                       const char *gateway_id,
                                       uint64_t ts_ms,
                                       const char *status,
                                       int modem_signal,
                                       int ble_count,
                                       const char *ble_ids)
{
    int written = snprintf(out, out_size,
        "{\"%s\":[{\"ts\":%" PRIu64 ",\"values\":{"
        "\"alive\":true,"
        "\"status\":\"%s\","
        "\"network_type\":\"4g\","
        "\"modem_signal\":%d,"
        "\"connected_ble_count\":%d,"
        "\"connected_ble_ids\":\"%s\""
        "}}]}",
        gateway_id, ts_ms, status, modem_signal, ble_count, ble_ids);

    return (written > 0 && (size_t)written < out_size) ? ESP_OK : ESP_FAIL;
}

esp_err_t json_build_gateway_connect(char *out,
                                     size_t out_size,
                                     const char *device_name,
                                     const char *device_type)
{
    int written = snprintf(out, out_size,
        "{\"device\":\"%s\",\"type\":\"%s\"}",
        device_name, device_type);

    return (written > 0 && (size_t)written < out_size) ? ESP_OK : ESP_FAIL;
}

esp_err_t json_build_gateway_attributes(char *out,
                                        size_t out_size,
                                        const char *model,
                                        const char *fw_version,
                                        const char *deployment_site,
                                        const char *room)
{
    int written = snprintf(out, out_size,
        "{"
        "\"gateway_model\":\"%s\","
        "\"fw_version\":\"%s\","
        "\"deployment_site\":\"%s\","
        "\"room\":\"%s\""
        "}",
        model, fw_version, deployment_site, room);

    return (written > 0 && (size_t)written < out_size) ? ESP_OK : ESP_FAIL;
}

esp_err_t json_build_ble_attributes(char *out,
                                    size_t out_size,
                                    const char *ble_id,
                                    const char *device_model,
                                    const char *fw_version)
{
    int written = snprintf(out, out_size,
        "{\"%s\":{"
        "\"device_model\":\"%s\","
        "\"fw_version\":\"%s\""
        "}}",
        ble_id, device_model, fw_version);

    return (written > 0 && (size_t)written < out_size) ? ESP_OK : ESP_FAIL;
}

esp_err_t json_build_handover_event(char *out,
                                    size_t out_size,
                                    const char *gateway_id,
                                    uint64_t ts_ms,
                                    const char *event,
                                    const char *ble_id,
                                    int rssi,
                                    const char *prev_gateway)
{
    int written;

    if (prev_gateway != NULL) {
        written = snprintf(out, out_size,
            "{\"%s\":[{\"ts\":%" PRIu64 ",\"values\":{"
            "\"event\":\"%s\","
            "\"ble_id\":\"%s\","
            "\"rssi\":%d,"
            "\"prev_gateway\":\"%s\""
            "}}]}",
            gateway_id, ts_ms, event, ble_id, rssi, prev_gateway);
    } else {
        written = snprintf(out, out_size,
            "{\"%s\":[{\"ts\":%" PRIu64 ",\"values\":{"
            "\"event\":\"%s\","
            "\"ble_id\":\"%s\""
            "}}]}",
            gateway_id, ts_ms, event, ble_id);
    }

    return (written > 0 && (size_t)written < out_size) ? ESP_OK : ESP_FAIL;
}