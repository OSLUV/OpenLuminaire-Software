#ifndef TEST_FAKE_HOST_H
#define TEST_FAKE_HOST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct fake_stdio_call {
    size_t offset;
    size_t len;
    bool   newline;
    bool   cr_translation;
} fake_stdio_call_t;

void fake_host_reset(void);

void fake_time_set_us(uint64_t now_us);
void fake_time_advance_us(uint64_t delta_us);
uint64_t fake_time_now_us(void);

void fake_uart_receive(const uint8_t *data, size_t len);
void fake_uart_receive_deferred(const uint8_t *data, size_t len);
void fake_uart_service_irq(void);
const uint8_t *fake_uart_tx_data(void);
size_t fake_uart_tx_len(void);
void fake_uart_tx_clear(void);

void fake_usb_receive(const uint8_t *data, size_t len);
const uint8_t *fake_usb_tx_data(void);
size_t fake_usb_tx_len(void);
void fake_usb_tx_clear(void);

size_t fake_stdio_call_count(void);
const fake_stdio_call_t *fake_stdio_calls(void);
size_t fake_stdio_flush_count(void);

#endif
