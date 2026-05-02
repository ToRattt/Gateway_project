#ifndef SIM4G_A7680C_H
#define SIM4G_A7680C_H

#include <stddef.h>
#include <stdint.h>
#include "driver/uart.h"
#include "esp_err.h"


#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uart_port_t uart_port;
    int tx_pin;
    int rx_pin;
    int baud_rate;
    int rx_buffer_size;
    int tx_buffer_size;
    uint32_t default_timeout_ms;
} sim4g_config_t;

esp_err_t sim4g_get_signal_strength(int *rssi);
esp_err_t sim4g_lock(uint32_t timeout_ms);
esp_err_t sim4g_unlock(void);
esp_err_t sim4g_init(const sim4g_config_t *config);
esp_err_t sim4g_deinit(void);

esp_err_t sim4g_send_cmd(const char *cmd, const char *expect, uint32_t timeout_ms);
int sim4g_send_raw(const char *data, size_t len);
int sim4g_read(char *out, size_t out_size, uint32_t timeout_ms);
void sim4g_flush(void);

esp_err_t sim4g_wait_response(char *out, size_t out_size, uint32_t timeout_ms);
esp_err_t sim4g_basic_check(void);
esp_err_t sim4g_check_sim_ready(void);
esp_err_t sim4g_check_signal(void);
esp_err_t sim4g_get_operator(void);
esp_err_t sim4g_check_attach(void);
esp_err_t sim4g_set_apn(const char *apn);
esp_err_t sim4g_activate_pdp(void);

/* Time sync / clock */
esp_err_t sim4g_sync_time_ntp(const char *server, int timezone_quarter);
esp_err_t sim4g_get_clock(char *out, size_t out_size);
esp_err_t sim4g_get_unix_time_ms(uint64_t *ts_ms);

uart_port_t sim4g_get_uart_port(void);

#ifdef __cplusplus
}
#endif

#endif