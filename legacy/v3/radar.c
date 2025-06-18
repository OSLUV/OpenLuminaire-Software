#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"
#include "pico/multicore.h"
#include "pins.h"

int actual_baudrate = 0;

void radar_uart_init(int baudrate)
{
	uart_init(MMWAVE_UART, baudrate);
    gpio_set_function(MMWAVE_RX_PIN, GPIO_FUNC_UART);
    gpio_set_function(MMWAVE_TX_PIN, GPIO_FUNC_UART);
    uart_set_format(MMWAVE_UART, 8, 1, UART_PARITY_NONE);
    actual_baudrate = uart_set_baudrate(MMWAVE_UART, baudrate);

    while (uart_is_readable(MMWAVE_UART)) uart_getc(MMWAVE_UART);
}

uint16_t mmwave_pkt_rx_buf_size;
uint8_t mmwave_pkt_rx_buf[0x100];

void radar_txn_done()
{}

void radar_uart_txn(uint16_t command_word, uint8_t* tx_buf, uint tx_len)
{
	printf("TX c=%04x, %02x bytes: [", command_word, tx_len);
	for (int i = 0; i < tx_len; i++)
	{
		printf("%02x ", tx_buf[i]);
	}
	printf("]\n");

	uint8_t preamble[] = {0xFD, 0xFC, 0xFB, 0xFA};
	uint8_t postamble[] = {0x04, 0x03, 0x02, 0x01};

	// Flush rx buf
	while (uart_is_readable(MMWAVE_UART))
        uart_getc(MMWAVE_UART);

    uint16_t pkt_len = tx_len + 2; // For command word

	uart_write_blocking(MMWAVE_UART, preamble, sizeof(preamble));
	uart_write_blocking(MMWAVE_UART, (uint8_t*)&pkt_len, 2);
	uart_write_blocking(MMWAVE_UART, (uint8_t*)&command_word, 2);
	uart_write_blocking(MMWAVE_UART, tx_buf, tx_len);
	uart_write_blocking(MMWAVE_UART, postamble, sizeof(postamble));

	sleep_ms(5);

	// if (!uart_is_readable(MMWAVE_UART)) return;

	uint8_t rx_byte1 = 0;
	int retry_count = 0;
	while (rx_byte1 != 0xFD)
	{
		uart_read_blocking(MMWAVE_UART, &rx_byte1, 1);
		if (retry_count++ > 5)
			return;
	}

	uint8_t rx_header_buf[3];
	uart_read_blocking(MMWAVE_UART, rx_header_buf, 3);

	if (rx_header_buf[0] != 0xFC || rx_header_buf[1] != 0xFB || rx_header_buf[2] != 0xFA)
	{
		return;
	}

	uint8_t postamble_rx[4] = {0};
	uart_read_blocking(MMWAVE_UART, (uint8_t*)&mmwave_pkt_rx_buf_size, 2);
	uart_read_blocking(MMWAVE_UART, mmwave_pkt_rx_buf, mmwave_pkt_rx_buf_size);
	uart_read_blocking(MMWAVE_UART, postamble_rx, 4);

	radar_txn_done();
	// TODO: Error handling and validation
}

void radar_do_enter_config_mode()
{
	uint8_t buf[] = {0x01, 0x00};
	radar_uart_txn(0x00FF, buf, sizeof(buf));
}

void radar_do_exit_config_mode()
{
	uint8_t buf[] = {};
	radar_uart_txn(0x00FE, buf, sizeof(buf));
}

void radar_do_factory_reset()
{
	uint8_t buf[] = {};
	radar_uart_txn(0x00A2, buf, sizeof(buf));
}

void radar_do_restart()
{
	uint8_t buf[] = {};
	radar_uart_txn(0x00A3, buf, sizeof(buf));
}

void radar_do_set_baudrate(uint16_t baudrate_val)
{
	radar_uart_txn(0x00A1, (uint8_t*)&baudrate_val, 2);
}

struct __packed radar_report 
{
	uint8_t type;
	uint8_t _head;
	struct __packed
	{
		uint8_t target_state;
		uint16_t moving_target_distance_cm;
		uint8_t moving_target_energy;
		uint16_t stationary_target_distance_cm;
		uint8_t stationary_target_energy;
		uint16_t detection_distance_cm;
	} report;
	uint8_t _end;
	uint8_t _check;
};

void radar_read_report()
{
	// if (!uart_is_readable(MMWAVE_UART)) return;

	repeat_rx:
	if (uart_getc(MMWAVE_UART) != 0xF4) goto repeat_rx;
	if (uart_getc(MMWAVE_UART) != 0xF3) goto repeat_rx;
	if (uart_getc(MMWAVE_UART) != 0xF2) goto repeat_rx;
	if (uart_getc(MMWAVE_UART) != 0xF1) goto repeat_rx;

	uint8_t postamble_rx[4] = {0};
	uart_read_blocking(MMWAVE_UART, (uint8_t*)&mmwave_pkt_rx_buf_size, 2);
	
	if (mmwave_pkt_rx_buf_size != 0x000D)
	{
		goto repeat_rx;
	}

	uart_read_blocking(MMWAVE_UART, mmwave_pkt_rx_buf, mmwave_pkt_rx_buf_size);
	uart_read_blocking(MMWAVE_UART, postamble_rx, 4);
}

struct radar_report* get_last_radar_report()
{
	return (struct radar_report*)mmwave_pkt_rx_buf;
}

bool radar_get_presence()
{
	bool present = false;

	if (get_last_radar_report()->_head == 0xAA)
	{
		if (get_last_radar_report()->report.target_state >= 0x02)
		{
			present |= get_last_radar_report()->report.moving_target_distance_cm < 100;
		}
	}

	return present;
}

// #define radar_write_imm(word, contents) {uint8_t buf[] = contents; radar_uart_txn(word, buf, sizeof(buf));}

void radar_diag_print()
{
	printf("RX %02x bytes: [", mmwave_pkt_rx_buf_size);
	for (int i = 0; i < mmwave_pkt_rx_buf_size; i++)
	{
		printf("%02x ", mmwave_pkt_rx_buf[i]);
	}
	printf("]\n");
}

// #define gpio_put(x, y) {}

bool radar_reinit(int baudrate, bool allow_fail)
{
	gpio_put(LED0_PIN, 1);

	gpio_init(MMWAVE_TX_PIN);
	gpio_set_pulls(MMWAVE_TX_PIN, false, false);

	uint64_t start = to_ms_since_boot(get_absolute_time());

	while (!gpio_get(MMWAVE_TX_PIN))
	{
		// Spin waiting for device to come up...
		sleep_ms(100);
		gpio_put(LED0_PIN, 0);
		sleep_ms(100);
		gpio_put(LED0_PIN, 1);

		if (allow_fail && (to_ms_since_boot(get_absolute_time()) - start) > 1500)
		{
			return false;
		}
	}

	sleep_ms(200);

	radar_uart_init(baudrate);

	sleep_ms(200);

	return true;
}

void radar_task()
{
	radar_reinit(256000, true);

	radar_do_enter_config_mode();
	radar_do_factory_reset();
	radar_do_set_baudrate(0x0001);
	radar_do_restart();
	bool ok = radar_reinit(9600, true);

	// if (!ok) return;

	radar_read_report();

	bool x = 1;
	while (1)
	{
		radar_read_report();
		// gpio_put(LED1_PIN, !radar_get_presence());
		// gpio_put(LED0_PIN, 1);
		// sleep_ms(50);
		// gpio_put(LED0_PIN, 0);
	}
}

void radar_init()
{
	multicore_launch_core1(radar_task);
}