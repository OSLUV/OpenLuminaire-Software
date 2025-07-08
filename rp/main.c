#include <stdio.h>
#include <string.h>
#include <pico/stdlib.h>

#include "st7789.h"
#include "pins.h"
#include "imu.h"
#include "mag.h"
#include "usbpd.h"
#include "lamp.h"
#include "buttons.h"
#include "sense.h"
#include "radar.h"
#include "menu.h"
#include "fan.h"
#include "radio.h"
#include "safety_logic.h"
#include "persistance.h"
#include "display.h"
#include "ui_main.h"
#include "ui_loading.h"
#include "ui_debug.h"
#include <lvgl.h>

#include "font.c"

void enable_lamp() {
	request_lamp_power(PWR_OFF); 
	update_lamp(); 
	sleep_ms(20);
	usbpd_negotiate(true);
	set_fan(100);
	sleep_ms(250);
	update_sense();
	set_switched_12v(true);
	sleep_ms(1000);
	update_sense();
	printf("Scripted start...\n");

	if (power_ok()) lamp_perform_type_test();

	if (get_lamp_type() == LAMP_TYPE_NONDIMMABLE)
	{
		set_switched_24v(true);
		sleep_ms(100);
	}
	request_lamp_power(PWR_100PCT);
}

void main()
{	
	display_screen_off();
	stdio_init_all();

	gpio_init(4);
	gpio_init(5);
	gpio_init(6);
	gpio_set_dir(4, GPIO_IN);
	gpio_set_dir(5, GPIO_IN);
	gpio_set_dir(6, GPIO_IN);

	init_persistance_region();
	printf("persistance_region.factory_lamp_type = %d\n", persistance_region.factory_lamp_type);
		
	lv_init();
	display_init();

	load_lamp_type_from_flash();
	splash_image_init();
	splash_image_open();

	init_buttons();
	init_imu();
	init_mag();
	init_lamp();
	init_sense();
	init_radar();
	init_fan();
	init_radio();
	
	//enable_lamp();
	
	printf("Enter mainloop... xx\n");
	
	// main UI init
    ui_main_init();
    ui_debug_init();	
    //ui_main_open();  
	
    //housekeeping flags
    const uint64_t TIMEOUT_US = 5ULL * 60 * 1000 * 1000;   // 5 min     
    uint64_t last_activity_us = time_us_64();
    bool screen_dark = false;		
	bool last_power_ok = false; 

	while (1) {
		update_sense();
		update_buttons();
		update_imu();
		update_mag();
		update_radar();
		update_usbpd(); //currently empty?
		update_radio();
		update_lamp();

		//printf("12v: %f 24v: %f vbus: %f power_ok: %d\n", sense_12v, sense_24v, sense_vbus, power_ok());
		
		bool ok = power_ok();
		if ( ok && !last_power_ok ) { 
			enable_lamp();  // turn lamp on
			ui_main_open();       //load ui screen on edge detection
			display_screen_on();   
			screen_dark  = false;      //cancel timeout
		}
		if ( !ok && last_power_ok ) {
			ui_psu_show();         // error screen  
			screen_dark  = false;
		}
		last_power_ok = ok;     // update edge detector 
		
		
		if (ok)
		{
			if (buttons_released) { // triggered on end of button press       
				last_activity_us = time_us_64();

				if (screen_dark) {        // wake-up path         
					display_screen_on();  // back-light on + one flush     
					screen_dark = false;
				}
			}
			
			// ----------- UI & DISPLAY ---------------------------------- 
			if (!screen_dark) {
				lv_timer_handler(); //
				ui_main_update();         // normal widgets                
				ui_debug_update();
			}

			// ----------- TIMEOUT CHECK --------------------------------- 
			if (!screen_dark &&
				(time_us_64() - last_activity_us) > TIMEOUT_US) {
				display_screen_off();     // back-light to 0               
				screen_dark = true;
			}

			// static int cycle= 0;
			// printf("Mainloop... %d\n", cycle++);

			update_safety_logic();
		} else { 
			lv_timer_handler();
			ui_psu_show();
		}
	}
}