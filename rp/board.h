/**
 * @file      board.h
 * @author    The OSLUV Project
 * @brief     Board version detection and configuration
 *
 * Detects V1.1 vs V1.2 controller PCB at startup and provides
 * board-specific parameters. Fail-safe: defaults to V1.1 if
 * detection is ambiguous (V1.1 behavior is safe on both boards).
 */

#ifndef _BOARD_H_
#define _BOARD_H_


/* Exported includes ---------------------------------------------------------*/

#include <stdbool.h>


/* Exported functions prototypes ---------------------------------------------*/

void board_init(void);
bool board_is_v1_2(void);
int  board_get_pdo_mv(void);
int  board_get_pdo_ma(void);


#endif /* _BOARD_H_ */

/*** END OF FILE ***/
