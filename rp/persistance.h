/**
 * @file      persistence.h
 * @author    The OSLUV Project
 * @brief     Functions prototypes for 
 *  
 */

#ifndef _D_PERSISTANCE_H_
#define _D_PERSISTANCE_H_


/* Exported includes ---------------------------------------------------------*/

#include <stdint.h>
#include "lamp.h"


/* Exported typedef ----------------------------------------------------------*/

typedef struct __packed {
    uint32_t magic;          /* guard */
    uint8_t  power_on;       /* 1 = lamp on */
    uint8_t  radar_on;       /* 1 = radar enabled */
    uint8_t  dim_index;      /* 0–3  (20/40/70/100 %) */
	uint8_t  factory_lamp_type;
    uint32_t lamp_on_seconds;      /* grand-total lamp-on (UV emitting) time, seconds */
    uint32_t dim_on_seconds[4];    /* per dim level: [0]=20 [1]=40 [2]=70 [3]=100 % */
} PERSISTANCE_REGION_T;


/* Exported variables --------------------------------------------------------*/

extern PERSISTANCE_REGION_T g_persistance_region;


/* Exported functions prototypes ---------------------------------------------*/

void persistance_read_region(void);
void persistance_write_region(void);

void persistance_set_power_state(bool b_pwr_on);
bool persistance_get_power_state(void);
void persistance_set_radar_state(bool b_radar_on);
bool persistance_get_radar_state(void);
void persistance_set_dim_index(uint8_t idx);
uint8_t persistance_get_dim_index(void);
void persistance_set_factory_lamp_type(uint8_t type);
uint8_t persistance_get_factory_lamp_type(void);

void persistance_add_lamp_on_seconds(uint32_t secs, uint8_t dim_index);
uint32_t persistance_get_lamp_on_seconds(void);
uint32_t persistance_get_dim_on_seconds(uint8_t idx);


#endif /* _D_PERSISTANCE_H_ */

/*** END OF FILE ***/
