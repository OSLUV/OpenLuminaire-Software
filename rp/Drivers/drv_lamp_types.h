/**
 * @file      drv_lamp_types.h
 * @author    The OSLUV Project
 * @brief     Lamp type definitions for  lamp control driver
 *  
 */

#ifndef _D_LAMP_TYPES_H_
#define _D_LAMP_TYPES_H_


/* Exported includes ---------------------------------------------------------*/

#include <stdint.h>
#include <stdbool.h>


/* Exported typedef ----------------------------------------------------------*/

/**
 * @enum D_LAMP_TYPE_E
 * @brief Available types of lamps
 * 
 * Used in persistance region; bump magic if changed
 * 
 */
typedef enum {
	D_LAMP_TYPE_UNKNOWN_C = 0,
	D_LAMP_TYPE_DIMMABLE_C,
	D_LAMP_TYPE_NON_DIMMABLE_C
} D_LAMP_TYPE_E;

/**
 * @enum D_LAMP_PWR_LEVEL_E
 * 
 * @brief Available power levels for lamp
 * 
 */
typedef enum {
	D_LAMP_PWR_OFF_C = 0,
	D_LAMP_PWR_20PCT_C,
	D_LAMP_PWR_40PCT_C,
	D_LAMP_PWR_70PCT_C,
	D_LAMP_PWR_100PCT_C,
	D_LAMP_PWR_MAX_SETTINGS_C,
	D_LAMP_PWR_UNKNOWN_C
} D_LAMP_PWR_LEVEL_E;

/**
 * @enum D_LAMP_STATE_E
 * 
 * @brief Lamp states list
 * 
 */
typedef enum {
	D_LAMP_STATE_OFF_C = 0,
	D_LAMP_STATE_STARTING_C,
	D_LAMP_STATE_RUNNING_C,
	D_LAMP_STATE_FULLPOWER_TEST_C,
	D_LAMP_STATE_RESTRIKE_COOLDOWN_1_C,
	D_LAMP_STATE_RESTRIKE_ATTEMPT_1_C,
	D_LAMP_STATE_RESTRIKE_COOLDOWN_2_C,
	D_LAMP_STATE_RESTRIKE_ATTEMPT_2_C,
	D_LAMP_STATE_RESTRIKE_COOLDOWN_3_C,
	D_LAMP_STATE_RESTRIKE_ATTEMPT_3_C,
	D_LAMP_STATE_FAILED_OFF_C
} D_LAMP_STATE_E;


#endif /* _D_LAMP_TYPES_H_ */

/*** END OF FILE ***/
