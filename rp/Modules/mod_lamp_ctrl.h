/**
 * @file      mod_lamp_ctrl.h
 * @author    The OSLUV Project
 * @brief     Functions prototypes for lamp control module
 *  
 */

#ifndef _M_LAMP_H_
#define _M_LAMP_H_


/* Exported includes ---------------------------------------------------------*/

#include <stdint.h>
#include <stdbool.h>
#include "Modules/mod_lamp_types.h"


/* Exported typedef ----------------------------------------------------------*/
/* Exported functions prototypes ---------------------------------------------*/

void mod_lamp_init(void);
void mod_lamp_ctrl_handler(void);

int8_t mod_lamp_perform_type_test(void);
void mod_lamp_reset_type(void);


#endif /* _M_LAMP_H_ */

/*** END OF FILE ***/
