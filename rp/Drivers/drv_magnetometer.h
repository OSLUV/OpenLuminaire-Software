/**
 * @file      drv_magnetometer.h
 * @author    The OSLUV Project
 * @brief     Functions prototypes for magnet sensor driver
 *  
 */

#ifndef _D_MAG_H_
#define _D_MAG_H_


/* Exported includes ---------------------------------------------------------*/

#include <stdint.h>


/* Exported variables --------------------------------------------------------*/

extern int16_t g_drv_mag_x, g_drv_mag_y, g_drv_mag_z;


/* Exported functions prototypes ---------------------------------------------*/

void drv_mag_init(void);
void drv_mag_update(void);


#endif /* _D_MAG_H_ */

/*** END OF FILE ***/
