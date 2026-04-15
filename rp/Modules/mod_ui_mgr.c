/**
 * @file      mod_ui_mgr.c
 * @author    The OSLUV Project
 * @brief     UI Manager module. This module handles all system screens to 
 * 			  display on LCD.
 */


/* Includes ------------------------------------------------------------------*/

#include <stdio.h>
#include <lvgl.h>
#include "pico/time.h"

#include "Modules/mod_ui_mgr.h"
#include "Drivers/drv_buttons.h"
#include "Drivers/drv_display.h"
#include "Drivers/drv_lamp.h"
#include "ui_main.h"
#include "ui_loading.h"
#include "ui_debug.h"


/* Private define ------------------------------------------------------------*/

#define M_UI_SPLASH_SCRN_TM_MS_C	1000										/* Time (in ms) Splash Screen will remain on screen */
#define M_UI_STANDBY_TM_MS_C		(1 * 30 * 1000)								/* Maximum time (in ms) for the UI to remain unused */


/* Private typedef -----------------------------------------------------------*/

typedef enum {
	M_UI_SCRN_NONE_C = 0,
	M_UI_SCRN_SPLASH_C,
	M_UI_SCRN_LOADING_C,
	M_UI_SCRN_MAIN_C,
	M_UI_SCRN_DEBUG_C,
	M_UI_LAST_SCRN_C
} M_UI_SCRN_E;


/* Global variables  ---------------------------------------------------------*/
/* Private variables  --------------------------------------------------------*/

static bool 			b_mod_ui_is_booting;
static bool 			b_mod_ui_is_backlight_on;
static M_UI_SCRN_E		mod_ui_screen;
static absolute_time_t  mod_ui_splash_tmout;
static absolute_time_t  mod_ui_stanby_tmout;


/* Callback prototypes -------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/

static inline void mod_ui_backlight_turn_on(void);
static inline void mod_ui_backlight_turn_off(void);
static void mod_ui_backlight_handler(void);
static void mod_ui_splash_screen_handler(void);
static void mod_ui_main_screen_handler(void);
static void mod_ui_psu_screen_handler(void);


/* Exported functions --------------------------------------------------------*/

/**
 * @brief Control Manager module initialization procedure
 * 
 */
void mod_ui_init(void)
{
	b_mod_ui_is_booting      = true;
	b_mod_ui_is_backlight_on = false;

    buttons_init();

	display_screen_off();

	lv_init();
	display_init();

	drv_lamp_load_type_from_flash();
	ui_loading_splash_image_init();

	/* TODO: This next block can be ggone after fixing usb-pd negotiation to 
	   non-blocking
	*/
	ui_loading_splash_image_open(NULL);
	b_mod_ui_is_backlight_on = true;
	mod_ui_stanby_tmout = make_timeout_time_ms (M_UI_STANDBY_TM_MS_C);
	/**/

	ui_main_init();
    ui_debug_init();
}

/**
 * @brief Control Manager module tasks
 * 
 */
void mod_ui_manager(void)
{
	buttons_update();

	mod_ui_backlight_handler();

	lv_timer_handler();

	if (b_mod_ui_is_booting)
	{
		mod_ui_splash_screen_handler();
	}
	else 
	{
		if (drv_lamp_is_power_ok()) 
		{
			mod_ui_main_screen_handler();
		} 
		else 
		{
			mod_ui_psu_screen_handler();
		}
	}
}

/* Callback functions --------------------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/**
 * @brief Handles display's backlight turn on
 * 
 */
static inline void mod_ui_backlight_turn_on(void)
{
	display_screen_on();  												// Back-light on + one flush
	
	b_mod_ui_is_backlight_on = true;
}

/**
 * @brief Handles display's backlight turn off
 * 
 */
static inline void mod_ui_backlight_turn_off(void)
{
	display_screen_off();
	
	b_mod_ui_is_backlight_on = false;
}

/**
 * @brief Handles display backlight control
 * 
 */
static void mod_ui_backlight_handler(void)
{
	if (b_mod_ui_is_backlight_on)
	{
		if (get_absolute_time() > mod_ui_stanby_tmout)
		{
			mod_ui_backlight_turn_off();
		}
	}

	if (g_buttons_released)
	{
		if (!b_mod_ui_is_backlight_on)
		{
			mod_ui_backlight_turn_on();
		}

		mod_ui_stanby_tmout =  make_timeout_time_ms (M_UI_STANDBY_TM_MS_C);
	}
}

/**
 * @brief Handles splash screen at system bootup
 * 
 */
static void mod_ui_splash_screen_handler(void)
{
	static uint8_t splash_scrn_stt  = 0;
	
	switch(splash_scrn_stt)
	{
		case 0:
			if (b_mod_ui_is_booting)
			{
				ui_loading_splash_image_open(NULL);
				mod_ui_screen = M_UI_SCRN_SPLASH_C;

				mod_ui_splash_tmout = make_timeout_time_ms (M_UI_SPLASH_SCRN_TM_MS_C);

				splash_scrn_stt++;
			}
		break;

		case 1:
			if (get_absolute_time() > mod_ui_splash_tmout)
			{
				b_mod_ui_is_booting = false;
			
				splash_scrn_stt = 0;
			}
		break;

		default:
			splash_scrn_stt = 0;
		break;
	}
}

/**
 * @brief Handles main screen displaying
 * 
 */
static void mod_ui_main_screen_handler(void)
{
	if (mod_ui_screen != M_UI_SCRN_MAIN_C)
	{
		ui_main_open();

		mod_ui_screen = M_UI_SCRN_MAIN_C;
	}
	ui_main_update();
	ui_debug_update();
}

/**
 * @brief Handles loading screen displaying
 * 
 */
static void mod_ui_psu_screen_handler(void)
{
	static uint64_t last_retry = 0;

	if (mod_ui_screen != M_UI_SCRN_LOADING_C)
	{
		ui_loading_show_psu();
		
		mod_ui_screen = M_UI_SCRN_LOADING_C;
	}

	if (last_retry == 0) 
	{
		last_retry = time_us_64();
	}

	if ((time_us_64() - last_retry) > 3000000)  // every 3s
	{
		ui_loading_show_psu_status("Retrying...");
		drv_lamp_power_up_rails();
		last_retry = time_us_64();

		if (drv_lamp_is_power_ok())
		{
			drv_lamp_perform_type_test();
			drv_lamp_request_power_level(D_LAMP_PWR_100PCT_C);
			ui_main_open();
		}
	}
}


/*** END OF FILE ***/