#include <pico/stdlib.h>
#include <stdio.h>
#include "main.h"
#include "buttons.h"
#include "sense.h"
#include "usbpd.h"
#include "lamp.h"
#include "imu.h"
#include "mag.h"
#include "radar.h"
#include "fan.h"
#include "st7789.h"
#include "radio.h"
#include "safety_logic.h"

void root_menu();

typedef void (*menu_t)();

int selected_idx = 0;
menu_t selected_menu = &root_menu;

int curr_idx = 0;

bool selectablef(const char* fmt, ...)
{
	static char work_buf[128];

	bool selected = selected_idx == curr_idx;
	curr_idx++;

	va_list args;
	va_start(args, fmt);

	int n = vsnprintf(work_buf, sizeof(work_buf), fmt, args);

	va_end(args);

	if (selected) draw_text("< ", 2);
	else draw_text("  ", 2);

	draw_text(work_buf, n);
	if (selected) draw_text(" >", 2);

	draw_text("\n", 1);

	return selected;
}

void do_menu()
{
	curr_idx = 0;

	menu_t prev_menu = selected_menu;

	selected_menu();

	if (g_buttons_pressed & BUTTON_UP_C)
	{
		selected_idx--;
	}
	else if (g_buttons_pressed & BUTTON_DOWN_C)
	{
		selected_idx++;
	}

	if (selected_idx < 0)
	{
		selected_idx = curr_idx-1;
	}
	else if (selected_idx >= curr_idx)
	{
		selected_idx = 0;
	}

	if (selected_menu != prev_menu) selected_idx = 0;
}

char* radar_op_mode = "OK";

