/**
 * @file      radio.c
 * @author    The OSLUV Project
 * @brief     Driver for Bluetooth/WiFi radio device
 * @hwref     U8 (TMAG5273)
 * @schematic lamp_controller.SchDoc
 *  
 */


/* Includes ------------------------------------------------------------------*/

#include <stdio.h>
#include <string.h>
#include <hardware/dma.h>
#include <hardware/gpio.h>
#include <hardware/uart.h>
#include "pins.h"
#include "main.h"


/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/

#define RADIO_BUF_SIZE_C    128


/* Global variables  ---------------------------------------------------------*/
/* Private variables  --------------------------------------------------------*/

static char radio_rx_buffer[RADIO_BUF_SIZE_C + 8] = {0};
static int radio_rx_pointer = 0;

static char radio_last_rx_msg[4][RADIO_BUF_SIZE_C + 8] = {0};


/* Callback prototypes -------------------------------------------------------*/

static void radio_uart_rx_callback(void);


/* Private function prototypes -----------------------------------------------*/

static void radio_process_rx_line(char* p_line);


/* Exported functions --------------------------------------------------------*/

/**
 * @brief Bluetooth/WiFi radio device initialization procedure
 * 
 * @return 	void  
 * 
 */
void radio_init(void)
{
	uart_init(UART_INST_RADIO, 9600);
    gpio_set_function(PIN_RADIO_TX, GPIO_FUNC_UART);
    gpio_set_function(PIN_RADIO_RX, GPIO_FUNC_UART);
    uart_set_format(UART_INST_RADIO, 8, 1, UART_PARITY_NONE);

    int actual_baudrate = uart_set_baudrate(UART_INST_RADIO, 115200);

    printf("radio uart actual_baudrate: %d\n", actual_baudrate);

    int UART_IRQ = UART_INST_RADIO == uart0 ? UART0_IRQ : UART1_IRQ;

    irq_set_exclusive_handler(UART_IRQ, radio_uart_rx_callback);
    irq_set_enabled(UART_IRQ, true);

    while (uart_is_readable(UART_INST_RADIO))
    {
        uart_getc(UART_INST_RADIO);
    }

    uart_set_irq_enables(UART_INST_RADIO, true, false);
}

#if 0
/**
 * @brief String output for debug
 * 
 */
void radio_debug(void)
{
	dbgf("Radio:\n");
    dbgf("%s\n", radio_last_rx_msg[2]);
    dbgf("%s\n", radio_last_rx_msg[1]);
    dbgf("%s\n", radio_last_rx_msg[0]);
}
#endif


/* Callback functions --------------------------------------------------------*/

/**
 * @brief Callback for UART data reception from radio device
 * 
 */
static void radio_uart_rx_callback(void)
{
	while (uart_is_readable(UART_INST_RADIO)) 
    {
        uint8_t ch = uart_getc(UART_INST_RADIO);
        
        radio_rx_buffer[radio_rx_pointer] = ch;

        if (ch == '\n')
        {
        	radio_rx_buffer[radio_rx_pointer] = 0;
        	radio_process_rx_line(radio_rx_buffer);
        	radio_rx_pointer = 0;
        	continue;
        }

        radio_rx_pointer++;

        if (radio_rx_pointer >= RADIO_BUF_SIZE_C)
        {
        	radio_rx_pointer = 0;
        }
    }
}


/* Private functions ---------------------------------------------------------*/

/**
 * @brief Processes a received string line from radio device
 * 
 * @param p_line Received line
 */
static void radio_process_rx_line(char* p_line)
{
	// printf("Radio line: '%s'\n", p_line);
	memmove(radio_last_rx_msg[2], radio_last_rx_msg[1], RADIO_BUF_SIZE_C);
	memmove(radio_last_rx_msg[1], radio_last_rx_msg[0], RADIO_BUF_SIZE_C);
	memmove(radio_last_rx_msg[0], radio_rx_buffer, RADIO_BUF_SIZE_C);
}

/*** END OF FILE ***/
