#include <stdio.h>
#include <string.h>
#include <hardware/dma.h>
#include <hardware/gpio.h>
#include <hardware/uart.h>
#include "pins.h"
#include "main.h"

char radio_rx_buffer[128 + 8] = {0};
int radio_rx_pointer = 0;

char* msg_start = NULL;

char last_radio_rx_msg[4][128 + 8] = {0};

void process_line(char* line)
{
	printf("Radio line: '%s'\n", line);
	memmove(last_radio_rx_msg[2], last_radio_rx_msg[1], 128);
	memmove(last_radio_rx_msg[1], last_radio_rx_msg[0], 128);
	memmove(last_radio_rx_msg[0], radio_rx_buffer, 128);
}

void on_radio_uart_rx()
{
	while (uart_is_readable(UART_INST_RADIO)) {
        uint8_t ch = uart_getc(UART_INST_RADIO);
        
        radio_rx_buffer[radio_rx_pointer] = ch;

        if (ch == '\n')
        {
        	radio_rx_buffer[radio_rx_pointer] = 0;
        	process_line(radio_rx_buffer);
        	radio_rx_pointer = 0;
        	continue;
        }

        radio_rx_pointer++;

        if (radio_rx_pointer == 128)
        {
        	radio_rx_pointer = 0;
        }
    }
}

void init_radio()
{
	uart_init(UART_INST_RADIO, 9600);
    gpio_set_function(PIN_RADIO_TX, GPIO_FUNC_UART);
    gpio_set_function(PIN_RADIO_RX, GPIO_FUNC_UART);
    uart_set_format(UART_INST_RADIO, 8, 1, UART_PARITY_NONE);

    int actual_baudrate = uart_set_baudrate(UART_INST_RADIO, 115200);
    printf("radio uart actual_baudrate: %d\n", actual_baudrate);

    int UART_IRQ = UART_INST_RADIO == uart0 ? UART0_IRQ : UART1_IRQ;

    irq_set_exclusive_handler(UART_IRQ, on_radio_uart_rx);
    irq_set_enabled(UART_IRQ, true);

    while (uart_is_readable(UART_INST_RADIO)) uart_getc(UART_INST_RADIO);

    uart_set_irq_enables(UART_INST_RADIO, true, false);
}

void update_radio()
{

}

char* get_radio_string()
{
	return last_radio_rx_msg[0];
}

void set_radio_string(char* s)
{

}

void debug_radio()
{
	dbgf("Radio:\n");
    dbgf("%s\n", last_radio_rx_msg[2]);
    dbgf("%s\n", last_radio_rx_msg[1]);
    dbgf("%s\n", last_radio_rx_msg[0]);
}
