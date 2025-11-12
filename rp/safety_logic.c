#include <stdio.h>
#include <pico/stdlib.h>
#include "lamp.h"
#include "radar.h"
#include "imu.h"

// entries are in centimeters, what is the furthest distance at which this power level restriction is in effect
// no entry for LAMP_PWR_100PCT_C since it's logically infinity

#define DEBOUNCE_US_OFF   (1  * 1000 * 1000)    /* 1 s  */
#define DEBOUNCE_US_ON    (3  * 1000 * 1000)    /* 3 s  */

struct break_row {
	int undiffused_low_tilt;
	int undiffused_high_tilt;
	int diffused_low_tilt;
	int diffused_high_tilt;
} breaks[LAMP_PWR_100PCT_C] = {
	// ICNIRP limits
	[LAMP_PWR_OFF_C] =   {110,  110,  54, 54},
	[LAMP_PWR_20PCT_C] = {113,  113,  88, 88},
	[LAMP_PWR_40PCT_C] = {115,  115,  111,  111},
	[LAMP_PWR_70PCT_C] = {116,  116, 112,  112}
	// // with 30% safety margin
	// [LAMP_PWR_OFF_C] =   {44,  80,  15,  24},
	// [LAMP_PWR_20PCT_C] = {64,  104,  21,  37},
	// [LAMP_PWR_40PCT_C] = {86,  108,  29,  49},
	// [LAMP_PWR_70PCT_C] = {102,  110, 35,  57}
	//// original
	// [LAMP_PWR_OFF_C] =   {36,  66,  12,  21},
	// [LAMP_PWR_20PCT_C] = {52,  96,  18,  31},
	// [LAMP_PWR_40PCT_C] = {71,  106, 25,  42},
	// [LAMP_PWR_70PCT_C] = {86,  108, 30,  51} 
	//// testing
	// [LAMP_PWR_OFF_C] =   {30,  30,  12,  21},
	// [LAMP_PWR_20PCT_C] = {70,  70,  18,  31},
	// [LAMP_PWR_40PCT_C] = {100,  100, 25,  42},
	// [LAMP_PWR_70PCT_C] = {150,  150, 30,  51}
};

LAMP_PWR_LEVEL_E cap = LAMP_PWR_100PCT_C;

int get_tilt_break()
{
	return 32; // degrees
}

bool get_is_high_tilt()
{
	return imu_get_pointing_down_angle() > get_tilt_break();
}

int get_distance_for_break_row(struct break_row* row, bool diffused, bool high_tilt)
{
	if (diffused && high_tilt)
	{
		return row->diffused_high_tilt;
	}
	else if (diffused && !high_tilt)
	{
		return row->diffused_low_tilt;
	}
	else if (!diffused && high_tilt)
	{
		return row->undiffused_high_tilt;
	}
	else
	{
		return row->undiffused_low_tilt;
	}
}

int get_power_for_distance(int distance_cm, bool diffused, bool high_tilt)
{
	// for (LAMP_PWR_LEVEL_E i = 0; i < LAMP_PWR_100PCT_C; i++)
	// {
		// if (distance_cm <= get_distance_for_break_row(&breaks[i], diffused, high_tilt))
		// {
			// return i;
		// }
	// }

	// return LAMP_PWR_100PCT_C;
	
	return (distance_cm <= 110) ? LAMP_PWR_OFF_C : LAMP_PWR_100PCT_C;
}

char safety_action_desc[128] = {0};
LAMP_PWR_LEVEL_E debounce_new_level = LAMP_PWR_OFF_C;
uint64_t debounce_new_time = 0;

bool radar_safety_enabled = false;

void update_safety_logic()
{
	if (!radar_safety_enabled)
	{
		sprintf(safety_action_desc, "Disabled");
		return;
	}

	int distance = radar_get_distance_cm();

	if (distance == -1)
	{
		sprintf(safety_action_desc, "Radar failed -- 100%%");
		lamp_request_power_level(cap);
		return;
	}

	LAMP_PWR_LEVEL_E pwr = get_power_for_distance(distance, false, get_is_high_tilt());

	// ignore requests to strike if the lamp is off but the requested distance requires dimming
	// doesn't currently matter since we do a binary on/off for the radar
	if (lamp_get_requested_power_level() == LAMP_PWR_OFF_C && pwr != LAMP_PWR_100PCT_C && pwr != LAMP_PWR_OFF_C)
	{
		sprintf(safety_action_desc, "Tooclose/%s", lamp_get_power_level_string(pwr));
		return; // Can't strike to anything but 100%
	}

	if (pwr != debounce_new_level)
	{
		debounce_new_level = pwr;
		debounce_new_time = time_us_64();
	}

	
	uint64_t debounce_us = (pwr == LAMP_PWR_OFF_C) ? DEBOUNCE_US_OFF : DEBOUNCE_US_ON;
	if ((time_us_64() - debounce_new_time) > debounce_us)
	{
		sprintf(safety_action_desc, "Req %s", lamp_get_power_level_string(pwr));
		lamp_request_power_level(cap < pwr ? cap : pwr);
	}
	else
	{
		sprintf(safety_action_desc, "Debounce for req %s", lamp_get_power_level_string(pwr));
	}
}

char* get_safety_logic_state_desc()
{
	return safety_action_desc;
}

void set_safety_logic_enabled(bool e)
{
	radar_safety_enabled = e;
}

bool get_safety_logic_enabled()
{
	return radar_safety_enabled;
}

void toggle_radar()
{
	set_safety_logic_enabled(!get_safety_logic_enabled());
}

void set_safety_logic_cap(LAMP_PWR_LEVEL_E l)
{
	cap = l;
}