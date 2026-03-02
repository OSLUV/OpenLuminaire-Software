/**
 * @file      imu.h
 * @author    The OSLUV Project
 * @brief     Functions prototypes for accelerometer device driver
 *  
 */

#ifndef _D_IMU_H_
#define _D_IMU_H_


/* Exported variables --------------------------------------------------------*/

extern float g_imu_x, g_imu_y, g_imu_z;


/* Exported functions prototypes ---------------------------------------------*/

void imu_init(void);
void imu_update(void);
int imu_get_pointing_down_angle(void);


#endif /* _D_IMU_H_ */

/*** END OF FILE ***/
