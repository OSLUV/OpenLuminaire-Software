/**
 * @file      system.h
 * @author    The OSLUV Project
 * @brief     System's global status
 *  
 */

#ifndef _SYS_H_
#define _SYS_H_


/* Exported includes ---------------------------------------------------------*/

#include <stdbool.h>
#include <stdint.h>


/* Exported includes ---------------------------------------------------------*/

typedef struct {

    float v_vbus;
    float v_12v;
    float v_24v;
    float acc_x;
    float acc_y;
    float acc_z;
    int16_t acc_pointing_down_angle;
    int16_t mag_x;
    int16_t mag_y;
    int16_t mag_z;

} SYS_STATUS_T;

/* Exported variables --------------------------------------------------------*/

extern SYS_STATUS_T g_sys;


#endif /* _SYS_H_ */

/*** END OF FILE ***/