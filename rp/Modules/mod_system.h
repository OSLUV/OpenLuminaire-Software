/**
 * @file      mod_system.h
 * @author    The OSLUV Project
 * @brief     Functions prototypes for System module
 *  
 */

#ifndef _M_SYS_H_
#define _M_SYS_H_


/* Exported includes ---------------------------------------------------------*/

#include <stdint.h>


/* Exported variables --------------------------------------------------------*/
/* Exported functions prototypes ---------------------------------------------*/

void mod_sys_init(void);
void mod_sys_startup_wdt(void);
void mod_sys_services(void);


#endif /* _M_SYS_H_ */

/*** END OF FILE ***/