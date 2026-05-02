#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "driver/gpio.h"
#include "driver/uart.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =========================
 * Device Profiles 
 * ========================= */
#define APP_PROFILE_GATEWAY    "Gateway_Profile"
#define APP_PROFILE_BLE        "BLE_Health_Device"
/* =========================
 * Gateway info
 * ========================= */
#define APP_GATEWAY_ID               "gw_02"
#define APP_GATEWAY_MODEL            "ESP32_A7680C"
#define APP_GATEWAY_FW_VERSION       "1.0.0"
#define APP_GATEWAY_DEPLOYMENT_SITE  "Lab_HUST"
#define APP_GATEWAY_ROOM             "Room_A"
/* =========================
 * Network info
 * ========================= */
#define APP_NETWORK_TYPE           "4G"

/* =========================
 * APN config
 * ========================= */
#define APP_MODEM_APN              "m3-world"

/* =========================
 * MQTT broker config
 * ========================= */
#define APP_MQTT_HOST              "mqtt.thingsboard.cloud"
#define APP_MQTT_PORT              1883

#define APP_MQTT_CLIENT_ID         APP_GATEWAY_ID "_client"
#define APP_MQTT_USERNAME          "Jppae3WqwLiTAbgsEnTd"
#define APP_MQTT_PASSWORD          ""

/* =========================
 * MQTT topics (ThingsBoard)
 * ========================= */
#define APP_MQTT_TOPIC_GW_CONNECT     "v1/gateway/connect"
#define APP_MQTT_TOPIC_GW_DISCONNECT  "v1/gateway/disconnect"
#define APP_MQTT_TOPIC_GW_TELEMETRY   "v1/gateway/telemetry"
#define APP_MQTT_TOPIC_GW_ATTRIBUTES  "v1/gateway/attributes"
#define APP_MQTT_TOPIC_GW_SELF_ATTRIBUTES "v1/devices/me/attributes"

/* =========================    
 * Publish intervals
 * ========================= */
#define APP_TELEMETRY_INTERVAL_MS     5000
#define APP_HEARTBEAT_INTERVAL_MS     30000

/* =========================
 * JSON buffer size
 * ========================= */
#define APP_JSON_BUFFER_SIZE          512

#ifdef __cplusplus
}
#endif

#endif