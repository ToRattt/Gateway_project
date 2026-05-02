#ifndef NETWORK_MRG_H
#define NETWORK_MRG_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NETWORK_STATE_IDLE = 0,
    NETWORK_STATE_SIM_READY,
    NETWORK_STATE_SIGNAL_OK,
    NETWORK_STATE_ATTACHED,
    NETWORK_STATE_PDP_ACTIVE,
    NETWORK_STATE_READY
} network_state_t;

esp_err_t network_mrg_init(void);
esp_err_t network_mrg_check_all(const char *apn);
esp_err_t network_mrg_wait_ready(const char *apn, int retry, int delay_ms);

bool network_mrg_is_ready(void);
network_state_t network_mrg_get_state(void);

#ifdef __cplusplus
}
#endif

#endif