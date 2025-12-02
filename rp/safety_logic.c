/**
 * @file      safety_logic.c
 * @author    The OSLUV Project
 * @brief     Safety logic module
 *  
 */


/* Includes ------------------------------------------------------------------*/

#include <stdio.h>
#include <pico/stdlib.h>
#include "lamp.h"
#include "radar.h"
#include "imu.h"


/* Private typedef -----------------------------------------------------------*/

typedef struct {
	int undiffused_low_tilt;
	int undiffused_high_tilt;
	int diffused_low_tilt;
	int diffused_high_tilt;
} BREAK_ROW_T;

/* Private define ------------------------------------------------------------*/

// entries are in centimeters, what is the furthest distance at which this power level restriction is in effect
// no entry for LAMP_PWR_100PCT_C since it's logically infinity

#define DEBOUNCE_US_OFF   (1 * 1000 * 1000)    /* 1s */
#define DEBOUNCE_US_ON    (3 * 1000 * 1000)    /* 3s */


/* Global variables  ---------------------------------------------------------*/
/* Private variables  --------------------------------------------------------*/

#if 1
// ICNIRP limits
static BREAK_ROW_T breaks[LAMP_PWR_100PCT_C] = {
	[LAMP_PWR_OFF_C] =   {110, 110,  54,  54},
	[LAMP_PWR_20PCT_C] = {113, 113,  88,  88},
	[LAMP_PWR_40PCT_C] = {115, 115, 111, 111},
	[LAMP_PWR_70PCT_C] = {116, 116, 112, 112}
};
#elif 0
// With 30% safety margin
static BREAK_ROW_T breaks[LAMP_PWR_100PCT_C] = {
	[LAMP_PWR_OFF_C] =   { 44,  80,  15,  24},
	[LAMP_PWR_20PCT_C] = { 64, 104,  21,  37},
	[LAMP_PWR_40PCT_C] = { 86, 108,  29,  49},
	[LAMP_PWR_70PCT_C] = {102, 110,  35,  57}
};
#elif 0
// Original
static BREAK_ROW_T breaks[LAMP_PWR_100PCT_C] = {
	[LAMP_PWR_OFF_C] =   {36,  66,  12,  21},
	[LAMP_PWR_20PCT_C] = {52,  96,  18,  31},
	[LAMP_PWR_40PCT_C] = {71, 106,  25,  42},
	[LAMP_PWR_70PCT_C] = {86, 108,  30,  51} 
};
#else
// Testing
static BREAK_ROW_T breaks[LAMP_PWR_100PCT_C] = {
	[LAMP_PWR_OFF_C] =   { 30,  30,  12,  21},
	[LAMP_PWR_20PCT_C] = { 70,  70,  18,  31},
	[LAMP_PWR_40PCT_C] = {100, 100,  25,  42},
	[LAMP_PWR_70PCT_C] = {150, 150,  30,  51}
};
#endif

static LAMP_PWR_LEVEL_E safety_logic_cap = LAMP_PWR_100PCT_C;

static char 			safety_logic_action_desc[128] = {0};
static LAMP_PWR_LEVEL_E safety_logic_debounce_new_level = LAMP_PWR_OFF_C;
static uint64_t 		safety_logic_debounce_new_time = 0;

static bool 			b_safety_logic_is_radar_enabled = false;


/* Private function prototypes -----------------------------------------------*/

static int safety_logic_get_tilt_break(void);
static int safety_logic_get_distance_for_break_row(BREAK_ROW_T* p_row, bool b_is_diffused, bool b_is_high_tilt);
static int safety_logic_get_power_for_distance(int distance, bool b_is_diffused, bool b_is_high_tilt);


/* Exported functions --------------------------------------------------------*/

/**
 * @brief Returns whether the pointing down angled is tilted beyond tilt break 
 * or not
 * 
 * @return true 
 * @return false 
 */
bool safety_logic_is_high_tilt(void)
{
	return imu_get_pointing_down_angle() > safety_logic_get_tilt_break();
}

/**
 * @brief 
 * 
 */
