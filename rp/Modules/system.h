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

    uint32_t lamp_test_b          : 1;                                          /* Perform lamp test */
    uint32_t lamp_test_n_reboot_b : 1;                                          /* Perform lamp test and reboots system when finished */
    uint32_t bit2_b  : 1;
    uint32_t bit3_b  : 1;
    uint32_t bit4_b  : 1;
    uint32_t bit5_b  : 1;
    uint32_t bit6_b  : 1;
    uint32_t bit7_b  : 1;
    uint32_t bit8_b  : 1;
    uint32_t bit9_b  : 1;
    uint32_t bit10_b : 1;
    uint32_t bit11_b : 1;
    uint32_t bit12_b : 1;
    uint32_t bit13_b : 1;
    uint32_t bit14_b : 1;
    uint32_t bit15_b : 1;
    uint32_t bit16_b : 1;
    uint32_t bit17_b : 1;
    uint32_t bit18_b : 1;
    uint32_t bit19_b : 1;
    uint32_t bit20_b : 1;
    uint32_t bit21_b : 1;
    uint32_t bit22_b : 1;
    uint32_t bit23_b : 1;
    uint32_t bit24_b : 1;
    uint32_t bit25_b : 1;
    uint32_t bit26_b : 1;
    uint32_t bit27_b : 1;
    uint32_t bit28_b : 1;
    uint32_t bit29_b : 1;
    uint32_t bit30_b : 1;
    uint32_t reboot_b : 1;

} SYS_TASKS_T;

typedef struct {

    SYS_TASKS_T task;
    float       v_vbus;
    float       v_12v;
    float       v_24v;
    float       acc_x;
    float       acc_y;
    float       acc_z;
    int16_t     acc_pointing_down_angle;
    int16_t     mag_x;
    int16_t     mag_y;
    int16_t     mag_z;

} SYS_STATUS_T;

typedef struct {

    SYS_TASKS_T task;

} SYS_CTRL_T;

/* Exported variables --------------------------------------------------------*/

extern SYS_STATUS_T g_sys_stt;
extern SYS_CTRL_T   g_sys_ctl;


#endif /* _SYS_H_ */

/*** END OF FILE ***/