/**
 * @file      d_uart_cmd.h
 * @author    The OSLUV Project
 * @brief     Functions prototypes for external command's UART peripheral driver
 *  
 */

#ifndef _D_UART_CMD_H_
#define _D_UART_CMD_H_


/* Exported includes ---------------------------------------------------------*/

#include <stdint.h>


/* Exported defines ----------------------------------------------------------*/

#define UART_CMD_INTERBYTE_TIMEOUT_US_C  50000U

#define UART_CMD_RX_FLAG_GAP_BEFORE_C    (1U << 0)
#define UART_CMD_RX_FLAG_DATA_LOSS_C     (1U << 1)


/* Exported variables --------------------------------------------------------*/
/* Exported functions prototypes ---------------------------------------------*/

void uart_cmd_init(void);
void uart_cmd_send_data(const uint8_t *p_data_buf, uint16_t data_len);
uint16_t uart_cmd_get_data(uint8_t *p_data_buf, uint8_t *p_rx_flags, uint16_t data_len);
uint16_t uart_cmd_get_rcvd_data_len(void);
void uart_cmd_flush(void);
void uart_cmd_discard_rx_after_blocking(void);


#endif /* _D_UART_CMD_H_ */

/*** END OF FILE ***/
