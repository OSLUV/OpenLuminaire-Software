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


/* Exported typedef ----------------------------------------------------------*/
/* Exported variables --------------------------------------------------------*/
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
