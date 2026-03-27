/**
 * @file      mod_ser_cmd.h
 * @author    The OSLUV Project
 * @brief     Functions prototypes for external serial commands module
 *  
 */

#ifndef _M_SER_CMD_H_
#define _M_SER_CMD_H_


/* Exported includes ---------------------------------------------------------*/

#include <stdint.h>


/* Exported variables --------------------------------------------------------*/
/* Exported functions prototypes ---------------------------------------------*/

void mod_cmd_init(void);
void mod_cmd_handler(void);


#endif /* _M_SER_CMD_H_ */

/*** END OF FILE ***/