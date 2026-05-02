#include "board_init.h"
#include "board_pins.h"

#include "esp_log.h"

static const char *TAG = "BOARD";

esp_err_t board_init(void)
{
    ESP_LOGI(TAG, "Board init done");
    ESP_LOGI(TAG, "MODEM UART: port=%d tx=%d rx=%d",
             MODEM_UART_PORT,
             MODEM_UART_TX_PIN,
             MODEM_UART_RX_PIN);

    return ESP_OK;
}