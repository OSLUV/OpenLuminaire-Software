/**
 * @file      lamp.h
 * @author    The OSLUV Project
 * @brief     Functions prototypes for lamp state control driver
 *  
 */

#ifndef _D_LAMP_H_
#define _D_LAMP_H_


/* Exported typedef --------------------------------------------------------*/

/**
 * @enum LAMP_TYPE_E
 * @brief Available types of lamps
 * 
 * Used in persistance region; bump magic if changed
 * 
 */
typedef enum {
	LAMP_TYPE_UNKNOWN_C = 0,
	LAMP_TYPE_DIMMABLE_C,
	LAMP_TYPE_NON_DIMMABLE_C
} LAMP_TYPE_E;

/**
 * @eunm LAMP_PWR_LEVEL_E
 * 
 * @brief Available power levels for lamp
 * 
 */
typedef enum {
	LAMP_PWR_OFF_C = 0,
	LAMP_PWR_20PCT_C,
	LAMP_PWR_40PCT_C,
	LAMP_PWR_70PCT_C,
	LAMP_PWR_100PCT_C,
	LAMP_PWR_MAX_SETTINGS_C,
	LAMP_PWR_UNKNOWN_C
} LAMP_PWR_LEVEL_E;

/**
 * @eunm LAMP_STATE_E
 * 
 * @brief Lamp states list
 * 
 */
typedef enum {
	LAMP_STATE_OFF_C = 0,
	LAMP_STATE_STARTING_C,
	LAMP_STATE_RUNNING_C,
	LAMP_STATE_FULLPOWER_TEST_C,
	LAMP_STATE_RESTRIKE_COOLDOWN_1_C,
	LAMP_STATE_RESTRIKE_ATTEMPT_1_C,
	LAMP_STATE_RESTRIKE_COOLDOWN_2_C,
	LAMP_STATE_RESTRIKE_ATTEMPT_2_C,
	LAMP_STATE_RESTRIKE_COOLDOWN_3_C,
	LAMP_STATE_RESTRIKE_ATTEMPT_3_C,
	LAMP_STATE_FAILED_OFF_C
} LAMP_STATE_E;


/* Exported functions prototypes ---------------------------------------------*/

void lamp_init(void);
void lamp_update(void);

void lamp_load_type_from_flash(void);
LAMP_TYPE_E lamp_get_type(void);
void lamp_perform_type_test(void);

void lamp_set_switched_12v(bool on);
void lamp_set_switched_24v(bool on);
bool lamp_get_switched_12v(void);
bool lamp_get_switched_24v(void);

bool lamp_request_power_level(LAMP_PWR_LEVEL_E);
LAMP_PWR_LEVEL_E lamp_get_requested_power_level(void);
LAMP_PWR_LEVEL_E lamp_get_commanded_power_level(void);
bool lamp_get_reported_power_level(LAMP_PWR_LEVEL_E *p_pwr_level);
bool lamp_is_power_ok(void);
const char* lamp_get_power_level_string(LAMP_PWR_LEVEL_E l);

int lamp_get_raw_freq(void);
LAMP_STATE_E lamp_get_lamp_state(void);
const char* lamp_get_lamp_state_str(LAMP_STATE_E s);
int lamp_get_state_elapsed_ms(void);
bool lamp_is_warming(void);


#endif /* _D_LAMP_H_ */

/*** END OF FILE ***/
