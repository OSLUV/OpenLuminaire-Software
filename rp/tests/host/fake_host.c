#include "fake_host.h"

#include <assert.h>
#include <string.h>

#include "hardware/irq.h"
#include "hardware/uart.h"
#include "pico/stdio.h"
#include "pico/time.h"

#define FAKE_IO_CAPACITY_C    16384U
#define FAKE_STDIO_CALLS_C     4096U

uart_inst_t fake_uart1_instance = {1U};

static uint64_t fake_now_us;

static uint8_t fake_uart_rx[FAKE_IO_CAPACITY_C];
static size_t  fake_uart_rx_len;
static size_t  fake_uart_rx_pos;
static uint8_t fake_uart_tx[FAKE_IO_CAPACITY_C];
static size_t  fake_uart_tx_count;

static uint8_t fake_usb_rx[FAKE_IO_CAPACITY_C];
static size_t  fake_usb_rx_len;
static size_t  fake_usb_rx_pos;
static uint8_t fake_usb_tx[FAKE_IO_CAPACITY_C];
static size_t  fake_usb_tx_count;

static fake_stdio_call_t fake_calls[FAKE_STDIO_CALLS_C];
static size_t            fake_call_count;
static size_t            fake_flush_count;

static irq_handler_t fake_uart_irq_handler;
static bool          fake_uart_irq_enabled;
static bool          fake_uart_rx_irq_enabled;

void fake_host_reset(void)
{
    fake_now_us = 1000000U;

    fake_uart_rx_len = 0;
    fake_uart_rx_pos = 0;
    fake_uart_tx_count = 0;

    fake_usb_rx_len = 0;
    fake_usb_rx_pos = 0;
    fake_usb_tx_count = 0;

    fake_call_count = 0;
    fake_flush_count = 0;

    fake_uart_irq_handler = NULL;
    fake_uart_irq_enabled = false;
    fake_uart_rx_irq_enabled = false;
}

void fake_time_set_us(uint64_t now_us)
{
    fake_now_us = now_us;
}

void fake_time_advance_us(uint64_t delta_us)
{
    fake_now_us += delta_us;
}

uint64_t fake_time_now_us(void)
{
    return fake_now_us;
}

absolute_time_t get_absolute_time(void)
{
    return fake_now_us;
}

uint64_t time_us_64(void)
{
    return fake_now_us;
}

void fake_uart_receive_deferred(const uint8_t *data, size_t len)
{
    assert(data != NULL || len == 0);
    assert((fake_uart_rx_len - fake_uart_rx_pos + len) <= sizeof(fake_uart_rx));

    if ((fake_uart_rx_pos > 0) && (fake_uart_rx_pos != fake_uart_rx_len))
    {
        memmove(fake_uart_rx,
                fake_uart_rx + fake_uart_rx_pos,
                fake_uart_rx_len - fake_uart_rx_pos);
        fake_uart_rx_len -= fake_uart_rx_pos;
        fake_uart_rx_pos = 0;
    }
    else if (fake_uart_rx_pos == fake_uart_rx_len)
    {
        fake_uart_rx_len = 0;
        fake_uart_rx_pos = 0;
    }

    if (len > 0)
    {
        memcpy(fake_uart_rx + fake_uart_rx_len, data, len);
        fake_uart_rx_len += len;
    }
}

void fake_uart_receive(const uint8_t *data, size_t len)
{
    fake_uart_receive_deferred(data, len);

    if (len == 0)
    {
        return;
    }

    fake_uart_service_irq();
    assert(fake_uart_rx_pos == fake_uart_rx_len);
}

void fake_uart_service_irq(void)
{
    assert(fake_uart_irq_handler != NULL);
    assert(fake_uart_irq_enabled);
    assert(fake_uart_rx_irq_enabled);
    fake_uart_irq_handler();
}

const uint8_t *fake_uart_tx_data(void)
{
    return fake_uart_tx;
}

size_t fake_uart_tx_len(void)
{
    return fake_uart_tx_count;
}

void fake_uart_tx_clear(void)
{
    fake_uart_tx_count = 0;
}

