#include "uart_debug.h"

#include "esp_log.h"

static const char *TAG = "UART_DEBUG";

esp_err_t uart_debug_init(void)
{
    ESP_LOGI(TAG, "UART debug init done");
    return ESP_OK;
}