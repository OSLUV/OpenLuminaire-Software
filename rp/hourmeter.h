/**
 * @file      hourmeter.h
 * @author    The OSLUV Project
 * @brief     Functions prototypes for the lamp-on-time hour meter
 *
 */

#ifndef _M_HOURMETER_H_
#define _M_HOURMETER_H_


/* Exported includes ---------------------------------------------------------*/

#include <stdint.h>


/* Exported functions prototypes ---------------------------------------------*/

void hourmeter_init(void);
void hourmeter_update(void);

uint32_t hourmeter_get_on_seconds(void);
uint32_t hourmeter_get_on_hours(void);
uint32_t hourmeter_get_dim_on_seconds(uint8_t idx);
uint32_t hourmeter_get_dim_on_hours(uint8_t idx);


#endif /* _M_HOURMETER_H_ */

/*** END OF FILE ***/
