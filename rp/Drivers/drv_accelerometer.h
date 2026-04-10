/**
 * @file      drv_accelerometer.h
 * @author    The OSLUV Project
 * @brief     Functions prototypes for accelerometer device driver
 *  
 */

#ifndef _D_ACC_H_
#define _D_ACC_H_


/* Exported variables --------------------------------------------------------*/

extern float g_drv_acc_x, g_drv_acc_y, g_drv_acc_z;


/* Exported functions prototypes ---------------------------------------------*/

void drv_acc_init(void);
void drv_acc_update(void);
int drv_acc_get_pointing_down_angle(void);


#endif /* _D_ACC_H_ */

/*** END OF FILE ***/
