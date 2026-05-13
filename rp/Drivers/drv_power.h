/**
 * @file      drv_power.h
 * @author    The OSLUV Project
 * @brief     Functions prototypes for power rails control driver
 *  
 */

#ifndef _D_POWER_H_
#define _D_POWER_H_


/* Exported includes ---------------------------------------------------------*/
/* Exported typedef ----------------------------------------------------------*/
/* Exported functions prototypes ---------------------------------------------*/

void drv_power_init(void);
void drv_power_set_switched_12v_level(uint16_t level);
void drv_power_set_switched_24v(bool on);


#endif /* _D_POWER_H_ */

/*** END OF FILE ***/
