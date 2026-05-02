#include "network_mrg.h"
#include "sim4g_a7680c.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "NETWORK_MRG";

static network_state_t s_state = NETWORK_STATE_IDLE;
static bool s_ready = false;

esp_err_t network_mrg_init(void)
{
    s_state = NETWORK_STATE_IDLE;
    s_ready = false;
    return ESP_OK;
}

esp_err_t network_mrg_check_all(const char *apn)
{
    esp_err_t err;

    err = sim4g_check_sim_ready();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SIM not ready");
        s_state = NETWORK_STATE_IDLE;
        s_ready = false;
        return err;
    }
    s_state = NETWORK_STATE_SIM_READY;

    err = sim4g_check_signal();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Signal check failed");
        s_ready = false;
        return err;
    }
    s_state = NETWORK_STATE_SIGNAL_OK;

    err = sim4g_check_attach();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Network attach failed");
        s_ready = false;
        return err;
    }
    s_state = NETWORK_STATE_ATTACHED;

    err = sim4g_set_apn(apn);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Set APN failed");
        s_ready = false;
        return err;
    }

    err = sim4g_activate_pdp();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Activate PDP failed");
        s_ready = false;
        return err;
    }
    s_state = NETWORK_STATE_PDP_ACTIVE;

    s_state = NETWORK_STATE_READY;
    s_ready = true;

    ESP_LOGI(TAG, "Network READY");
    return ESP_OK;
}

esp_err_t network_mrg_wait_ready(const char *apn, int retry, int delay_ms)
{
    if (apn == NULL || retry <= 0 || delay_ms <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t last_err = ESP_FAIL;

    for (int i = 0; i < retry; i++) {
        ESP_LOGI(TAG, "Network check attempt %d/%d", i + 1, retry);

        last_err = network_mrg_check_all(apn);
        if (last_err == ESP_OK) {
            return ESP_OK;
        }

        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }

    s_ready = false;
    return last_err;
}

bool network_mrg_is_ready(void)
{
    return s_ready;
}

network_state_t network_mrg_get_state(void)
{
    return s_state;
}