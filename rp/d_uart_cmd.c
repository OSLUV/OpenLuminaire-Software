/**
 * @file      d_uart_cmd.c
 * @author    The OSLUV Project
 * @brief     Driver for external command's UART peripheral
 * @schematic lamp_controller.SchDoc
 *  
 */


/* Includes ------------------------------------------------------------------*/

#include <hardware/uart.h>
#include <hardware/irq.h>
#include <pico/stdlib.h>
#include "d_uart_cmd.h"


/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

#define UART_CMD_PORT_C             uart1
#define UART_CMD_TX_PIN_C           9                                           /* UART_RX */
#define UART_CMD_RX_PIN_C           8                                           /* UART_TX */
#define UART_CMD_BAUDRATE_C         9600
#define UART_CMD_IRQ_C              UART1_IRQ

#define UART_CMD_BUF_MAX_DATA_LEN_C 1024

#if (UART_CMD_BUF_MAX_DATA_LEN_C == 0) || \
    (UART_CMD_BUF_MAX_DATA_LEN_C & (UART_CMD_BUF_MAX_DATA_LEN_C - 1))
#warning "UART commands reception buffer size is not a base 2 size as expected."
#endif


/* Global variables  ---------------------------------------------------------*/
/* Private variables  --------------------------------------------------------*/

volatile struct {
    uint16_t head;
    uint16_t tail;
    uint8_t  data[UART_CMD_BUF_MAX_DATA_LEN_C];
} uart_cmd_buf;


/* Callback prototypes -------------------------------------------------------*/

static void __isr uart_cmd_rx_isr(void);


/* Private function prototypes -----------------------------------------------*/

static inline void uart_cmd_put_char(uint8_t data);
static inline char uart_cmd_get_char(void);
static inline void uart_cmd_rx_flush(void);
static inline void uart_cmd_enable_rx_isr(void);
static inline void uart_cmd_disable_rx_isr(void);


/* Exported functions --------------------------------------------------------*/

/**
 * @brief Commands UART peripheral driver initialization procedure
 * 
 */
void uart_cmd_init(void)
{
    uart_cmd_buf.head = 0;
    uart_cmd_buf.tail = 0;

    uart_init(UART_CMD_PORT_C, UART_CMD_BAUDRATE_C);

    gpio_set_function(UART_CMD_TX_PIN_C, GPIO_FUNC_UART);
    gpio_set_function(UART_CMD_RX_PIN_C, GPIO_FUNC_UART); 

    uart_set_format(UART_CMD_PORT_C, 8, 1, UART_PARITY_NONE);

    uart_set_fifo_enabled(UART_CMD_PORT_C, true);

    uart_cmd_rx_flush();

    irq_set_exclusive_handler(UART_CMD_IRQ_C, uart_cmd_rx_isr);
    uart_cmd_enable_rx_isr();

    uart_set_irqs_enabled(UART_CMD_PORT_C, true, false);
}

/**
 * @brief Handles the sending of a required amount of data bytes through the 
 * commands UART peripheral
 * 
 * @param p_data_buf Data to send in bytes
 * @param data_len   Data bytes length to send
 */
void uart_cmd_send_data(uint8_t *p_data_buf, uint16_t data_len)
{
    for (uint16_t idx = 0; idx < data_len; idx++)
    {
        uart_putc_raw(UART_CMD_PORT_C, p_data_buf[idx]);
    }
}

/**
 * @brief Handles data pull from the local receiving data buffer
 * 
 * @param p_data_buf  Data buffer to deliver data
 * @param data_len    Data bytes length to pull
 * @return uint16_t Data bytes length pulled from local buffer
 */
uint16_t uart_cmd_get_data(uint8_t *p_data_buf, uint16_t data_len)
{
    uint16_t count;

    count = 0;
    for (uint16_t idx = 0; idx < data_len; idx++)
    {        
        if (uart_cmd_buf.tail == uart_cmd_buf.head)                             /* Buffer is empty? */
        {
            return count;
        }

        uart_cmd_disable_rx_isr();

        p_data_buf[idx] = uart_cmd_buf.data[uart_cmd_buf.tail];

        uart_cmd_buf.tail++;
        uart_cmd_buf.tail &= (UART_CMD_BUF_MAX_DATA_LEN_C - 1);

        uart_cmd_enable_rx_isr();

        count++;
    }

    return count;
}

/**
 * @brief Returns how many data bytes are available at local receiving buffer
 * 
 * @return uint16_t Data bytes length available in local buffer
 */
uint16_t uart_cmd_get_rcvd_data_len(void)
{
  uint16_t length;
  uint16_t head, tail;
    
  tail = uart_cmd_buf.tail;
  head = uart_cmd_buf.head;
  
  length = 0;
  if (head > tail)
  {
    length = head - tail;
  } 
  else if (tail > head) 
  {
    length  = UART_CMD_BUF_MAX_DATA_LEN_C - tail;
    length += head;
  }

  return length;
}

/**
 * @brief Flushes local buffer by reseting head and tail indexes
 * 
 */
void uart_cmd_flush(void)
{
    uart_cmd_disable_rx_isr();

    uart_cmd_buf.tail = 0;
    uart_cmd_buf.head = 0;

    uart_cmd_enable_rx_isr();
}


/* Callback functions --------------------------------------------------------*/

/**
 * @brief UART peripheral receiving ISR. It puts received data on a local 
 * circular buffer
 * 
 */
static void __isr uart_cmd_rx_isr(void)
{
    while (uart_is_readable(UART_CMD_PORT_C)) 
    {
        uart_cmd_buf.data[uart_cmd_buf.head] = uart_cmd_get_char();

        uart_cmd_buf.head++;
        uart_cmd_buf.head &= (UART_CMD_BUF_MAX_DATA_LEN_C - 1);

        if (uart_cmd_buf.head == uart_cmd_buf.tail)                             /* Buffer is full ? */
        {
            uart_cmd_buf.tail &= (UART_CMD_BUF_MAX_DATA_LEN_C - 1);
        }
    }
}


/* Private functions ---------------------------------------------------------*/

/**
 * @brief Low level function for sending a single character over UART peripheral
 * 
 * @param data  Single character to send
 */
static inline void uart_cmd_put_char(uint8_t data)
{
    uart_putc_raw(UART_CMD_PORT_C, data);
}

/**
 * @brief Low level function for getting a single character from UART peripheral
 * 
 * @return char  Single character received
 */
static inline char uart_cmd_get_char(void)
{
    if (uart_is_readable(UART_CMD_PORT_C)) 
    {
        return uart_getc(UART_CMD_PORT_C);;
    }

    return 0;   
}

/**
 * @brief Flushes the UART peripheral receiving buffer
 * 
 */
static inline void uart_cmd_rx_flush(void)
{
    while (uart_is_readable(UART_CMD_PORT_C)) 
    {
        (void)uart_getc(UART_CMD_PORT_C);
    }
}

/**
 * @brief Enables UART RX ISR
 * 
 */
static inline void uart_cmd_enable_rx_isr(void)
{
    irq_set_enabled(UART_CMD_IRQ_C, true);
}

/**
 * @brief Disables UART RX ISR
 * 
 */
static inline void uart_cmd_disable_rx_isr(void)
{
    irq_set_enabled(UART_CMD_IRQ_C, false);
}

/*** END OF FILE ***/