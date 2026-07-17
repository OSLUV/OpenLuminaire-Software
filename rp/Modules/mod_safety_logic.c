/**
 * @file      mod_safety_logic.c
 * @author    The OSLUV Project
 * @brief     Safety logic module
 *  
 */


/* Includes ------------------------------------------------------------------*/

#include <stdio.h>
#include <pico/stdlib.h>
#include "Modules/system.h"
#include "Modules/mod_safety_logic.h"
#include "Modules/mod_lamp_ctrl.h"
#include "Drivers/drv_accelerometer.h"
#include "Drivers/drv_debug.h"
#include "Drivers/drv_lamp.h"
#include "Drivers/drv_radar.h"


/* Private typedef -----------------------------------------------------------*/

typedef struct {
	int undiffused_low_tilt;
	int undiffused_high_tilt;
	int diffused_low_tilt;
	int diffused_high_tilt;
} M_SAFETY_BREAK_ROW_T;

/* Private define ------------------------------------------------------------*/

#if 0
#define M_SAFETY_DBG_ID_STR_C        	"mod_safety          "
#define M_SAFETY_DBG_PRINTF(...)    	debug_print_f(__VA_ARGS__)
#define M_SAFETY_DBG_PRINT_TXT(...)		debug_print_mod_f(M_SAFETY_DBG_ID_STR_C, __VA_ARGS__)
#define M_SAFETY_DBG_PRINT_ERR(...)		debug_print_err(M_SAFETY_DBG_ID_STR_C, __VA_ARGS__)
#define M_SAFETY_DBG_PRINT_WRN(...)		debug_print_warn(M_SAFETY_DBG_ID_STR_C, __VA_ARGS__)
#define M_SAFETY_DBG_PRINT_OK(...)		debug_print_ok(M_SAFETY_DBG_ID_STR_C, __VA_ARGS__)
#else
#define M_SAFETY_DBG_PRINTF(...)    	
#define M_SAFETY_DBG_PRINT_TXT(...)		
#define M_SAFETY_DBG_PRINT_ERR(...)		
#define M_SAFETY_DBG_PRINT_WRN(...)		
#define M_SAFETY_DBG_PRINT_OK(...)		
#endif

// entries are in centimeters, what is the furthest distance at which this power level restriction is in effect
// no entry for M_LAMP_PWR_100PCT_C since it's logically infinity

#define M_SAFETY_DEBOUNCE_US_OFF_C   	(1 * 1000 * 1000)    /* 1s */
#define M_SAFETY_DEBOUNCE_US_ON_C    	(3 * 1000 * 1000)    /* 3s */


/* Global variables  ---------------------------------------------------------*/

extern int16_t g_mod_ctrl_pointing_down_angle;


/* Private variables  --------------------------------------------------------*/

#if 1
// ICNIRP limits
static M_SAFETY_BREAK_ROW_T breaks[M_LAMP_PWR_100PCT_C] = {
	[M_LAMP_PWR_OFF_C] =   {110, 110,  54,  54},
	[M_LAMP_PWR_20PCT_C] = {113, 113,  88,  88},
	[M_LAMP_PWR_40PCT_C] = {115, 115, 111, 111},
	[M_LAMP_PWR_70PCT_C] = {116, 116, 112, 112}
};
#elif 0
// With 30% safety margin
static M_SAFETY_BREAK_ROW_T breaks[M_LAMP_PWR_100PCT_C] = {
	[M_LAMP_PWR_OFF_C] =   { 44,  80,  15,  24},
	[M_LAMP_PWR_20PCT_C] = { 64, 104,  21,  37},
	[M_LAMP_PWR_40PCT_C] = { 86, 108,  29,  49},
	[M_LAMP_PWR_70PCT_C] = {102, 110,  35,  57}
};
#elif 0
// Original
static M_SAFETY_BREAK_ROW_T breaks[M_LAMP_PWR_100PCT_C] = {
	[M_LAMP_PWR_OFF_C] =   {36,  66,  12,  21},
	[M_LAMP_PWR_20PCT_C] = {52,  96,  18,  31},
	[M_LAMP_PWR_40PCT_C] = {71, 106,  25,  42},
	[M_LAMP_PWR_70PCT_C] = {86, 108,  30,  51} 
};
#else
// Testing
static M_SAFETY_BREAK_ROW_T breaks[M_LAMP_PWR_100PCT_C] = {
	[M_LAMP_PWR_OFF_C] =   { 30,  30,  12,  21},
	[M_LAMP_PWR_20PCT_C] = { 70,  70,  18,  31},
	[M_LAMP_PWR_40PCT_C] = {100, 100,  25,  42},
	[M_LAMP_PWR_70PCT_C] = {150, 150,  30,  51}
};
#endif

static M_LAMP_PWR_LEVEL_E 	mod_safety_cap_pwlvl = M_LAMP_PWR_100PCT_C;

static char 				mod_safety_action_desc_str[128] = {0};
static M_LAMP_PWR_LEVEL_E 	mod_safety_debounce_new_level_pwlvl = M_LAMP_PWR_OFF_C;
static uint64_t 			mod_safety_debounce_new_time = 0;

static bool 				b_mod_safety_is_radar_enabled = false;


/* Private function prototypes -----------------------------------------------*/

static int mod_safety_get_tilt_break(void);
static int mod_safety_get_distance_for_break_row(M_SAFETY_BREAK_ROW_T* p_row, bool b_is_diffused, bool b_is_high_tilt);
static int mod_safety_get_power_for_distance(int distance, bool b_is_diffused, bool b_is_high_tilt);


