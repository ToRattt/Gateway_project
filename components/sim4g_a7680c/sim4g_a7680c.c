#include "sim4g_a7680c.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "freertos/semphr.h"
#include "driver/uart.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "SIM4G_A7680C";

static sim4g_config_t s_cfg;
static bool s_inited = false;
static SemaphoreHandle_t s_uart_mutex = NULL;   

// 1. Bỏ từ khóa 'static' để mqtt.c có thể gọi được
esp_err_t sim4g_wait_response(char *out, size_t out_size, uint32_t timeout_ms)
{
    if (out == NULL || out_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(out, 0, out_size);
    size_t total = 0;
    TickType_t start = xTaskGetTickCount();

    while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(timeout_ms)) {
        int len = uart_read_bytes(
            s_cfg.uart_port,
            (uint8_t *)&out[total],
            out_size - total - 1,
            pdMS_TO_TICKS(50) // Giảm xuống 50ms để phản hồi nhạy hơn
        );

        if (len > 0) {
            total += (size_t)len;
            out[total] = '\0';

            // 2. Sửa điều kiện kiểm tra: Nới lỏng strstr để nhận diện OK/ERROR linh hoạt hơn
            if (strstr(out, "OK") != NULL ||
                strstr(out, "ERROR") != NULL ||
                strstr(out, ">") != NULL) {
                return ESP_OK;
            }

            if (total >= out_size - 1) {
                return ESP_OK;
            }
        }
    }

    return (total > 0) ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t sim4g_init(const sim4g_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    s_cfg = *config;

    uart_config_t uart_config = {
        .baud_rate = s_cfg.baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
                .source_clk = UART_SCLK_DEFAULT,
        #endif
    };

    esp_err_t err = uart_driver_install(
        s_cfg.uart_port,
        s_cfg.rx_buffer_size,
        s_cfg.tx_buffer_size,
        0,
        NULL,
        0
    );
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_driver_install failed: %s", esp_err_to_name(err));
        return err;
    }

    err = uart_param_config(s_cfg.uart_port, &uart_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_param_config failed: %s", esp_err_to_name(err));
        uart_driver_delete(s_cfg.uart_port);
        return err;
    }

    err = uart_set_pin(
        s_cfg.uart_port,
        s_cfg.tx_pin,
        s_cfg.rx_pin,
        UART_PIN_NO_CHANGE,
        UART_PIN_NO_CHANGE
    );
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_set_pin failed: %s", esp_err_to_name(err));
        uart_driver_delete(s_cfg.uart_port);
        return err;
    }

    uart_flush(s_cfg.uart_port);
    s_uart_mutex = xSemaphoreCreateRecursiveMutex();
    if (s_uart_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create UART mutex");
        uart_driver_delete(s_cfg.uart_port);
        return ESP_ERR_NO_MEM;
    }
    s_inited = true;

    ESP_LOGI(TAG, "Init OK: uart=%d tx=%d rx=%d baud=%d",
             s_cfg.uart_port, s_cfg.tx_pin, s_cfg.rx_pin, s_cfg.baud_rate);

    vTaskDelay(pdMS_TO_TICKS(300));
    return ESP_OK;
}

esp_err_t sim4g_deinit(void)
{
    if (!s_inited) {
        return ESP_OK;
    }

    esp_err_t err = uart_driver_delete(s_cfg.uart_port);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_driver_delete failed: %s", esp_err_to_name(err));
        return err;
    }

    s_inited = false;
    return ESP_OK;
}



void sim4g_flush(void)
{
    if (s_inited) {
        uart_flush(s_cfg.uart_port);
    }
}

int sim4g_send_raw(const char *data, size_t len)
{
    if (!s_inited || data == NULL || len == 0) {
        return -1;
    }

    return uart_write_bytes(s_cfg.uart_port, data, len);
}

int sim4g_read(char *out, size_t out_size, uint32_t timeout_ms)
{
    if (!s_inited || out == NULL || out_size == 0) {
        return -1;
    }

    memset(out, 0, out_size);

    int len = uart_read_bytes(
        s_cfg.uart_port,
        (uint8_t *)out,
        out_size - 1,
        pdMS_TO_TICKS(timeout_ms)
    );

    if (len > 0) {
        out[len] = '\0';
    }

    return len;
}

