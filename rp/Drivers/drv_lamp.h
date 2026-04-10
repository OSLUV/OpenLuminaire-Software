/**
 * @file      drv_lamp.h
 * @author    The OSLUV Project
 * @brief     Functions prototypes for lamp state control driver
 *  
 */

#ifndef _D_LAMP_H_
#define _D_LAMP_H_


/* Exported includes ---------------------------------------------------------*/

#include <stdint.h>
#include <stdbool.h>
#include "Drivers/drv_lamp_types.h"


/* Exported typedef ----------------------------------------------------------*/
/* Exported functions prototypes ---------------------------------------------*/

void drv_lamp_init(void);
void drv_lamp_update(void);

void drv_lamp_load_type_from_flash(void);
D_LAMP_TYPE_E drv_lamp_get_type(void);
void drv_lamp_perform_type_test(void);
void drv_lamp_reset_type(void);

void drv_lamp_set_switched_12v(bool on);
void drv_lamp_set_switched_24v(bool on);
bool drv_lamp_get_switched_12v(void);
bool drv_lamp_get_switched_24v(void);
void drv_lamp_power_up_rails(void);

bool drv_lamp_request_power_level(D_LAMP_PWR_LEVEL_E pwr_level);
D_LAMP_PWR_LEVEL_E drv_lamp_get_requested_power_level(void);
D_LAMP_PWR_LEVEL_E drv_lamp_get_commanded_power_level(void);
bool drv_lamp_get_reported_power_level(D_LAMP_PWR_LEVEL_E *p_pwr_level);
bool drv_lamp_is_power_ok(void);
const char* drv_lamp_get_power_level_string(D_LAMP_PWR_LEVEL_E pwr_level);

int drv_lamp_get_raw_freq(void);
D_LAMP_STATE_E drv_lamp_get_lamp_state(void);
const char* drv_lamp_get_lamp_state_str(D_LAMP_STATE_E state);
int drv_lamp_get_state_elapsed_ms(void);
bool drv_lamp_is_warming(void);


#endif /* _D_LAMP_H_ */

/*** END OF FILE ***/
