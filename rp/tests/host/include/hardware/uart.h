#ifndef TEST_HARDWARE_UART_H
#define TEST_HARDWARE_UART_H

#include <stdbool.h>
#include <stdint.h>

#include "pico/stdlib.h"

typedef struct uart_inst {
    unsigned int id;
} uart_inst_t;

typedef enum uart_parity {
    UART_PARITY_NONE = 0
} uart_parity_t;

extern uart_inst_t fake_uart1_instance;

#define uart1 (&fake_uart1_instance)

uint uart_init(uart_inst_t *uart, uint baudrate);
void uart_set_format(uart_inst_t *uart,
                     uint data_bits,
                     uint stop_bits,
                     uart_parity_t parity);
void uart_set_fifo_enabled(uart_inst_t *uart, bool enabled);
void uart_set_irqs_enabled(uart_inst_t *uart, bool rx_has_data, bool tx_needs_data);
bool uart_is_readable(uart_inst_t *uart);
char uart_getc(uart_inst_t *uart);
void uart_putc_raw(uart_inst_t *uart, char character);

#endif
