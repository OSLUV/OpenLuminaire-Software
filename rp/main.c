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
	
	// st7789_init(&lcd_config, LCD_WIDTH, LCD_HEIGHT);
	// st7789_set_backlight(50);
	// display_splash_screen();

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
	usbpd_negotiate(true);
	set_fan(100);

	sleep_ms(250);
	update_sense();

	set_switched_12v(true);

	sleep_ms(1000);

	update_sense();

	printf("Scripted start...\n");

	lamp_perform_type_test();


	if (get_lamp_type() == LAMP_TYPE_NONDIMMABLE)
	{
		set_switched_24v(true);
		sleep_ms(100);
	}

	request_lamp_power(PWR_100PCT);
	
	printf("Enter mainloop... xx\n");
	
	// main UI init
    ui_main_init();
    ui_debug_init();
    ui_main_open();  

    // housekeeping flags
    const uint64_t TIMEOUT_US = 5ULL * 60 * 1000 * 1000;   // 5 min     
    uint64_t last_activity_us = time_us_64();
    bool screen_dark = false;
	bool psu_ok = usbpd_get_is_12v() && (usbpd_get_negotiated_mA() >= 2500);
	/*if (psu_ok) {
		ui_main_open();
	} else {
		ui_psu_show();
	}*/
	
	
	while (1) {
		update_sense();
		update_buttons();
		update_imu();
		update_mag();
		update_radar();
		update_usbpd(); //currently empty?
		update_radio();
		update_lamp();
		
		dump_buttons();
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
	}
}
