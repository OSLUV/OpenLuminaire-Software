/**
 * @file      main.c
 * @author    The OSLUV Project
 * @brief     Main application loop
 * @schematic lamp_controller.SchDoc
 * @schematic power.SchDoc
 * 
 * @mainpage Software for OSLUV OpenLuminaire
 * OpenLuminaire is  OSHWA certified Open Source 222nm luminiare for the 
 * RP2040-based hardware.
 *  
 */


/* Includes ------------------------------------------------------------------*/

#include <stdio.h>
#include <string.h>
#include <pico/stdlib.h>
#include <lvgl.h>

#include "st7789.h"
#include "pins.h"
#include "imu.h"
#include "mag.h"
#include "usbpd.h"
#include "lamp.h"
#include "buttons.h"
#include "sense.h"
#include "radar.h"
#include "fan.h"
#include "radio.h"
#include "safety_logic.h"
#include "persistance.h"
#include "display.h"
#include "ui_main.h"
#include "ui_loading.h"
#include "ui_debug.h"

#include "m_cmd.h"

#include "font.c"


/* Application main function -------------------------------------------------*/

void main(void)
{	
	display_screen_off();
	stdio_init_all();

	gpio_init(4);
	gpio_init(5);
	gpio_init(6);
	gpio_set_dir(4, GPIO_IN);
	gpio_set_dir(5, GPIO_IN);
	gpio_set_dir(6, GPIO_IN);

	persistance_read_region();
	printf("g_persistance_region.factory_lamp_type = %d\n", 
		   g_persistance_region.factory_lamp_type);
		
	lv_init();
	display_init();

	lamp_load_type_from_flash();
	ui_loading_splash_image_init();
	ui_loading_splash_image_open(NULL);

	buttons_init();
	imu_init();
	mag_init();
	lamp_init();
	sense_init();
	radar_init();
	fan_init();
	//radio_init();
	usbpd_negotiate(true);  // V1.2: Now negotiates 20V/1.5A (was 12V/2.5A) — see usbpd.c
	fan_set_speed(100);
	m_cmd_init();

	sleep_ms(250);
	sense_update();
	printf("[V1.2] Pre-enable: VBUS=%.2f 12V=%.2f 24V=%.2f\n",
		   g_sense_vbus, g_sense_12v, g_sense_24v);

	// V1.2: Reverse power-on order — 24V first (boost from VSYS), then 12V (buck from 24V)
	// V1.1: lamp_set_switched_12v(true);
	lamp_set_switched_24v(true);
	sleep_ms(500);
	sense_update();
	printf("[V1.2] After 24V: VBUS=%.2f 12V=%.2f 24V=%.2f\n",
		   g_sense_vbus, g_sense_12v, g_sense_24v);

	lamp_set_switched_12v(true);

	sleep_ms(1000);

	sense_update();
	printf("[V1.2] After 12V: VBUS=%.2f 12V=%.2f 24V=%.2f\n",
		   g_sense_vbus, g_sense_12v, g_sense_24v);

	printf("Scripted start...\n");

	// TEMPORARY: Hardcode dimmable for V1.2 testing — skip type test
	// TODO: Replace with dimming-response type test
	printf("[V1.2] TEMPORARY: Hardcoding lamp type as DIMMABLE for testing\n");
	// if (lamp_is_power_ok())
	// {
	// 	lamp_perform_type_test();
	// }

	// V1.2: Ensure both rails are on after type test.
	// Phase A (non-dimmable test) leaves 12V off if it succeeds — re-enable it
	// so fan, radar (+5V), and voltage monitoring all work.
	// V1.1: if (lamp_get_type() == LAMP_TYPE_NON_DIMMABLE_C)
	// V1.1: {
	// V1.1: 	lamp_set_switched_24v(true);
	// V1.1: 	sleep_ms(100);
	// V1.1: }
	if (!lamp_get_switched_24v())
	{
		lamp_set_switched_24v(true);
		sleep_ms(500);
	}
	if (!lamp_get_switched_12v())
	{
		lamp_set_switched_12v(true);
		sleep_ms(1000);
	}
	sense_update();
	printf("[V1.2] Post-test: VBUS=%.2f 12V=%.2f 24V=%.2f\n",
		   g_sense_vbus, g_sense_12v, g_sense_24v);

	lamp_request_power_level(LAMP_PWR_100PCT_C);
	
	printf("Enter mainloop... xx\n");
	
	// Main UI init
    ui_main_init();
    ui_debug_init();
	
    //ui_main_open();  
	
	if (lamp_is_power_ok()) 
	{
		ui_main_open();
	} 
	else 
	{
		ui_loading_show_psu();
	}	
	
    // Housekeeping flags
    const uint64_t TIMEOUT_US = 5ULL * 60 * 1000 * 1000;   						// 5 min
    uint64_t last_activity_us = time_us_64();
	bool b_is_screen_dark = false;
	
	while (1)
	{
		m_cmd_handler();
		sense_update();
		buttons_update();
		imu_update();
		mag_update();
		radar_update();
		usbpd_update();
		lamp_update();
		
		if (lamp_is_power_ok())
		{
			if (g_buttons_released) 
			{
				last_activity_us = time_us_64();

				if (b_is_screen_dark)
				{
					// Wake-up path         
					display_screen_on();  										// Back-light on + one flush
					b_is_screen_dark = false;
				}
			}
			
			// ----------- UI & DISPLAY ---------------------------------- 
			if (!b_is_screen_dark ||
				(display_get_backlight_brightness() > 0)) 						// Is screen on ?
			{
				if (b_is_screen_dark)
				{
					b_is_screen_dark = false;

					last_activity_us = time_us_64();
				}

				lv_timer_handler();
				ui_main_update();         										// Normal widgets                
				ui_debug_update();
			}

			// ----------- TIMEOUT CHECK --------------------------------- 
			if (!b_is_screen_dark && 
				((time_us_64() - last_activity_us) > TIMEOUT_US))
			{
				display_screen_off();     										// Set back-light to 0
				b_is_screen_dark = true;
			}

			// static int cycle= 0;
			// printf("Mainloop... %d\n", cycle++);

			safety_logic_update();
		}
		else 
		{
			ui_loading_show_psu();
		}
	}
}

/*** END OF FILE ***/
