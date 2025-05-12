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

struct {
	int pwm;
	int power;
} dim_settings[] = {
	{100, 20},
	{83, 40},
	{50, 70},
	{0, 100},
};

#define DIM_100PCT 3
#define DIM_70PCT 2
#define DIM_40PCT 1
#define DIM_20PCT 0

#define NUM_DIM_SETTING (sizeof(dim_settings)/sizeof(dim_settings[0]))

int curr_dim_setting = NUM_DIM_SETTING-1;

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

	dbgf("Lamp Req %s / Report %s\n", get_lamp_command()?"ON ":"off", get_lamp_status()?"ON ":"off");

	if (selectablef("Toggle Power") && (buttons_pressed & BUTTON_CENTER))
	{
		set_lamp(!get_lamp_command(), get_lamp_dim());
	}

	if (selectablef("Adjust Dimming (%3d%% Pwr)", dim_settings[curr_dim_setting].power))
	{
		bool update = false;

		if (buttons_pulsed & BUTTON_LEFT)
		{
			curr_dim_setting--;
			update = true;
		}
		else if (buttons_pulsed & BUTTON_RIGHT)
		{
			curr_dim_setting++;
			update = true;
		}

		if (curr_dim_setting==-1) curr_dim_setting=0;
		if (curr_dim_setting==NUM_DIM_SETTING) curr_dim_setting=NUM_DIM_SETTING-1;

		if (update) set_lamp(get_lamp_command(), dim_settings[curr_dim_setting].pwm);
	}

	int freq = get_lamp_freq();
	char* calc_pwr = "???%";

	if (!get_lamp_command()) calc_pwr = "OFF%";
	else if (freq < 100 && get_lamp_status()) calc_pwr = "100%";
	else if (freq > 900 && freq < 1100) calc_pwr = " 70%";
	else if (freq > 400 && freq < 600) calc_pwr = " 40%";
	else if (freq > 150 && freq < 250) calc_pwr = " 20%";

	dbgf("  -> %3d%% PWM/%.1fV\n", get_lamp_dim(), ((float)get_lamp_dim())/100.*3.3);
	dbgf("  -> Status %dHz / %s\n", freq, calc_pwr);

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

	static bool radar_control_enabled = true;
	static char* radar_op_mode = "OK";
	if (selectablef("Toggle Prox (%s/%s)", radar_control_enabled?" ON":"off", radar_op_mode) && (buttons_pressed & BUTTON_CENTER))
	{
		radar_control_enabled = !radar_control_enabled;
	}

#define SET_LAMP(on, level) if (radar_control_enabled) {curr_dim_setting = level; set_lamp(on, dim_settings[curr_dim_setting].pwm);}

	
	{
		if (radar_distance_cm == -1)
		{
			radar_op_mode = "FAIL-ON";
			SET_LAMP(true, DIM_100PCT);
		}
		else if (radar_distance_cm < 40)
		{
			radar_op_mode = "CLOSE-OFF";
			SET_LAMP(false, DIM_100PCT);
		}
		else if (radar_distance_cm < 80)
		{
			radar_op_mode = "CLOSE-20%";
			SET_LAMP(true, DIM_20PCT);
		}
		else if (radar_distance_cm < 120)
		{
			radar_op_mode = "CLOSE-40%";
			SET_LAMP(true, DIM_40PCT);
		}
		else if (radar_distance_cm < 160)
		{
			radar_op_mode = "CLOSE-70%";
			SET_LAMP(true, DIM_70PCT);
		}
		else
		{
			radar_op_mode = "FAR";
			SET_LAMP(true, DIM_100PCT);
		}
	}

	


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

	dbgf("\n");

	debug_radio();

	draw_box(200, 200 + imu_x*20, 10, 0xf000);
	draw_box(210, 200 + imu_y*20, 10, 0x0ff0);
	draw_box(220, 200 + imu_z*20, 10, 0x000f);
}