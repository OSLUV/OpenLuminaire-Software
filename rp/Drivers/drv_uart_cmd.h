/**
 * @file      drv_uart_cmd.h
 * @author    The OSLUV Project
 * @brief     Functions prototypes for external command's UART peripheral driver
 *  
 */

#ifndef _DRV_UART_CMD_H_
#define _DRV_UART_CMD_H_


/* Exported includes ---------------------------------------------------------*/

#include <stdint.h>


/* Exported variables --------------------------------------------------------*/
/* Exported functions prototypes ---------------------------------------------*/

void drv_uart_cmd_init(void);
void drv_uart_cmd_send_data(uint8_t *p_data_buf, uint16_t data_len);
uint16_t drv_uart_cmd_get_data(uint8_t *p_data_buf, uint16_t data_len);
uint16_t drv_uart_cmd_get_rcvd_data_len(void);
void drv_uart_cmd_flush(void);


#endif /* _DRV_UART_CMD_H_ */

/*** END OF FILE ***/