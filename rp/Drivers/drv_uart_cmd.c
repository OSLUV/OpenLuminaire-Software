/**
 * @file      drv_uart_cmd.c
 * @author    The OSLUV Project
 * @brief     Driver for external command's UART peripheral
 * @schematic lamp_controller.SchDoc
 *  
 */


/* Includes ------------------------------------------------------------------*/

#include <hardware/uart.h>
#include <hardware/irq.h>
#include <pico/stdlib.h>
#include "Drivers/drv_uart_cmd.h"


/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

#define D_UART_CMD_PORT_C             uart1
#define D_UART_CMD_TX_PIN_C           9                                         /* UART_RX */
#define D_UART_CMD_RX_PIN_C           8                                         /* UART_TX */
#define D_UART_CMD_BAUDRATE_C         9600
#define D_UART_CMD_IRQ_C              UART1_IRQ

#define D_UART_CMD_BUF_MAX_DATA_LEN_C 1024

#if (D_UART_CMD_BUF_MAX_DATA_LEN_C == 0) || \
    (D_UART_CMD_BUF_MAX_DATA_LEN_C & (D_UART_CMD_BUF_MAX_DATA_LEN_C - 1))
#warning "UART commands reception buffer size is not a base 2 size as expected."
#endif


/* Global variables  ---------------------------------------------------------*/
/* Private variables  --------------------------------------------------------*/

volatile struct {
    uint16_t head;
    uint16_t tail;
    uint8_t  data[D_UART_CMD_BUF_MAX_DATA_LEN_C];
} drv_uart_cmd_buf;


/* Callback prototypes -------------------------------------------------------*/

static void __isr drv_uart_cmd_rx_isr(void);


/* Private function prototypes -----------------------------------------------*/

static inline void drv_uart_cmd_put_char(uint8_t data);
static inline char drv_uart_cmd_get_char(void);
static inline void drv_uart_cmd_rx_flush(void);
static inline void drv_uart_cmd_enable_rx_isr(void);
static inline void drv_uart_cmd_disable_rx_isr(void);


/* Exported functions --------------------------------------------------------*/

/**
 * @brief Commands UART peripheral driver initialization procedure
 * 
 */
void drv_uart_cmd_init(void)
{
    drv_uart_cmd_buf.head = 0;
    drv_uart_cmd_buf.tail = 0;

    uart_init(D_UART_CMD_PORT_C, D_UART_CMD_BAUDRATE_C);

    gpio_set_function(D_UART_CMD_TX_PIN_C, GPIO_FUNC_UART);
    gpio_set_function(D_UART_CMD_RX_PIN_C, GPIO_FUNC_UART); 

    uart_set_format(D_UART_CMD_PORT_C, 8, 1, UART_PARITY_NONE);

    uart_set_fifo_enabled(D_UART_CMD_PORT_C, true);

    drv_uart_cmd_rx_flush();

    irq_set_exclusive_handler(D_UART_CMD_IRQ_C, drv_uart_cmd_rx_isr);
    drv_uart_cmd_enable_rx_isr();

    uart_set_irqs_enabled(D_UART_CMD_PORT_C, true, false);
}

/**
 * @brief Handles the sending of a required amount of data bytes through the 
 * commands UART peripheral
 * 
 * @param p_data_buf Data to send in bytes
 * @param data_len   Data bytes length to send
 */
void drv_uart_cmd_send_data(uint8_t *p_data_buf, uint16_t data_len)
{
    for (uint16_t idx = 0; idx < data_len; idx++)
    {
        uart_putc_raw(D_UART_CMD_PORT_C, p_data_buf[idx]);
    }
}

/**
 * @brief Handles data pull from the local receiving data buffer
 * 
 * @param p_data_buf  Data buffer to deliver data
 * @param data_len    Data bytes length to pull
 * @return uint16_t Data bytes length pulled from local buffer
 */
