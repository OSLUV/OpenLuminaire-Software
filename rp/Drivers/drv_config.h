/**
 * @file      drv_config.h
 * @author    The OSLUV Project
 * @brief     Functions prototypes for system configuration driver
 *  
 */

#ifndef _D_CONFIG_H_
#define _D_CONFIG_H_


/* Exported includes ---------------------------------------------------------*/

#include <stdint.h>
#include "Drivers/drv_lamp.h"


/* Exported typedef ----------------------------------------------------------*/

typedef struct __packed {
    uint32_t magic;          /* guard */
    uint8_t  power_on;       /* 1 = lamp on */
    uint8_t  radar_on;       /* 1 = radar enabled */
    uint8_t  dim_index;      /* 0–3  (20/40/70/100 %) */
	uint8_t  factory_lamp_type;
} D_CFG_DATA_T;


/* Exported variables --------------------------------------------------------*/

extern D_CFG_DATA_T g_drv_cfg;


/* Exported functions prototypes ---------------------------------------------*/

void drv_cfg_init(void);
void drv_cfg_read(void);
void drv_cfg_save(void);

void drv_cfg_set_power_state(bool b_pwr_on);
bool drv_cfg_get_power_state(void);
void drv_cfg_set_radar_state(bool b_radar_on);
bool drv_cfg_get_radar_state(void);
void drv_cfg_set_dim_index(uint8_t idx);
uint8_t drv_cfg_get_dim_index(void);
void drv_cfg_set_factory_lamp_type(uint8_t type);
uint8_t drv_cfg_get_factory_lamp_type(void);


#endif /* _D_CONFIG_H_ */

/*** END OF FILE ***/