/* Exported functions --------------------------------------------------------*/

/**
 * @brief Returns whether the pointing down angled is tilted beyond tilt break 
 * or not
 * 
 * @return true 
 * @return false 
 */
bool mod_safety_is_high_tilt(void)
{
	return g_sys_stt.acc_pointing_down_angle > mod_safety_get_tilt_break();
}

/**
 * @brief 
 * 
 */
void mod_safety_update(void)
{
	if (!b_mod_safety_is_radar_enabled)
	{
		sprintf(mod_safety_action_desc_str, "Disabled");

		M_SAFETY_DBG_PRINT_TXT(mod_safety_action_desc_str);

		return;
	}

	int distance = drv_radar_get_distance_cm();

	if (distance == -1)
	{
		sprintf(mod_safety_action_desc_str, "Radar failed -- 100%%");
		M_SAFETY_DBG_PRINT_TXT(mod_safety_action_desc_str);

		if (g_sys_stt.is_power_ok)
		{
			g_sys_ctl.lamp_req_pwr_level = mod_safety_cap_pwlvl;
		}

		return;
	}

	M_LAMP_PWR_LEVEL_E lamp_pwr = mod_safety_get_power_for_distance(distance, 
													   false, 
													   mod_safety_is_high_tilt());

	/* Ignore requests to strike if the lamp is off but the requested distance 
	 * requires dimming doesn't currently matter since we do a binary on/off for 
	 * the radar
	 */
	if ((g_sys_ctl.lamp_req_pwr_level == M_LAMP_PWR_OFF_C) && 
		(lamp_pwr != M_LAMP_PWR_100PCT_C) && 
		(lamp_pwr != M_LAMP_PWR_OFF_C))
	{
		sprintf(mod_safety_action_desc_str, 
				"Tooclose/%s", 
				mod_lamp_get_power_level_str(lamp_pwr));
		M_SAFETY_DBG_PRINT_TXT(mod_safety_action_desc_str);

		return; 																// Can't strike to anything but 100%
	}

	if (lamp_pwr != mod_safety_debounce_new_level_pwlvl)
	{
		mod_safety_debounce_new_level_pwlvl = lamp_pwr;
		mod_safety_debounce_new_time = time_us_64();
	}

	
	uint64_t debounce_us = (lamp_pwr == M_LAMP_PWR_OFF_C) ? M_SAFETY_DEBOUNCE_US_OFF_C : M_SAFETY_DEBOUNCE_US_ON_C;

	if ((time_us_64() - mod_safety_debounce_new_time) > debounce_us)
	{
		sprintf(mod_safety_action_desc_str, 
				"Req %s", 
				mod_lamp_get_power_level_str(lamp_pwr));
		M_SAFETY_DBG_PRINT_TXT(mod_safety_action_desc_str);

		if (g_sys_stt.is_power_ok)
		{
			g_sys_ctl.lamp_req_pwr_level = (mod_safety_cap_pwlvl < lamp_pwr ? mod_safety_cap_pwlvl : lamp_pwr);
		}
	}
	else
	{
		sprintf(mod_safety_action_desc_str, 
				"Debounce for req %s", 
				mod_lamp_get_power_level_str(lamp_pwr));
		M_SAFETY_DBG_PRINT_TXT(mod_safety_action_desc_str);
	}
}

/**
 * @brief Return pointer to state description string buffer
 * 
 * @return char* 
 */
char* mod_safety_get_state_desc(void)
{
	return mod_safety_action_desc_str;
}

/**
 * @brief Enables/disables radar
 * 
 * @param b_enable true/false state to enable/disable radar
 */
void mod_safety_set_radar_enabled_state(bool b_enable)
{
	b_mod_safety_is_radar_enabled = b_enable;
}

/**
 * @brief Returns the radar enabled state
 * 
 * @return true 
 * @return false 
 */
bool mod_safety_get_radar_enabled_state(void)
{
	return b_mod_safety_is_radar_enabled;
}

#if 0
/**
 * @brief Toggles the radar enabled state
 * 
 */
void mod_safety_toggle_radar_enabled_state(void)
{
	mod_safety_set_radar_enabled_state(!mod_safety_get_radar_enabled_state());
}
#endif

/**
 * @brief Sets the CAP power level
 * 
 * @param pwr_level 
 */
void mod_safety_set_cap_power(M_LAMP_PWR_LEVEL_E pwr_level)
{
	mod_safety_cap_pwlvl = pwr_level;
}


/* Private functions ---------------------------------------------------------*/

/**
 * @brief Get the maximum tilt break in degrees
 * 
 * @return int 
 */
static int mod_safety_get_tilt_break(void)
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
static int mod_safety_get_distance_for_break_row(M_SAFETY_BREAK_ROW_T* p_row, bool b_is_diffused, bool b_is_high_tilt)
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
static int mod_safety_get_power_for_distance(int distance, bool b_is_diffused, bool b_is_high_tilt)
{
#if 0
	for (M_LAMP_PWR_LEVEL_E idx = 0; idx < M_LAMP_PWR_100PCT_C; idx++)
	{
		if (distance <= mod_safety_get_distance_for_break_row(&breaks[idx], 
												   b_is_diffused, 
												   b_is_high_tilt))
		{
			return idx;
		}
	}

	return M_LAMP_PWR_100PCT_C;
#else
	return (distance <= 110) ? M_LAMP_PWR_OFF_C : M_LAMP_PWR_100PCT_C;
#endif
}


/*** END OF FILE ***/
