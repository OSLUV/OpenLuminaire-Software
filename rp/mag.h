/**
 * @file      mag.h
 * @author    The OSLUV Project
 * @brief     Functions prototypes for magnet sensor driver
 *  
 */

#ifndef _D_MAG_H_
#define _D_MAG_H_


/* Exported includes ---------------------------------------------------------*/

#include <stdint.h>


/* Exported variables --------------------------------------------------------*/

extern int16_t g_mag_x, g_mag_y, g_mag_z;


/* Exported functions prototypes ---------------------------------------------*/

void mag_init(void);
void mag_update(void);


#endif /* _D_MAG_H_ */

/*** END OF FILE ***/
