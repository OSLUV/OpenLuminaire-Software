/**
 * @file      drv_fan.h
 * @author    The OSLUV Project
 * @brief     Functions prototypes for external fan driver
 *  
 */

#ifndef _D_FAN_H_
#define _D_FAN_H_


/* Exported functions prototypes ---------------------------------------------*/

void drv_fan_init(void);
void drv_fan_set_speed(int speed);
int drv_fan_get_speed(void);


#endif /* _D_FAN_H_ */

/*** END OF FILE ***/