void safety_logic_update(void)
{
	if (!b_safety_logic_is_radar_enabled)
	{
		sprintf(safety_logic_action_desc, "Disabled");
		return;
	}

	int distance = radar_get_distance_cm();

	if (distance == -1)
	{
		sprintf(safety_logic_action_desc, "Radar failed -- 100%%");
		lamp_request_power_level(safety_logic_cap);

		return;
	}

	LAMP_PWR_LEVEL_E lamp_pwr = safety_logic_get_power_for_distance(distance, 
													   false, 
													   safety_logic_is_high_tilt());

	/* Ignore requests to strike if the lamp is off but the requested distance 
	 * requires dimming doesn't currently matter since we do a binary on/off for 
	 * the radar
	 */
	if ((lamp_get_requested_power_level() == LAMP_PWR_OFF_C) && 
		(lamp_pwr != LAMP_PWR_100PCT_C) && 
		(lamp_pwr != LAMP_PWR_OFF_C))
	{
		sprintf(safety_logic_action_desc, 
				"Tooclose/%s", 
					lamp_get_power_level_string(lamp_pwr));

		return; 																// Can't strike to anything but 100%
	}

	if (lamp_pwr != safety_logic_debounce_new_level)
	{
		safety_logic_debounce_new_level = lamp_pwr;
		safety_logic_debounce_new_time = time_us_64();
	}

	
	uint64_t debounce_us = (lamp_pwr == LAMP_PWR_OFF_C) ? DEBOUNCE_US_OFF : DEBOUNCE_US_ON;

	if ((time_us_64() - safety_logic_debounce_new_time) > debounce_us)
	{
		sprintf(safety_logic_action_desc, 
				"Req %s", 
				lamp_get_power_level_string(lamp_pwr));

		lamp_request_power_level(safety_logic_cap < lamp_pwr ? safety_logic_cap : lamp_pwr);
	}
	else
	{
		sprintf(safety_logic_action_desc, 
				"Debounce for req %s", 
				lamp_get_power_level_string(lamp_pwr));
	}
}

/**
 * @brief Return pointer to state description string buffer
 * 
 * @return char* 
 */
char* safety_logic_get_state_desc(void)
{
	return safety_logic_action_desc;
}

/**
 * @brief Enables/disables radar
 * 
 * @param b_enable true/false state to enable/disable radar
 */
void safety_logic_set_radar_enabled_state(bool b_enable)
{
	b_safety_logic_is_radar_enabled = b_enable;
}

/**
 * @brief Returns the radar enabled state
 * 
 * @return true 
 * @return false 
 */
bool safety_logic_get_radar_enabled_state(void)
{
	return b_safety_logic_is_radar_enabled;
}

#if 0
/**
 * @brief Toggles the radar enabled state
 * 
 */
void safety_logic_toggle_radar_enabled_state(void)
{
	safety_logic_set_radar_enabled_state(!safety_logic_get_radar_enabled_state());
}
#endif

/**
 * @brief Sets the CAP power level
 * 
 * @param pwr_level 
 */
void safety_logic_set_cap_power(LAMP_PWR_LEVEL_E pwr_level)
{
	safety_logic_cap = pwr_level;
}


/* Private functions ---------------------------------------------------------*/

/**
 * @brief Get the maximum tilt break in degrees
 * 
 * @return int 
 */
static int safety_logic_get_tilt_break(void)
{
	return 32;
}

/**
 * @brief Gets the distance for a break row
 * 
 * @param p_row Break row to get the distance from
 * @param b_is_diffused 
 * @param b_is_high_tilt 
 * @return int 
 */
static int safety_logic_get_distance_for_break_row(BREAK_ROW_T* p_row, bool b_is_diffused, bool b_is_high_tilt)
{
	if (b_is_diffused && b_is_high_tilt)
	{
		return p_row->diffused_high_tilt;
	}
	else if (b_is_diffused && !b_is_high_tilt)
	{
		return p_row->diffused_low_tilt;
	}
	else if (!b_is_diffused && b_is_high_tilt)
	{
		return p_row->undiffused_high_tilt;
	}
	else
	{
		return p_row->undiffused_low_tilt;
	}
}

/**
 * @brief Get the power level for a distance
 * 
 * @param distance Distance in centimeters
 * @param b_is_diffused 
 * @param b_is_high_tilt 
 * @return int 
 */
static int safety_logic_get_power_for_distance(int distance, bool b_is_diffused, bool b_is_high_tilt)
{
#if 0
	for (LAMP_PWR_LEVEL_E idx = 0; idx < LAMP_PWR_100PCT_C; idx++)
	{
		if (distance <= safety_logic_get_distance_for_break_row(&breaks[idx], 
												   b_is_diffused, 
												   b_is_high_tilt))
		{
			return idx;
		}
	}

	return LAMP_PWR_100PCT_C;
#else
	return (distance <= 110) ? LAMP_PWR_OFF_C : LAMP_PWR_100PCT_C;
#endif
}


/*** END OF FILE ***/