uint16_t drv_uart_cmd_get_data(uint8_t *p_data_buf, uint16_t data_len)
{
    uint16_t count;

    count = 0;
    for (uint16_t idx = 0; idx < data_len; idx++)
    {        
        if (drv_uart_cmd_buf.tail == drv_uart_cmd_buf.head)                             /* Buffer is empty? */
        {
            return count;
        }

        drv_uart_cmd_disable_rx_isr();

        p_data_buf[idx] = drv_uart_cmd_buf.data[drv_uart_cmd_buf.tail];

        drv_uart_cmd_buf.tail++;
        drv_uart_cmd_buf.tail &= (D_UART_CMD_BUF_MAX_DATA_LEN_C - 1);

        drv_uart_cmd_enable_rx_isr();

        count++;
    }

    return count;
}

/**
 * @brief Returns how many data bytes are available at local receiving buffer
 * 
 * @return uint16_t Data bytes length available in local buffer
 */
uint16_t drv_uart_cmd_get_rcvd_data_len(void)
{
  uint16_t length;
  uint16_t head, tail;
    
  tail = drv_uart_cmd_buf.tail;
  head = drv_uart_cmd_buf.head;
  
  length = 0;
  if (head > tail)
  {
    length = head - tail;
  } 
  else if (tail > head) 
  {
    length  = D_UART_CMD_BUF_MAX_DATA_LEN_C - tail;
    length += head;
  }

  return length;
}

/**
 * @brief Flushes local buffer by reseting head and tail indexes
 * 
 */
void drv_uart_cmd_flush(void)
{
    drv_uart_cmd_disable_rx_isr();

    drv_uart_cmd_buf.tail = 0;
    drv_uart_cmd_buf.head = 0;

    drv_uart_cmd_enable_rx_isr();
}


/* Callback functions --------------------------------------------------------*/

/**
 * @brief UART peripheral receiving ISR. It puts received data on a local 
 * circular buffer
 * 
 */
static void __isr drv_uart_cmd_rx_isr(void)
{
    while (uart_is_readable(D_UART_CMD_PORT_C)) 
    {
        drv_uart_cmd_buf.data[drv_uart_cmd_buf.head] = drv_uart_cmd_get_char();

        drv_uart_cmd_buf.head++;
        drv_uart_cmd_buf.head &= (D_UART_CMD_BUF_MAX_DATA_LEN_C - 1);

        if (drv_uart_cmd_buf.head == drv_uart_cmd_buf.tail)                             /* Buffer is full ? */
        {
            drv_uart_cmd_buf.tail &= (D_UART_CMD_BUF_MAX_DATA_LEN_C - 1);
        }
    }
}


/* Private functions ---------------------------------------------------------*/

/**
 * @brief Low level function for sending a single character over UART peripheral
 * 
 * @param data  Single character to send
 */
static inline void drv_uart_cmd_put_char(uint8_t data)
{
    uart_putc_raw(D_UART_CMD_PORT_C, data);
}

/**
 * @brief Low level function for getting a single character from UART peripheral
 * 
 * @return char  Single character received
 */
static inline char drv_uart_cmd_get_char(void)
{
    if (uart_is_readable(D_UART_CMD_PORT_C)) 
    {
        return uart_getc(D_UART_CMD_PORT_C);;
    }

    return 0;   
}

/**
 * @brief Flushes the UART peripheral receiving buffer
 * 
 */
static inline void drv_uart_cmd_rx_flush(void)
{
    while (uart_is_readable(D_UART_CMD_PORT_C)) 
    {
        (void)uart_getc(D_UART_CMD_PORT_C);
    }
}

/**
 * @brief Enables UART RX ISR
 * 
 */
static inline void drv_uart_cmd_enable_rx_isr(void)
{
    irq_set_enabled(D_UART_CMD_IRQ_C, true);
}

/**
 * @brief Disables UART RX ISR
 * 
 */
static inline void drv_uart_cmd_disable_rx_isr(void)
{
    irq_set_enabled(D_UART_CMD_IRQ_C, false);
}

/*** END OF FILE ***/