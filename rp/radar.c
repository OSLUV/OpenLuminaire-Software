/**
 * @file      radar.c
 * @author    The OSLUV Project
 * @brief     Driver for external mmWave Radar (human presence motion module)
 * @hwref     HLK-LD2410C
 * @schematic lamp_controller.SchDoc
 *  
 */


/* Includes ------------------------------------------------------------------*/

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


/* Compile-time --------------------------------------------------------------*/

static_assert(sizeof(RADAR_MESSAGE_T) == (0x0D + 2 + 4 + 4));


/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/

#define RADAR_INVALID_CM_C			30      									/* Sensor returns this when idle  */
#define RADAR_STALE_US_C      		(1000 * 1000) 								/* How long a value stays “fresh” */

#define RADAR_EN_CFG_CMD_C 			0x00FF
#define RADAR_END_CFG_CMD_C 		0x00FE
#define RADAR_SET_BAUDRATE_CMD_C	0x00A1
#define RADAR_FACTORY_STNS_CMD_C	0x00A2
#define RADAR_RESTART_CMD_C			0x00A3

#define RADAR_BAUDRATE_9600_C		0x0001
#define RADAR_BAUDRATE_19200_C		0x0002
#define RADAR_BAUDRATE_38400_C		0x0003
#define RADAR_BAUDRATE_57600_C		0x0004
#define RADAR_BAUDRATE_115200_C		0x0005
#define RADAR_BAUDRATE_230400_C		0x0006
#define RADAR_BAUDRATE_256000_C		0x0007
#define RADAR_BAUDRATE_460800_C		0x0008


/* Global variables  ---------------------------------------------------------*/
/* Private variables  --------------------------------------------------------*/

static int				 radar_errors = 0;
static int				 radar_distance_cm = -1;
static int 				 radar_last_good_distance_cm = -1;
static uint64_t			 radar_last_good_time_us     = 0;

static RADAR_REPORT_T	 radar_last_report;
static uint64_t 		 radar_last_report_time = 0;
static uint64_t 		 radar_last_reinit_time = 0;

volatile uint8_t 		 radar_uart_rx_buffer[256];
volatile int			 radar_uart_rx_ptr = 0;
volatile uint64_t		 radar_uart_last_rx_time = 0;

volatile RADAR_MESSAGE_T radar_message;
volatile bool 			 b_radar_is_message_ok;


/* Callback prototypes -------------------------------------------------------*/

void radar_uart_rx_callback(void);


/* Private function prototypes -----------------------------------------------*/

static inline int radar_pick_distance(uint16_t det, uint16_t mov, uint16_t stat);
static void radar_init_comms(void);
static void radar_reset_rx(void);
static void radar_process_rx_msg(void);
static void radar_uart_txn(uint16_t command_word, uint8_t* p_tx_buf, uint tx_len);
static void radar_do_enter_config_mode(void);
static void radar_do_exit_config_mode(void);
static void radar_do_factory_reset(void);
static void radar_do_restart(void);
static void radar_do_set_baudrate(uint16_t baudrate_val);
static void radar_reinit(int baudrate);

void dbgf(const char *fmt, ...);


/* Exported functions --------------------------------------------------------*/

/**
 * @brief mmWave Radar initialization procedure
 * 
 * @return 	void  
 * 
 */
void radar_init(void)
{
	uart_init(UART_INST_MMWAVE, 9600);
    gpio_set_function(PIN_MMWAVE_RX, GPIO_FUNC_UART);
    gpio_set_function(PIN_MMWAVE_TX, GPIO_FUNC_UART);
    uart_set_format(UART_INST_MMWAVE, 8, 1, UART_PARITY_NONE);

    while (uart_is_readable(UART_INST_MMWAVE)) 
	{
		uart_getc(UART_INST_MMWAVE);
	}

    int UART_IRQ = UART_INST_MMWAVE == uart0 ? UART0_IRQ : UART1_IRQ;

    irq_set_exclusive_handler(UART_IRQ, radar_uart_rx_callback);
    irq_set_enabled(UART_IRQ, true);
}

/**
 * @brief   Updates mmWave Radar received messages
 * 
 * @return 	void  
 */
