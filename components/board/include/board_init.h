#ifndef BOARD_INIT_H
#define BOARD_INIT_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t board_init(void);
esp_err_t board_modem_reset_pin_init(void);

#ifdef __cplusplus
}
#endif

#endif