#include <stdio.h>
#include <pico/stdlib.h>
#include "lamp.h"
#include "radar.h"
#include "imu.h"

// entries are in centimeters, what is the furthest distance at which this power level restriction is in effect
// no entry for PWR_100PCT since it's logically infinity

struct break_row {
	int undiffused_low_tilt;
	int undiffused_high_tilt;
	int diffused_low_tilt;
	int diffused_high_tilt;
} breaks[PWR_100PCT] = {
	// ICNIRP limits
	[PWR_OFF] =   {110,  110,  54, 54},
	[PWR_20PCT] = {113,  113,  88, 88},
	[PWR_40PCT] = {115,  115,  111,  111},
	[PWR_70PCT] = {116,  116, 112,  112}
	// // with 30% safety margin
	// [PWR_OFF] =   {44,  80,  15,  24},
	// [PWR_20PCT] = {64,  104,  21,  37},
	// [PWR_40PCT] = {86,  108,  29,  49},
	// [PWR_70PCT] = {102,  110, 35,  57}
	//// original
	// [PWR_OFF] =   {36,  66,  12,  21},
	// [PWR_20PCT] = {52,  96,  18,  31},
	// [PWR_40PCT] = {71,  106, 25,  42},
	// [PWR_70PCT] = {86,  108, 30,  51} 
	//// testing
	// [PWR_OFF] =   {30,  30,  12,  21},
	// [PWR_20PCT] = {70,  70,  18,  31},
	// [PWR_40PCT] = {100,  100, 25,  42},
	// [PWR_70PCT] = {150,  150, 30,  51}
};

enum pwr_level cap = PWR_100PCT;

int get_tilt_break()
{
	return 32; // degrees
}

bool get_is_high_tilt()
{
	return get_angle_pointing_down() > get_tilt_break();
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
	// for (enum pwr_level i = 0; i < PWR_100PCT; i++)
	// {
		// if (distance_cm <= get_distance_for_break_row(&breaks[i], diffused, high_tilt))
		// {
			// return i;
		// }
	// }

	// return PWR_100PCT;
	
	return (distance_cm <= 110) ? PWR_OFF : PWR_100PCT;
}

char safety_action_desc[128] = {0};
enum pwr_level debounce_new_level = PWR_OFF;
uint64_t debounce_new_time = 0;

bool radar_safety_enabled = false;

void update_safety_logic()
{
	if (!radar_safety_enabled)
	{
		sprintf(safety_action_desc, "Disabled");
		return;
	}

	int distance = get_radar_distance_cm();

	if (distance == -1)
	{
		sprintf(safety_action_desc, "Radar failed -- 100%%");
		request_lamp_power(cap);
		return;
	}

	enum pwr_level pwr = get_power_for_distance(distance, false, get_is_high_tilt());

	// ignore requests to strike if the lamp is off but the requested distance requires dimming
	// doesn't currently matter since we do a binary on/off for the radar
	if (get_lamp_requested_power() == PWR_OFF && pwr != PWR_100PCT && pwr != PWR_OFF)
	{
		sprintf(safety_action_desc, "Tooclose/%s", pwr_level_str(pwr));
		return; // Can't strike to anything but 100%
	}

	if (pwr != debounce_new_level)
	{
		debounce_new_level = pwr;
		debounce_new_time = time_us_64();
	}

	if ((time_us_64() - debounce_new_time) > (1000*1000*3))
	{
		sprintf(safety_action_desc, "Req %s", pwr_level_str(pwr));
		request_lamp_power(cap < pwr ? cap : pwr);
	}
	else
	{
		sprintf(safety_action_desc, "Debounce for req %s", pwr_level_str(pwr));
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

void set_safety_logic_cap(enum pwr_level l)
{
	cap = l;
}