esp_err_t sim4g_send_cmd(const char *cmd, const char *expect, uint32_t timeout_ms)
{
    if (!s_inited) return ESP_ERR_INVALID_STATE;
    if (cmd == NULL) return ESP_ERR_INVALID_ARG;

    if (xSemaphoreTakeRecursive(s_uart_mutex, pdMS_TO_TICKS(10000)) != pdTRUE) {
        ESP_LOGE(TAG, "UART mutex timeout on cmd: %s", cmd);
        return ESP_ERR_TIMEOUT;
    }

    char resp[512];
    uart_flush(s_cfg.uart_port);
    ESP_LOGI(TAG, ">> %s", cmd);
    uart_write_bytes(s_cfg.uart_port, cmd, strlen(cmd));
    uart_write_bytes(s_cfg.uart_port, "\r\n", 2);

    esp_err_t err = sim4g_wait_response(resp, sizeof(resp), timeout_ms);

    xSemaphoreGiveRecursive(s_uart_mutex);  // luôn trả mutex dù lỗi

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "<< timeout/no response");
        return err;
    }

    ESP_LOGI(TAG, "<< %s", resp);

    if (expect == NULL) {
        return (strstr(resp, "ERROR") == NULL) ? ESP_OK : ESP_FAIL;
    }

    return (strstr(resp, expect) != NULL) ? ESP_OK : ESP_FAIL;
}

esp_err_t sim4g_basic_check(void)
{
    esp_err_t err = sim4g_send_cmd("AT", "OK", s_cfg.default_timeout_ms);
    if (err != ESP_OK) {
        return err;
    }

    return sim4g_send_cmd("ATE0", "OK", s_cfg.default_timeout_ms);
}

esp_err_t sim4g_check_sim_ready(void)
{
    return sim4g_send_cmd("AT+CPIN?", "READY", 3000);
}

esp_err_t sim4g_check_signal(void)
{
    return sim4g_send_cmd("AT+CSQ", "+CSQ", 3000);
}

esp_err_t sim4g_get_operator(void)
{
    return sim4g_send_cmd("AT+COPS?", "+COPS", 5000);
}

esp_err_t sim4g_check_attach(void)
{
    return sim4g_send_cmd("AT+CGATT?", "+CGATT: 1", 5000);
}

esp_err_t sim4g_set_apn(const char *apn)
{
    if (apn == NULL || strlen(apn) == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    char cmd[128];
    snprintf(cmd, sizeof(cmd), "AT+CGDCONT=1,\"IP\",\"%s\"", apn);

    return sim4g_send_cmd(cmd, "OK", 5000);
}

esp_err_t sim4g_activate_pdp(void)
{
    return sim4g_send_cmd("AT+CGACT=1,1", "OK", 10000);
}

uart_port_t sim4g_get_uart_port(void)
{
    return s_cfg.uart_port;
}

esp_err_t sim4g_sync_time_ntp(const char *server, int timezone_quarter)
{
    char cmd[128];
    esp_err_t err;

    snprintf(cmd, sizeof(cmd), "AT+CNTP=\"%s\",%d", server, timezone_quarter);
    err = sim4g_send_cmd(cmd, "OK", 5000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Set NTP server failed");
        return err;
    }

    // Bước 1: Gửi AT+CNTP, chờ OK
    err = sim4g_send_cmd("AT+CNTP", "OK", 5000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "CNTP command failed");
        return err;
    }

    // Bước 2: Chờ riêng URC "+CNTP: 0" — module gửi sau vài giây
    char urc[128] = {0};
    ESP_LOGI(TAG, "Waiting for +CNTP URC...");
    esp_err_t urc_err = sim4g_wait_response(urc, sizeof(urc), 10000); // chờ tối đa 10s
    ESP_LOGI(TAG, "URC received: [%s]", urc);

    if (urc_err != ESP_OK || strstr(urc, "+CNTP: 0") == NULL) {
        ESP_LOGE(TAG, "NTP URC not received or failed: %s", urc);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Time sync OK");
    return ESP_OK;
}

esp_err_t sim4g_get_clock(char *out, size_t out_size)
{
    if (!s_inited || out == NULL || out_size == 0) return ESP_ERR_INVALID_ARG;

    // --- BẮT ĐẦU BỌC LOCK ---
    if (sim4g_lock(5000) != ESP_OK) {
        ESP_LOGE(TAG, "Cannot lock UART for get_clock");
        return ESP_ERR_TIMEOUT;
    }

    uart_flush_input(s_cfg.uart_port);
    uart_write_bytes(s_cfg.uart_port, "AT+CCLK?\r\n", 10);

    esp_err_t err = sim4g_wait_response(out, out_size, 5000);
    
    sim4g_unlock(); 
    // --- KẾT THÚC BỌC UNLOCK ---

    if (err != ESP_OK) return err;
    return (strstr(out, "+CCLK:") != NULL) ? ESP_OK : ESP_FAIL;
}

static int64_t days_from_civil(int y, unsigned m, unsigned d)
{
    y -= (m <= 2);
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (int)doe - 719468;
}

esp_err_t sim4g_get_unix_time_ms(uint64_t *ts_ms)
{
    if (!s_inited || ts_ms == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    char resp[128] = {0};
    esp_err_t err = sim4g_get_clock(resp, sizeof(resp));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "sim4g_get_clock failed: %s", esp_err_to_name(err));
        return err;
    }

    int yy = 0, mo = 0, dd = 0;
    int hh = 0, mm = 0, ss = 0;
    int tz_quarter = 0;
    char tz_sign = '+';

    char *p = strstr(resp, "+CCLK:");
    if (p == NULL) {
        ESP_LOGE(TAG, "CCLK not found in response: %s", resp);
        return ESP_FAIL;
    }

    int parsed = sscanf(p,
                        "+CCLK: \"%d/%d/%d,%d:%d:%d%c%d\"",
                        &yy, &mo, &dd,
                        &hh, &mm, &ss,
                        &tz_sign, &tz_quarter);

    if (parsed != 8) {
        ESP_LOGE(TAG, "CCLK parse failed, parsed=%d, resp=%s", parsed, resp);
        return ESP_FAIL;
    }

    int year = 2000 + yy;
    if (year < 2024 || year > 2035 ||  // ← chặn năm 2070
        mo < 1 || mo > 12 ||
        dd < 1 || dd > 31 ||
        hh < 0 || hh > 23 ||
        mm < 0 || mm > 59 ||
        ss < 0 || ss > 59 ||
        (tz_sign != '+' && tz_sign != '-')) {
        ESP_LOGE(TAG, "Invalid or unsynced time: %d/%d/%d", year, mo, dd);
        return ESP_FAIL;  // Trả về lỗi thay vì dùng thời gian sai
    }

    int64_t days = days_from_civil(year, (unsigned)mo, (unsigned)dd);
    int64_t local_epoch =
        days * 86400LL +
        hh * 3600LL +
        mm * 60LL +
        ss;

    int32_t tz_offset_sec = tz_quarter * 15 * 60;
    int64_t utc_epoch = (tz_sign == '+')
                        ? (local_epoch - tz_offset_sec)
                        : (local_epoch + tz_offset_sec);

    if (utc_epoch < 0) {
        ESP_LOGE(TAG, "Invalid epoch after timezone adjust: %lld", (long long)utc_epoch);
        return ESP_FAIL;
    }

    *ts_ms = (uint64_t)utc_epoch * 1000ULL;

    ESP_LOGI(TAG,
             "CCLK parsed: %02d/%02d/%02d %02d:%02d:%02d %c%02d",
             yy, mo, dd, hh, mm, ss, tz_sign, tz_quarter);
    ESP_LOGI(TAG, "Unix time UTC: %llu ms", (unsigned long long)*ts_ms);

    return ESP_OK;
}

