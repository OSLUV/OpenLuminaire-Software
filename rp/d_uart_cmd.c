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
    uint8_t  rx_flags[UART_CMD_BUF_MAX_DATA_LEN_C];
    uint64_t last_rx_us;
    bool     has_last_rx;
    bool     mark_data_loss;
} uart_cmd_buf;


/* Callback prototypes -------------------------------------------------------*/

static void __isr uart_cmd_rx_isr(void);


/* Private function prototypes -----------------------------------------------*/

static inline uint8_t uart_cmd_get_char(void);
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
    uart_cmd_buf.head           = 0;
    uart_cmd_buf.tail           = 0;
    uart_cmd_buf.last_rx_us     = 0;
    uart_cmd_buf.has_last_rx    = false;
    uart_cmd_buf.mark_data_loss = false;

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
void uart_cmd_send_data(const uint8_t *p_data_buf, uint16_t data_len)
{
    for (uint16_t idx = 0; idx < data_len; idx++)
    {
        uart_putc_raw(UART_CMD_PORT_C, (char)p_data_buf[idx]);
    }
}

/**
 * @brief Handles data pull from the local receiving data buffer
 * 
 * @param p_data_buf Data buffer to deliver data
 * @param p_rx_flags Per-byte receive metadata buffer
 * @param data_len   Data bytes length to pull
 * @return uint16_t Data bytes length pulled from local buffer
 */
uint16_t uart_cmd_get_data(uint8_t *p_data_buf, uint8_t *p_rx_flags, uint16_t data_len)
{
    uint16_t count;

    count = 0;
    for (uint16_t idx = 0; idx < data_len; idx++)
    {
        uart_cmd_disable_rx_isr();

        if (uart_cmd_buf.tail == uart_cmd_buf.head)                             /* Buffer is empty? */
        {
            uart_cmd_enable_rx_isr();
            return count;
        }

        p_data_buf[idx] = uart_cmd_buf.data[uart_cmd_buf.tail];
        p_rx_flags[idx] = uart_cmd_buf.rx_flags[uart_cmd_buf.tail];

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

    uart_cmd_buf.tail           = 0;
    uart_cmd_buf.head           = 0;
    uart_cmd_buf.last_rx_us     = 0;
    uart_cmd_buf.has_last_rx    = false;
    uart_cmd_buf.mark_data_loss = false;
    uart_cmd_rx_flush();

    uart_cmd_enable_rx_isr();
}

/**
 * @brief Discards RX made ambiguous while interrupts were blocked
 *
 * Flash erase/program masks interrupts while the UART FIFO remains active.
 * Bytes separated on the wire can therefore be drained together and receive
 * misleading ISR timestamps. Drop everything observed across that blackout
 * and mark the next byte as data loss. Only a new physical idle gap can make
 * that next byte a trusted line boundary; otherwise the parser discards its
 * line through the terminator.
 */
void uart_cmd_discard_rx_after_blocking(void)
{
    bool discarded_data;

    uart_cmd_disable_rx_isr();

    discarded_data    = uart_cmd_buf.tail != uart_cmd_buf.head;
    uart_cmd_buf.tail = 0;
    uart_cmd_buf.head = 0;

    while (uart_is_readable(UART_CMD_PORT_C))
    {
        (void)uart_getc(UART_CMD_PORT_C);
        discarded_data = true;
    }

    if (discarded_data)
    {
        uart_cmd_buf.last_rx_us     = time_us_64();
        uart_cmd_buf.has_last_rx    = true;
        uart_cmd_buf.mark_data_loss = true;
    }

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
        uint64_t now_us;
        uint8_t  rx_flags;

        now_us   = time_us_64();
        rx_flags = 0;

        if (uart_cmd_buf.mark_data_loss)
        {
            rx_flags |= UART_CMD_RX_FLAG_DATA_LOSS_C;
            uart_cmd_buf.mark_data_loss = false;
        }

        if (uart_cmd_buf.has_last_rx &&
            ((now_us - uart_cmd_buf.last_rx_us) > UART_CMD_INTERBYTE_TIMEOUT_US_C))
        {
            rx_flags |= UART_CMD_RX_FLAG_GAP_BEFORE_C;
        }

        uart_cmd_buf.last_rx_us  = now_us;
        uart_cmd_buf.has_last_rx = true;

        uart_cmd_buf.data[uart_cmd_buf.head] = uart_cmd_get_char();
        uart_cmd_buf.rx_flags[uart_cmd_buf.head] = rx_flags;

        uart_cmd_buf.head++;
        uart_cmd_buf.head &= (UART_CMD_BUF_MAX_DATA_LEN_C - 1);

        if (uart_cmd_buf.head == uart_cmd_buf.tail)                             /* Buffer is full ? */
        {
            uart_cmd_buf.tail = (uart_cmd_buf.tail + 1) & (UART_CMD_BUF_MAX_DATA_LEN_C - 1); /* Drop oldest byte */
            uart_cmd_buf.rx_flags[uart_cmd_buf.tail] |= UART_CMD_RX_FLAG_DATA_LOSS_C;
        }
    }
}


/* Private functions ---------------------------------------------------------*/

/**
 * @brief Low level function for getting a single character from UART peripheral
 * 
 * @return uint8_t Single character received
 */
static inline uint8_t uart_cmd_get_char(void)
{
    if (uart_is_readable(UART_CMD_PORT_C)) 
    {
        return (uint8_t)uart_getc(UART_CMD_PORT_C);
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
