/**
 * @file      mod_ctrl_mgr.h
 * @author    The OSLUV Project
 * @brief     Functions prototypes for Control Manager module
 *  
 */

#ifndef _M_CTRL_H_
#define _M_CTRL_H_


/* Exported includes ---------------------------------------------------------*/

#include <stdint.h>


/* Exported variables --------------------------------------------------------*/
/* Exported functions prototypes ---------------------------------------------*/

void mod_ctrl_init(void);
void mod_ctrl_manager(void);
void mod_ctrl_perform_lamp_retest(void);


#endif /* _M_CTRL_H_ */

/*** END OF FILE ***/