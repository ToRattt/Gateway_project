#ifndef MQTT_H
#define MQTT_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t mqtt_init(void);
esp_err_t mqtt_connect(const char *host,
                       int port,
                       const char *client_id,
                       const char *username,
                       const char *password);

esp_err_t mqtt_publish(const char *topic,
                       const char *payload,
                       int qos,
                       int retain);

esp_err_t mqtt_disconnect(void);
bool mqtt_is_connected(void);

#ifdef __cplusplus
}
#endif

#endif