void radar_update(void)
{
	if ((time_us_64() - radar_uart_last_rx_time) > (50 * 1000))
	{
		// printf("Reset rx\n");
		radar_reset_rx();
	}

	if ((time_us_64() - radar_last_report_time) > (1000 * 3000) && 
	    (time_us_64() - radar_last_reinit_time) > (1000 * 3000) && 
		lamp_get_switched_12v())
	{
		radar_init_comms();
		radar_last_reinit_time = time_us_64();
	}

	if (b_radar_is_message_ok)
	{
		// printf("Radar radar_message:\n");

		if ((*(uint32_t*)radar_message.preamble  == 0xf1f2f3f4) && 
		    (*(uint32_t*)radar_message.postamble == 0xf5f6f7f8))
		{
			// // printf("preamble = %08x\n", *(uint32_t*)radar_message.preamble);
			// printf(".type = %d\n", radar_message.inner.type);
			// printf(".report.target_state = %d\n", radar_message.inner.report.target_state);
			// printf(".report.moving_target_distance_cm = %d\n", radar_message.inner.report.moving_target_distance_cm);
			// printf(".report.moving_target_energy = %d\n", radar_message.inner.report.moving_target_energy);
			// printf(".report.stationary_target_distance_cm = %d\n", radar_message.inner.report.stationary_target_distance_cm);
			// printf(".report.stationary_target_energy = %d\n", radar_message.inner.report.stationary_target_energy);
			// printf(".report.detection_distance_cm = %d\n", radar_message.inner.report.detection_distance_cm);
			// // printf("postamble = %08x\n", *(uint32_t*)radar_message.postamble);
			// printf("\n");

			memcpy((char*)&radar_last_report, (char*)&radar_message.inner, sizeof(radar_last_report));
			radar_last_report_time = time_us_64();
			radar_errors = 0;
		}
		else
		{
			printf(">>ill formed<< pre=%08x post=%08x\n", 
				   *(uint32_t*)radar_message.preamble, 
				   *(uint32_t*)radar_message.postamble);
			radar_errors++;

#if 0
			if (radar_errors > 4)
			{
				radar_reinit();
			}
#endif
		}

		int candidate = radar_pick_distance(
            radar_last_report.report.detection_distance_cm,
            radar_last_report.report.moving_target_distance_cm,
            radar_last_report.report.stationary_target_distance_cm);

        /* Only accept it if it isn’t the 30 cm sentinel      */
        if (candidate != -1) /* Is message valid ? */
		{
            radar_last_good_distance_cm = candidate;
            radar_last_good_time_us     = time_us_64();
        }

		b_radar_is_message_ok = false;
	}
	else
	{
		// printf("(No new radar_message, ptr=%d)\n", radar_uart_rx_ptr);
	}

	if ((time_us_64() - radar_last_report_time) > (5 * 1000 * 1000))
	{
		radar_distance_cm = -1;
	}
	else
	{
		radar_distance_cm = radar_last_good_distance_cm;
	}
}

/**
 * @brief Prints output for debug
 * 
 */
void radar_debug(void)
{
	int dt = (time_us_64() - radar_last_report_time) / (1000);
	dbgf("Radar: Ty%d dT% 8dms %s\n", radar_last_report.type, dt, (dt>1000 || radar_last_report_time == 0)?"STALE":"OK");
	dbgf("Radar: M: %dcm %de\n", radar_last_report.report.moving_target_distance_cm, radar_last_report.report.moving_target_energy);
	dbgf("Radar: S: %dcm %de\n", radar_last_report.report.stationary_target_distance_cm, radar_last_report.report.stationary_target_energy);
	dbgf("Radar: DD: %dcm PIN:%d/%d / RD:%d\n", radar_last_report.report.detection_distance_cm, gpio_get(PIN_MMWAVE_RX), gpio_get(PIN_MMWAVE_TX), radar_get_distance_cm());
}

/**
 * @brief Returns the sensed distance in centimeters
 * 
 * @return int 
 */
int radar_get_distance_cm(void)
{
	return radar_distance_cm;
}

#if 0
/**
 * @brief Returns the moving target distance in centimeters
 * 
 * @return int 
 */
int radar_get_moving_target_cm(void)
{
	return radar_last_report.report.moving_target_distance_cm;
}
#endif

#if 0
/**
 * @brief Returns the stationary target distance in centimeters
 * 
 * @return int 
 */
int radar_get_stationary_target_cm(void)
{
	return radar_last_report.report.stationary_target_distance_cm;
}
#endif

/**
 * @brief Returns the last report data @ref RADAR_REPORT_T
 * 
 * @return RADAR_REPORT_T* 
 */
RADAR_REPORT_T* radar_debug_get_report(void)
{
	return &radar_last_report;
}

/**
 * @brief Returns the last report time
 * 
 * @return int 
 */
int radar_debug_get_report_time(void)
{
	return radar_last_report_time;
}


/* Callback functions --------------------------------------------------------*/

/**
 * @brief Callback for mmWave Radar UART RX ISR
 * 
 */
void radar_uart_rx_callback(void)
{
    while (uart_is_readable(UART_INST_MMWAVE))
	{
        uint8_t ch = uart_getc(UART_INST_MMWAVE);
        
        radar_uart_rx_buffer[radar_uart_rx_ptr] = ch;

        radar_uart_rx_ptr++;

        if (radar_uart_rx_ptr == sizeof(RADAR_MESSAGE_T))
        {
        	radar_process_rx_msg();
        	radar_reset_rx();
        }
    }
}


/* Private functions ---------------------------------------------------------*/

