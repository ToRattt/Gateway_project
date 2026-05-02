#ifndef BOARD_PINS_H
#define BOARD_PINS_H

#include "driver/gpio.h"
#include "driver/uart.h"

/* UART debug */
#define DEBUG_UART_PORT      UART_NUM_0
#define DEBUG_UART_TX_PIN    GPIO_NUM_1
#define DEBUG_UART_RX_PIN    GPIO_NUM_3

/* Modem 4G */
#define MODEM_UART_PORT      UART_NUM_1
#define MODEM_UART_TX_PIN    GPIO_NUM_17
#define MODEM_UART_RX_PIN    GPIO_NUM_16
#define MODEM_UART_BAUD_RATE        115200
#define MODEM_UART_RX_BUFFER_SIZE   2048
#define MODEM_UART_TX_BUFFER_SIZE   2048
#define MODEM_DEFAULT_TIMEOUT_MS    1000

#endif