void fake_usb_receive(const uint8_t *data, size_t len)
{
    assert(data != NULL || len == 0);
    assert((fake_usb_rx_len - fake_usb_rx_pos + len) <= sizeof(fake_usb_rx));

    if ((fake_usb_rx_pos > 0) && (fake_usb_rx_pos != fake_usb_rx_len))
    {
        memmove(fake_usb_rx,
                fake_usb_rx + fake_usb_rx_pos,
                fake_usb_rx_len - fake_usb_rx_pos);
        fake_usb_rx_len -= fake_usb_rx_pos;
        fake_usb_rx_pos = 0;
    }
    else if (fake_usb_rx_pos == fake_usb_rx_len)
    {
        fake_usb_rx_len = 0;
        fake_usb_rx_pos = 0;
    }

    if (len > 0)
    {
        memcpy(fake_usb_rx + fake_usb_rx_len, data, len);
        fake_usb_rx_len += len;
    }
}

const uint8_t *fake_usb_tx_data(void)
{
    return fake_usb_tx;
}

size_t fake_usb_tx_len(void)
{
    return fake_usb_tx_count;
}

void fake_usb_tx_clear(void)
{
    fake_usb_tx_count = 0;
    fake_call_count = 0;
    fake_flush_count = 0;
}

size_t fake_stdio_call_count(void)
{
    return fake_call_count;
}

const fake_stdio_call_t *fake_stdio_calls(void)
{
    return fake_calls;
}

size_t fake_stdio_flush_count(void)
{
    return fake_flush_count;
}

int getchar_timeout_us(uint32_t timeout_us)
{
    (void)timeout_us;

    if (fake_usb_rx_pos == fake_usb_rx_len)
    {
        return -1;
    }

    return fake_usb_rx[fake_usb_rx_pos++];
}

int stdio_put_string(const char *data, int len, bool newline, bool cr_translation)
{
    size_t write_len;

    assert(data != NULL || len == 0);
    assert(len >= 0);
    assert(fake_call_count < FAKE_STDIO_CALLS_C);

    write_len = (size_t)len;
    assert((fake_usb_tx_count + write_len + (newline ? 1U : 0U)) <= sizeof(fake_usb_tx));

    fake_calls[fake_call_count].offset = fake_usb_tx_count;
    fake_calls[fake_call_count].len = write_len;
    fake_calls[fake_call_count].newline = newline;
    fake_calls[fake_call_count].cr_translation = cr_translation;
    fake_call_count++;

    if (write_len > 0)
    {
        memcpy(fake_usb_tx + fake_usb_tx_count, data, write_len);
        fake_usb_tx_count += write_len;
    }

    if (newline)
    {
        fake_usb_tx[fake_usb_tx_count++] = '\n';
    }

    return len + (newline ? 1 : 0);
}

int putchar_raw(int character)
{
    char byte;

    byte = (char)character;
    (void)stdio_put_string(&byte, 1, false, false);
    return character;
}

void stdio_flush(void)
{
    fake_flush_count++;
}

uint uart_init(uart_inst_t *uart, uint baudrate)
{
    assert(uart == uart1);
    return baudrate;
}

void uart_set_format(uart_inst_t *uart,
                     uint data_bits,
                     uint stop_bits,
                     uart_parity_t parity)
{
    assert(uart == uart1);
    assert(data_bits == 8U);
    assert(stop_bits == 1U);
    assert(parity == UART_PARITY_NONE);
}

void uart_set_fifo_enabled(uart_inst_t *uart, bool enabled)
{
    assert(uart == uart1);
    assert(enabled);
}

void uart_set_irqs_enabled(uart_inst_t *uart, bool rx_has_data, bool tx_needs_data)
{
    assert(uart == uart1);
    fake_uart_rx_irq_enabled = rx_has_data;
    assert(!tx_needs_data);
}

bool uart_is_readable(uart_inst_t *uart)
{
    assert(uart == uart1);
    return fake_uart_rx_pos < fake_uart_rx_len;
}

char uart_getc(uart_inst_t *uart)
{
    assert(uart == uart1);
    assert(fake_uart_rx_pos < fake_uart_rx_len);
    return (char)fake_uart_rx[fake_uart_rx_pos++];
}

void uart_putc_raw(uart_inst_t *uart, char character)
{
    assert(uart == uart1);
    assert(fake_uart_tx_count < sizeof(fake_uart_tx));
    fake_uart_tx[fake_uart_tx_count++] = (uint8_t)character;
}

void gpio_set_function(uint pin, uint function)
{
    assert((pin == 8U) || (pin == 9U));
    assert(function == GPIO_FUNC_UART);
}

void irq_set_exclusive_handler(uint irq_num, irq_handler_t handler)
{
    assert(irq_num == UART1_IRQ);
    fake_uart_irq_handler = handler;
}

void irq_set_enabled(uint irq_num, bool enabled)
{
    assert(irq_num == UART1_IRQ);
    fake_uart_irq_enabled = enabled;
}