/**
 * @brief Returns the best distance
 * 
 * Prefer the detection field when it’s valid, otherwise take the nearer of the 
 * two raw fields, but only if it is greather than 30 cm @ref RADAR_INVALID_CM_C
 * 
 * @param det Detection distance
 * @param mov Movement distance
 * @param stat Stationary distance
 * @return int 
 */
static inline int radar_pick_distance(uint16_t det, uint16_t mov, uint16_t stat)
{
#if 0
    if (det > RADAR_INVALID_CM_C) 
	{
		return det;
	}
#endif

    int m = (mov  > RADAR_INVALID_CM_C) ? mov  : INT_MAX;
    int s = (stat > RADAR_INVALID_CM_C) ? stat : INT_MAX;

    int best = (m < s) ? m : s;
	
    return (best == INT_MAX) ? -1 : best;
}

/**
 * @brief Inititialization procedure for Radar's UART communication
 * 
 */
static void radar_init_comms(void)
{
	printf("radar_init_comms()\n");
	radar_reinit(256000);
    radar_reinit(9600);
}

/**
 * @brief Resets UART Rx reception time
 * 
 */
static void radar_reset_rx(void)
{
	radar_uart_rx_ptr = 0;
	radar_uart_last_rx_time = time_us_64();
}

/**
 * @brief Processes received message from radar device
 * 
 */
static void radar_process_rx_msg(void)
{
	if (b_radar_is_message_ok) 
	{
		return;
	}

	memcpy((char*)&radar_message, (char*)radar_uart_rx_buffer, sizeof(radar_message));
	b_radar_is_message_ok = true;
}

/**
 * @brief Sends command to radar device
 * 
 * @param command_word Command to send
 * @param p_tx_buf Data to send
 * @param tx_len Data length to send
 */
static void radar_uart_txn(uint16_t command_word, uint8_t* p_tx_buf, uint tx_len)
{
	uint8_t preamble[]  = {0xFD, 0xFC, 0xFB, 0xFA}; // Fixed frame header 
	uint8_t postamble[] = {0x04, 0x03, 0x02, 0x01}; // Fixed end of frame

    uint16_t pkt_len = tx_len + 2; // For command word

	uart_write_blocking(UART_INST_MMWAVE, preamble, sizeof(preamble));
	uart_write_blocking(UART_INST_MMWAVE, (uint8_t*)&pkt_len, 2);
	uart_write_blocking(UART_INST_MMWAVE, (uint8_t*)&command_word, 2);
	uart_write_blocking(UART_INST_MMWAVE, p_tx_buf, tx_len);
	uart_write_blocking(UART_INST_MMWAVE, postamble, sizeof(postamble));
}

/**
 * @brief Sets the radar device in config mode
 * 
 * All commands must be sent after this command
 * 
 */
static void radar_do_enter_config_mode(void)
{
	uint8_t buf[] = {0x01, 0x00};
	radar_uart_txn(RADAR_EN_CFG_CMD_C, buf, sizeof(buf));
}

/**
 * @brief Ends the config mode in the radar device
 * 
 */
static void radar_do_exit_config_mode(void)
{
	uint8_t buf[] = {};
	radar_uart_txn(RADAR_END_CFG_CMD_C, buf, sizeof(buf));
}

/**
 * @brief Restores all radar's configuration values to their non-factory values.
 * 
 */
static void radar_do_factory_reset(void)
{
	uint8_t buf[] = {};
	radar_uart_txn(RADAR_FACTORY_STNS_CMD_C, buf, sizeof(buf));
}

/**
 * @brief Restarts the radar device
 * 
 */
static void radar_do_restart(void)
{
	uint8_t buf[] = {};
	radar_uart_txn(RADAR_RESTART_CMD_C, buf, sizeof(buf));
}

/**
 * @brief Sets radar UART port baud rate
 * 
 * @param baudrate_val 
 */
static void radar_do_set_baudrate(uint16_t baudrate_val)
{
	radar_uart_txn(RADAR_SET_BAUDRATE_CMD_C, (uint8_t*)&baudrate_val, 2);
}

/**
 * @brief Radar's reinitialization process
 * 
 * This process is not guaranteed.
 * 
 * @param baudrate 
 */
static void radar_reinit(int baudrate)
{
	int actual_baudrate = uart_set_baudrate(UART_INST_MMWAVE, baudrate);

    // printf("actual_baudrate: %d\n", actual_baudrate);

    uart_set_irq_enables(UART_INST_MMWAVE, false, false);

    radar_do_enter_config_mode();
    sleep_ms(50);
	radar_do_factory_reset();
	sleep_ms(50);
	radar_do_set_baudrate(RADAR_BAUDRATE_9600_C);
	sleep_ms(50);
	radar_do_restart();
	while (uart_is_readable(UART_INST_MMWAVE)) 
	{
		uart_getc(UART_INST_MMWAVE);
	}
	sleep_ms(50);

	uart_set_irq_enables(UART_INST_MMWAVE, true, false);
}

/*** END OF FILE ***/