void root_menu()
{
	dbgf("OSLUV Lamp Controller 1.0\n");

	dbgf("\n");

	dbgf("VBUS: %.1f/12V: %.1f/24V: %.1f\n", g_sense_vbus, g_sense_12v, g_sense_24v);
	dbgf("USB %s %s/%.1fA\n", usbpd_get_is_trying_for_12v()?"Req 12V":"Req 5V", usbpd_get_is_12v()?"Got 12V":"Got 5V", ((float)usbpd_get_negotiated_mA())/1000.);

	if (usbpd_get_is_trying_for_12v())
	{
		if (selectablef("Down-negotiate USB") && (g_buttons_pressed & BUTTON_CENTER_C))
		{
			usbpd_negotiate(false);
		}
	}
	else
	{
		if (selectablef("Up-negotiate USB") && (g_buttons_pressed & BUTTON_CENTER_C))
		{
			usbpd_negotiate(true);
		}
	}

	dbgf("\n");

	dbgf("12V Switched %s / 24V Reg %s\n", lamp_get_switched_12v()?"ON ":"off", lamp_get_switched_24v()?"ON ":"off");

	if (selectablef("Toggle 12V") && (g_buttons_pressed & BUTTON_CENTER_C))
	{
		lamp_set_switched_12v(!lamp_get_switched_12v());
	}

	if (selectablef("Toggle 24V") && (g_buttons_pressed & BUTTON_CENTER_C))
	{
		lamp_set_switched_24v(!lamp_get_switched_24v());
	}

	LAMP_PWR_LEVEL_E rep;
	lamp_get_reported_power_level(&rep);

	dbgf("Lamp  Req  / Cmd  / Report\n");
	dbgf("Power %04s / %04s / %04s %dHz\n",
		lamp_get_power_level_string(lamp_get_requested_power_level()),
		lamp_get_power_level_string(lamp_get_commanded_power_level()),
		lamp_get_power_level_string(rep),
		lamp_get_raw_freq());

	dbgf("State: %s %d\n", lamp_get_lamp_state_str(lamp_get_lamp_state()), lamp_get_state_elapsed_ms());

	const char* lamp_type[] = {
		[LAMP_TYPE_DIMMABLE_C] = "DIMMABLE",
		[LAMP_TYPE_NON_DIMMABLE_C] = "NONDIMMABLE",
		[LAMP_TYPE_UNKNOWN_C] = "UNKNOWN"
	};

	if (selectablef("Lamp Type: %s (retest)", lamp_type[lamp_get_type()]))
	{
		if (g_buttons_pressed & BUTTON_CENTER_C)
		{
			lamp_perform_type_test();
		}
	}

	if (selectablef("Adjust Power"))
	{
		LAMP_PWR_LEVEL_E req_lvl = lamp_get_requested_power_level();

		if ((g_buttons_pulsed & BUTTON_LEFT_C) && (req_lvl > 0))
		{
			printf("Req LEFT\n");
			if (lamp_get_type() == LAMP_TYPE_DIMMABLE_C)
			{
				req_lvl--;
			}
			else
			{
				req_lvl = LAMP_PWR_OFF_C;
			}
		}
		else if ((g_buttons_pulsed & BUTTON_RIGHT_C) && (req_lvl < (LAMP_PWR_MAX_SETTINGS_C-1)))
		{
			printf("Req RIGHT\n");
			if (lamp_get_type() == LAMP_TYPE_DIMMABLE_C)
			{
				req_lvl++;
			}
			else
			{
				req_lvl = LAMP_PWR_100PCT_C;
			}
		}
		else if (g_buttons_pressed & BUTTON_CENTER_C)
		{
			printf("Req CENTER\n");
			req_lvl = (req_lvl == LAMP_PWR_OFF_C) ? LAMP_PWR_100PCT_C : LAMP_PWR_OFF_C;
		}

		if (req_lvl != lamp_get_requested_power_level())
		{
			printf("Adjust power req %d, have %d\n", req_lvl, lamp_get_requested_power_level());
			lamp_request_power_level(req_lvl);
		}
	}

	// int freq = lamp_get_raw_freq();
	// dbgf("  -> %3d%% PWM/%.1fV\n", get_lamp_dim(), ((float)get_lamp_dim())/100.*3.3);

	if (selectablef("Adjust Fan (%3d%%)", fan_get_speed()))
	{
		if (g_buttons_pulsed & BUTTON_LEFT_C)
		{
			fan_set_speed(fan_get_speed()-5);
		}
		else if (g_buttons_pulsed & BUTTON_RIGHT_C)
		{
			fan_set_speed(fan_get_speed()+5);
		}
	}

	if (selectablef("Toggle Prox (%s/tilt %s)", get_safety_logic_enabled()?" ON":"off", get_is_high_tilt()?"hi":"lo") && (g_buttons_pressed & BUTTON_CENTER_C))
	{
		set_safety_logic_enabled(!get_safety_logic_enabled());
	}

	dbgf("  ->%s\n", get_safety_logic_state_desc());

	if (selectablef("Adjust Backlight (%d%%)", st7789_get_backlight()))
	{
		if (g_buttons_pulsed & BUTTON_LEFT_C)
		{
			printf("adj -5\n");
			st7789_set_backlight(st7789_get_backlight()-5);
		}
		else if (g_buttons_pulsed & BUTTON_RIGHT_C)
		{
			printf("adj +5\n");
			st7789_set_backlight(st7789_get_backlight()+5);
		}
	}

	dbgf("\n");

	dbgf("IMU: %+.2f/%+.2f/%+.2f\n", g_imu_x, g_imu_y, g_imu_z);
	dbgf("Mag: %+ 5d/%+ 5d/%+ 5d\n", g_mag_x, g_mag_y, g_mag_z);

	dbgf("\n");

	radar_debug();
	// dbgf("radar_distance_cm: %d", radar_distance_cm);

	dbgf("\n");

	dbgf("IMU angle: %d\n", imu_get_pointing_down_angle());

	// radio_debug();

	draw_box(200, 200 + g_imu_x*20, 10, 0xf000);
	draw_box(210, 200 + g_imu_y*20, 10, 0x0ff0);
	draw_box(220, 200 + g_imu_z*20, 10, 0x000f);
}