esp_err_t sim4g_lock(uint32_t timeout_ms)
{
    if (s_uart_mutex == NULL) return ESP_ERR_INVALID_STATE;
    // BẮT BUỘC dùng TakeRecursive
    if (xSemaphoreTakeRecursive(s_uart_mutex, pdMS_TO_TICKS(timeout_ms)) == pdTRUE) {
        return ESP_OK;
    }
    return ESP_ERR_TIMEOUT;
}

esp_err_t sim4g_unlock(void)
{
    if (s_uart_mutex == NULL) return ESP_ERR_INVALID_STATE;
    // BẮT BUỘC dùng GiveRecursive
    if (xSemaphoreGiveRecursive(s_uart_mutex) == pdTRUE) {
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t sim4g_get_signal_strength(int *rssi) {
    char resp[64];
    
    // --- BẮT ĐẦU BỌC LOCK ---
    if (sim4g_lock(3000) != ESP_OK) return ESP_ERR_TIMEOUT;

    // Gửi lệnh qua hàm send_cmd (hàm này có mutex nội bộ nhưng dùng Recursive nên vẫn an toàn)
    esp_err_t err = sim4g_send_cmd("AT+CSQ", "OK", 2000);
    
    if (err == ESP_OK) {
        sim4g_read(resp, sizeof(resp), 1000); // Đọc nội dung trả về
        char *p = strstr(resp, "+CSQ:");
        if (p) {
            int csq = atoi(p + 6);
            if (csq == 99) *rssi = -113;
            else *rssi = -113 + (2 * csq);
        } else {
            err = ESP_FAIL;
        }
    }
    
    sim4g_unlock();
    // --- KẾT THÚC BỌC UNLOCK ---
    
    return err;
}

int sim4g_send_payload_raw(const char *data, size_t len)
{
    if (data == NULL || len == 0) return -1;
    // Giả sử Mutex đã được lấy ở tầng mqtt trước khi gọi hàm này
    return uart_write_bytes(s_cfg.uart_port, data, len);
}