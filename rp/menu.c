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

	if (buttons_pressed & BUTTON_UP)
	{
		selected_idx--;
	}
	else if (buttons_pressed & BUTTON_DOWN)
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

	dbgf("VBUS: %.1f/12V: %.1f/24V: %.1f\n", sense_vbus, sense_12v, sense_24v);
	dbgf("USB %s %s/%.1fA\n", usbpd_get_is_trying_for_12v()?"Req 12V":"Req 5V", usbpd_get_is_12v()?"Got 12V":"Got 5V", ((float)usbpd_get_negotiated_mA())/1000.);

	if (usbpd_get_is_trying_for_12v())
	{
		if (selectablef("Down-negotiate USB") && (buttons_pressed & BUTTON_CENTER))
		{
			usbpd_negotiate(false);
		}
	}
	else
	{
		if (selectablef("Up-negotiate USB") && (buttons_pressed & BUTTON_CENTER))
		{
			usbpd_negotiate(true);
		}
	}

	dbgf("\n");

	dbgf("12V Switched %s / 24V Reg %s\n", get_switched_12v()?"ON ":"off", get_switched_24v()?"ON ":"off");

	if (selectablef("Toggle 12V") && (buttons_pressed & BUTTON_CENTER))
	{
		set_switched_12v(!get_switched_12v());
	}

	if (selectablef("Toggle 24V") && (buttons_pressed & BUTTON_CENTER))
	{
		set_switched_24v(!get_switched_24v());
	}

	enum pwr_level rep;
	get_lamp_reported_power(&rep);

	dbgf("Lamp  Req  / Cmd  / Report\n");
	dbgf("Power %04s / %04s / %04s %dHz\n",
		pwr_level_str(get_lamp_requested_power()),
		pwr_level_str(get_lamp_commanded_power()),
		pwr_level_str(rep),
		get_lamp_raw_freq());

	dbgf("State: %s %d\n", lamp_state_str(get_lamp_state()), get_lamp_state_elapsed_ms());

	const char* lamp_type[] = {
		[LAMP_TYPE_DIMMABLE] = "DIMMABLE",
		[LAMP_TYPE_NONDIMMABLE] = "NONDIMMABLE",
		[LAMP_TYPE_UNKNOWN] = "UNKNOWN"
	};

	if (selectablef("Lamp Type: %s (retest)", lamp_type[get_lamp_type()]))
	{
		if (buttons_pressed & BUTTON_CENTER)
		{
			lamp_perform_type_test();
		}
	}

	if (selectablef("Adjust Power"))
	{
		enum pwr_level req_lvl = get_lamp_requested_power();

		if ((buttons_pulsed & BUTTON_LEFT) && (req_lvl > 0))
		{
			printf("Req LEFT\n");
			if (get_lamp_type() == LAMP_TYPE_DIMMABLE)
			{
				req_lvl--;
			}
			else
			{
				req_lvl = PWR_OFF;
			}
		}
		else if ((buttons_pulsed & BUTTON_RIGHT) && (req_lvl < (NUM_REAL_PWR_SETTING-1)))
		{
			printf("Req RIGHT\n");
			if (get_lamp_type() == LAMP_TYPE_DIMMABLE)
			{
				req_lvl++;
			}
			else
			{
				req_lvl = PWR_100PCT;
			}
		}
		else if (buttons_pressed & BUTTON_CENTER)
		{
			printf("Req CENTER\n");
			req_lvl = (req_lvl == PWR_OFF) ? PWR_100PCT : PWR_OFF;
		}

		if (req_lvl != get_lamp_requested_power())
		{
			printf("Adjust power req %d, have %d\n", req_lvl, get_lamp_requested_power());
			request_lamp_power(req_lvl);
		}
	}

	// int freq = get_lamp_raw_freq();
	// dbgf("  -> %3d%% PWM/%.1fV\n", get_lamp_dim(), ((float)get_lamp_dim())/100.*3.3);

	if (selectablef("Adjust Fan (%3d%%)", get_fan()))
	{
		if (buttons_pulsed & BUTTON_LEFT)
		{
			set_fan(get_fan()-5);
		}
		else if (buttons_pulsed & BUTTON_RIGHT)
		{
			set_fan(get_fan()+5);
		}
	}

	if (selectablef("Toggle Prox (%s/tilt %s)", get_safety_logic_enabled()?" ON":"off", get_is_high_tilt()?"hi":"lo") && (buttons_pressed & BUTTON_CENTER))
	{
		set_safety_logic_enabled(!get_safety_logic_enabled());
	}

	dbgf("  ->%s\n", get_safety_logic_state_desc());

	if (selectablef("Adjust Backlight (%d%%)", st7789_get_backlight()))
	{
		if (buttons_pulsed & BUTTON_LEFT)
		{
			printf("adj -5\n");
			st7789_set_backlight(st7789_get_backlight()-5);
		}
		else if (buttons_pulsed & BUTTON_RIGHT)
		{
			printf("adj +5\n");
			st7789_set_backlight(st7789_get_backlight()+5);
		}
	}

	dbgf("\n");

	dbgf("IMU: %+.2f/%+.2f/%+.2f\n", imu_x, imu_y, imu_z);
	dbgf("Mag: %+ 5d/%+ 5d/%+ 5d\n", mag_x, mag_y, mag_z);

	dbgf("\n");

	radar_debug();
	// dbgf("radar_distance_cm: %d", radar_distance_cm);

	dbgf("\n");

	dbgf("IMU angle: %d\n", get_angle_pointing_down());

	// debug_radio();

	draw_box(200, 200 + imu_x*20, 10, 0xf000);
	draw_box(210, 200 + imu_y*20, 10, 0x0ff0);
	draw_box(220, 200 + imu_z*20, 10, 0x000f);
}
