#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <math.h>
#include <limits.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"
#include "pins.h"
#include "lamp.h"
#include "radar.h"

#define RADAR_INVALID_CM        30      /* sensor returns this when idle   */
#define RADAR_STALE_US      (1000*1000) /* how long a value stays “fresh”  */

static int       last_good_distance_cm = -1;
static uint64_t  last_good_time_us     = 0;


static_assert(sizeof(struct radar_message) == (0x0D + 2 + 4 + 4));

volatile uint8_t uart_rx_buffer[256];
volatile int uart_rx_ptr = 0;
volatile uint64_t uart_last_rx_time = 0;

volatile struct radar_message message;
volatile bool message_ok;

void reset_rx()
{
	uart_rx_ptr = 0;
	uart_last_rx_time = time_us_64();
}

void process_rx()
{
	if (message_ok) return;

	memcpy((char*)&message, (char*)uart_rx_buffer, sizeof(message));
	message_ok = true;
}

void on_uart_rx() {
    while (uart_is_readable(UART_INST_MMWAVE)) {
        uint8_t ch = uart_getc(UART_INST_MMWAVE);
        
        uart_rx_buffer[uart_rx_ptr] = ch;

        uart_rx_ptr++;

        if (uart_rx_ptr == sizeof(struct radar_message))
        {
        	process_rx();
        	reset_rx();
        }
    }
}

