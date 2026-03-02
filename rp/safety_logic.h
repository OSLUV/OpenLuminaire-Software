/**
 * @file      safety_logic.h
 * @author    The OSLUV Project
 * @brief     Functions prototypes for safety logic module
 *  
 */

#ifndef _SAFETY_LOGIC_H_
#define _SAFETY_LOGIC_H_


/* Exported includes ---------------------------------------------------------*/

#include "lamp.h"


/* Exported functions prototypes ---------------------------------------------*/

bool safety_logic_is_high_tilt(void);
void safety_logic_update(void);
char* safety_logic_get_state_desc(void);
void safety_logic_set_radar_enabled_state(bool b_enable);
bool safety_logic_get_radar_enabled_state(void);
void safety_logic_toggle_radar_enabled_state(void);
void safety_logic_set_cap_power(LAMP_PWR_LEVEL_E pwr_level);


#endif /* _SAFETY_LOGIC_H_ */

/*** END OF FILE ***/
