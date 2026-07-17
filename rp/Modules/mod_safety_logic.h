/**
 * @file      mod_safety_logic.h
 * @author    The OSLUV Project
 * @brief     Functions prototypes for safety logic module
 *  
 */

#ifndef _M_SAFETY_LOGIC_H_
#define _M_SAFETY_LOGIC_H_


/* Exported includes ---------------------------------------------------------*/

#include "Modules/mod_lamp_defs.h"


/* Exported functions prototypes ---------------------------------------------*/

bool mod_safety_is_high_tilt(void);
void mod_safety_update(void);
char* mod_safety_get_state_desc(void);
void mod_safety_set_radar_enabled_state(bool b_enable);
bool mod_safety_get_radar_enabled_state(void);
void mod_safety_toggle_radar_enabled_state(void);
void mod_safety_set_cap_power(M_LAMP_PWR_LEVEL_E pwr_level);


#endif /* _M_SAFETY_LOGIC_H_ */

/*** END OF FILE ***/