void radar_uart_txn(uint16_t command_word, uint8_t* tx_buf, uint tx_len)
{
	uint8_t preamble[] = {0xFD, 0xFC, 0xFB, 0xFA};
	uint8_t postamble[] = {0x04, 0x03, 0x02, 0x01};

    uint16_t pkt_len = tx_len + 2; // For command word

	uart_write_blocking(UART_INST_MMWAVE, preamble, sizeof(preamble));
	uart_write_blocking(UART_INST_MMWAVE, (uint8_t*)&pkt_len, 2);
	uart_write_blocking(UART_INST_MMWAVE, (uint8_t*)&command_word, 2);
	uart_write_blocking(UART_INST_MMWAVE, tx_buf, tx_len);
	uart_write_blocking(UART_INST_MMWAVE, postamble, sizeof(postamble));
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

void radar_reinit(int baudrate)
{
	// This (re)init process is totally blind. Can't do anything if it fails anyway
	int actual_baudrate = uart_set_baudrate(UART_INST_MMWAVE, baudrate);
    // printf("actual_baudrate: %d\n", actual_baudrate);

    uart_set_irq_enables(UART_INST_MMWAVE, false, false);

    radar_do_enter_config_mode();
    sleep_ms(50);
	radar_do_factory_reset();
	sleep_ms(50);
	radar_do_set_baudrate(0x0001);
	sleep_ms(50);
	radar_do_restart();
	while (uart_is_readable(UART_INST_MMWAVE)) uart_getc(UART_INST_MMWAVE);
	sleep_ms(50);

	uart_set_irq_enables(UART_INST_MMWAVE, true, false);
}

void init_radar()
{
	uart_init(UART_INST_MMWAVE, 9600);
    gpio_set_function(PIN_MMWAVE_RX, GPIO_FUNC_UART);
    gpio_set_function(PIN_MMWAVE_TX, GPIO_FUNC_UART);
    uart_set_format(UART_INST_MMWAVE, 8, 1, UART_PARITY_NONE);

    while (uart_is_readable(UART_INST_MMWAVE)) uart_getc(UART_INST_MMWAVE);

    int UART_IRQ = UART_INST_MMWAVE == uart0 ? UART0_IRQ : UART1_IRQ;

    irq_set_exclusive_handler(UART_IRQ, on_uart_rx);
    irq_set_enabled(UART_IRQ, true);
}

void init_radar_comms()
{
	printf("init_radar_comms()\n");
	radar_reinit(256000);
    radar_reinit(9600);
}

void dbgf(char*, ...);

struct radar_report last_report;
uint64_t last_report_time = 0;
uint64_t last_reinit_time = 0;

int errors = 0;

int radar_distance_cm = -1;

static inline int
pick_distance(uint16_t det, uint16_t mov, uint16_t stat)
{
    /* prefer the detection field when it’s valid, otherwise take
       the nearer of the two raw fields, but only if it is > 30 cm */
    //if (det > RADAR_INVALID_CM) return det;

    int m = (mov  > RADAR_INVALID_CM) ? mov  : INT_MAX;
    int s = (stat > RADAR_INVALID_CM) ? stat : INT_MAX;

    int best = (m < s) ? m : s;
    return (best == INT_MAX) ? -1 : best;
}

void update_radar()
{
	if ((time_us_64() - uart_last_rx_time) > (50*1000))
	{
		// printf("Reset rx\n");
		reset_rx();
	}

	if ((time_us_64() - last_report_time) > (1000*3000) && (time_us_64() - last_reinit_time) > (1000*3000) && get_switched_12v())
	{
		init_radar_comms();
		last_reinit_time = time_us_64();
	}

	if (message_ok)
	{
		// printf("Radar message:\n");

		if (*(uint32_t*)message.preamble == 0xf1f2f3f4 && *(uint32_t*)message.postamble == 0xf5f6f7f8)
		{
			// // printf("preamble = %08x\n", *(uint32_t*)message.preamble);
			// printf(".type = %d\n", message.inner.type);
			// printf(".report.target_state = %d\n", message.inner.report.target_state);
			// printf(".report.moving_target_distance_cm = %d\n", message.inner.report.moving_target_distance_cm);
			// printf(".report.moving_target_energy = %d\n", message.inner.report.moving_target_energy);
			// printf(".report.stationary_target_distance_cm = %d\n", message.inner.report.stationary_target_distance_cm);
			// printf(".report.stationary_target_energy = %d\n", message.inner.report.stationary_target_energy);
			// printf(".report.detection_distance_cm = %d\n", message.inner.report.detection_distance_cm);
			// // printf("postamble = %08x\n", *(uint32_t*)message.postamble);
			// printf("\n");

			memcpy((char*)&last_report, (char*)&message.inner, sizeof(last_report));
			last_report_time = time_us_64();
			errors = 0;
		}
		else
		{
			printf(">>ill formed<< pre=%08x post=%08x\n", *(uint32_t*)message.preamble, *(uint32_t*)message.postamble);
			errors++;

			// if (errors > 4)
			// {
			// 	radar_reinit();
			// }
		}

		int candidate = pick_distance(
            last_report.report.detection_distance_cm,
            last_report.report.moving_target_distance_cm,
            last_report.report.stationary_target_distance_cm);

        /* Only accept it if it isn’t the 30 cm sentinel      */
        /* (candidate == -1 means nothing valid in this msg)  */
        if (candidate != -1) {
            last_good_distance_cm = candidate;
            last_good_time_us     = time_us_64();
        }

		message_ok = false;
	}
	else
	{
		// printf("(No new message, ptr=%d)\n", uart_rx_ptr);
	}

	if ((time_us_64() - last_report_time) > (5*1000*1000))
	{
		radar_distance_cm = -1;
	}
	else
	{
		radar_distance_cm = last_good_distance_cm;//MIN(last_report.report.moving_target_distance_cm, last_report.report.stationary_target_distance_cm);
	}
}

void radar_debug()
{
	int dt = (time_us_64() - last_report_time)/(1000);
	dbgf("Radar: Ty%d dT% 8dms %s\n", last_report.type, dt, (dt>1000 || last_report_time == 0)?"STALE":"OK");
	dbgf("Radar: M: %dcm %de\n", last_report.report.moving_target_distance_cm, last_report.report.moving_target_energy);
	dbgf("Radar: S: %dcm %de\n", last_report.report.stationary_target_distance_cm, last_report.report.stationary_target_energy);
	dbgf("Radar: DD: %dcm PIN:%d/%d / RD:%d\n", last_report.report.detection_distance_cm, gpio_get(PIN_MMWAVE_RX), gpio_get(PIN_MMWAVE_TX), get_radar_distance_cm());
}

int get_radar_distance_cm()
{
	return radar_distance_cm;
}

int get_moving_target_cm()
{
	return last_report.report.moving_target_distance_cm;
}

int get_stationary_target_cm()
{
	return last_report.report.stationary_target_distance_cm;
}


struct radar_report* debug_get_radar_report()
{
	return &last_report;
}

int debug_get_radar_report_time()
{
	return last_report_time;
}