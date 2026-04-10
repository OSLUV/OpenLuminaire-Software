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
#include "radio.h"
#include "safety_logic.h"
#include "display.h"
#include "ui_main.h"
#include "ui_loading.h"
#include "ui_debug.h"

#include "buttons.h"
#include "lamp.h"

#include "Modules/mod_comm_mgr.h"
#include "Modules/mod_ctrl_mgr.h"
#include "Modules/mod_pow_mgr.h"
#include "Modules/mod_system.h"

#include "font.c"

/* Private function prototypes -----------------------------------------------*/

static void main_sys_init(void);


/* Application main function -------------------------------------------------*/

void main(void)
{
	main_sys_init();
	
    // Housekeeping flags
    const uint64_t TIMEOUT_US = 5ULL * 60 * 1000 * 1000;   						// 5 min
    uint64_t last_activity_us = time_us_64();
	bool b_is_screen_dark = false;
	
	while (1)
	{
		mod_sys_services();

		mod_pow_manager();
		
		mod_comm_manager();
		
		mod_ctrl_manager();
		
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
			lv_timer_handler();

			static uint64_t last_retry = 0;
			if (last_retry == 0) last_retry = time_us_64();

			if ((time_us_64() - last_retry) > 3000000)  // every 3s
			{
				ui_loading_show_psu_status("Retrying...");
				lamp_power_up_rails();
				last_retry = time_us_64();

				if (lamp_is_power_ok())
				{
					lamp_perform_type_test();
					lamp_request_power_level(LAMP_PWR_100PCT_C);
					ui_main_open();
				}
			}
		}
	}
}

/**
 * @brief Full system initialization procedure
 * 
 */
static void main_sys_init(void)
{
	display_screen_off();
	stdio_init_all();

	gpio_init(4); /* RADIO_RX */
	gpio_init(5); /* RADIO_TX */
	gpio_init(6); /* RADIO_ENABLE */
	gpio_set_dir(4, GPIO_IN);
	gpio_set_dir(5, GPIO_IN);
	gpio_set_dir(6, GPIO_IN);

	mod_sys_init();

	lv_init();
	display_init();

	lamp_load_type_from_flash();
	ui_loading_splash_image_init();
	ui_loading_splash_image_open(NULL);

	mod_pow_init();
	mod_sys_startup_wdt();
	mod_comm_init();
	mod_ctrl_init();
	
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
}


/*** END OF FILE ***/
