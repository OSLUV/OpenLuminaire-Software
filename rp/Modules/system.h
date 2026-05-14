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
#include "Modules/mod_lamp_defs.h"


/* Exported typedefs ---------------------------------------------------------*/

typedef struct {

    uint32_t lamp_on            : 1;
    uint32_t lamp_test_b        : 1;                                            /* Perform lamp test */
    uint32_t lamp_test_n_reboot : 1;                                            /* Perform lamp test and reboots system when finished */
    uint32_t rails_on           : 1;
    uint32_t rails_off          : 1;
    uint32_t radar_on           : 1;
    uint32_t bit6  : 1;
    uint32_t bit7  : 1;
    uint32_t bit8  : 1;
    uint32_t bit9  : 1;
    uint32_t bit10 : 1;
    uint32_t bit11 : 1;
    uint32_t bit12 : 1;
    uint32_t bit13 : 1;
    uint32_t bit14 : 1;
    uint32_t bit15 : 1;
    uint32_t bit16 : 1;
    uint32_t bit17 : 1;
    uint32_t bit18 : 1;
    uint32_t bit19 : 1;
    uint32_t bit20 : 1;
    uint32_t bit21 : 1;
    uint32_t bit22 : 1;
    uint32_t bit23 : 1;
    uint32_t bit24 : 1;
    uint32_t bit25 : 1;
    uint32_t bit26 : 1;
    uint32_t bit27 : 1;
    uint32_t bit28 : 1;
    uint32_t bit29 : 1;
    uint32_t save_cfg           : 1;
    uint32_t reboot             : 1;

} SYS_TASKS_T;

typedef struct {

    M_LAMP_TYPE_E       lamp_type;
    M_LAMP_STATE_E 	    lamp_state;
    uint64_t            lamp_elap_ms_time;
    uint32_t            lamp_latched_freq_hz;
    M_LAMP_PWR_LEVEL_E  lamp_power_level;
    M_LAMP_PWR_LEVEL_E  lamp_cmd_power_level;
    uint8_t             is_lamp_warming;
    uint8_t             dummy_0;
    uint8_t             hw_is_1_2;
    uint8_t             is_rails_powering_on;
    uint8_t             is_rails_powering_off;
    uint8_t             is_12v_rail_on;
    uint8_t             is_24v_rail_on;
    uint8_t             is_power_ok;
    SYS_TASKS_T         task;
    float               v_vbus;
    float               v_12v;
    float               v_24v;
    float               acc_x;
    float               acc_y;
    float               acc_z;
    int16_t             acc_pointing_down_angle;
    int16_t             mag_x;
    int16_t             mag_y;
    int16_t             mag_z;

} SYS_STATUS_T;

typedef struct {

    SYS_TASKS_T         task;
    M_LAMP_PWR_LEVEL_E  lamp_req_pwr_level;
    uint32_t            ui_dim_index;

} SYS_CTRL_T;

/* Exported variables --------------------------------------------------------*/

extern SYS_STATUS_T     g_sys_stt;
extern SYS_CTRL_T       g_sys_ctl;


#endif /* _SYS_H_ */

/*** END OF FILE ***/