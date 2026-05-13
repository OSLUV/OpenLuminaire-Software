/**
 * @file      drv_lamp.h
 * @author    The OSLUV Project
 * @brief     Functions prototypes for lamp state control driver
 *  
 */

#ifndef _D_LAMP_H_
#define _D_LAMP_H_


/* Exported includes ---------------------------------------------------------*/

#include <stdint.h>
#include <stdbool.h>


/* Exported typedef ----------------------------------------------------------*/
/* Exported functions prototypes ---------------------------------------------*/

void drv_lamp_init(void);
void drv_lamp_enable(void);
void drv_lamp_disable(void);
bool drv_lamp_is_enabled(void);
void drv_lamp_set_pwm_level(uint16_t level);
bool drv_lamp_is_status_on(void);
uint32_t drv_lamp_get_status_evts_count(void);


#endif /* _D_LAMP_H_ */

/*** END OF FILE ***/
