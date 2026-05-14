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
#include "Modules/mod_ui_scrn_main.h"
#include "Modules/mod_ui_screens.h"
#include "Modules/mod_ui_scrn_loading.h"
#include "Modules/mod_ui_scrn_debug.h"
#include "Modules/mod_lamp_defs.h"
#include "Modules/system.h"
#include "Drivers/drv_buttons.h"
#include "Drivers/drv_debug.h"
#include "Drivers/drv_display.h"


/* Private define ------------------------------------------------------------*/

#define M_UI_DBG_ID_STR_C        	"mod_ui              "
#define M_UI_DBG_PRINTF(...)    	debug_print_f(__VA_ARGS__)
#define M_UI_DBG_PRINT_TXT(...)		debug_print_mod_f(M_UI_DBG_ID_STR_C, __VA_ARGS__)
#define M_UI_DBG_PRINT_ERR(...)		debug_print_err(M_UI_DBG_ID_STR_C, __VA_ARGS__)
#define M_UI_DBG_PRINT_WRN(...)		debug_print_warn(M_UI_DBG_ID_STR_C, __VA_ARGS__)
#define M_UI_DBG_PRINT_OK(...)		debug_print_ok(M_UI_DBG_ID_STR_C, __VA_ARGS__)

#define M_UI_SPLASH_SCRN_TM_MS_C	1250										/* Time (in ms) Splash Screen will remain on screen */
#define M_UI_LOADING_RETRY_TM_MS_C	3000										/* Time (in ms) for loading to retry */
#define M_UI_STANDBY_TM_MS_C		(5 * 60 * 1000)								/* Maximum time (in ms) for the UI to remain unused */


/* Private typedef -----------------------------------------------------------*/
/* Global variables  ---------------------------------------------------------*/

M_UI_SCRN_E 			g_mod_ui_new_screen;


/* Private variables  --------------------------------------------------------*/

static bool 			b_mod_ui_is_booting;
static bool 			b_mod_ui_main_is_init;
static M_UI_SCRN_E		mod_ui_screen;
static absolute_time_t  mod_ui_splash_tmout;
static absolute_time_t  mod_ui_loading_retry_tmout;
static absolute_time_t  mod_ui_stanby_tmout;


/* Callback prototypes -------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/

static inline void mod_ui_backlight_turn_on(void);
static inline void mod_ui_backlight_turn_off(void);
static void mod_ui_backlight_handler(void);
static void mod_ui_screen_handler(void);
static void mod_ui_splash_screen_handler(void);
static void mod_ui_main_screen_handler(void);
static void mod_ui_debug_screen_handler(void);
static void mod_ui_psu_screen_handler(void);


/* Exported functions --------------------------------------------------------*/

/**
 * @brief Control Manager module initialization procedure
 * 
 */
void mod_ui_init(void)
{
	g_sys_stt.is_disp_on  = false;

	b_mod_ui_is_booting   = true;
	b_mod_ui_main_is_init = false;

    drv_buttons_init();

	drv_display_screen_turn_off();

	lv_init();
	drv_display_init();

	mod_ui_screen		= M_UI_SCRN_NONE_C;
	g_mod_ui_new_screen = M_UI_SCRN_SPLASH_C;

	ui_loading_splash_image_init();

	/* TODO: This next block can be deleted after fixing usb-pd negotiation to 
	   non-blocking
	*/
	ui_loading_splash_image_open(NULL);
	g_sys_stt.is_disp_on = true;
	mod_ui_stanby_tmout = make_timeout_time_ms (M_UI_STANDBY_TM_MS_C);
	/**/

    mod_ui_debug_init();
}

/**
 * @brief Control Manager module tasks
 * 
 */
void mod_ui_manager(void)
{
	drv_buttons_monitor();

	mod_ui_backlight_handler();

	lv_timer_handler();

	mod_ui_screen_handler();
}

/* Callback functions --------------------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/**
 * @brief Handles display's backlight turn on
 * 
 */
static inline void mod_ui_backlight_turn_on(void)
{
	drv_display_screen_turn_on();  												// Back-light on + one flush
	
	g_sys_stt.is_disp_on = true;
}

/**
 * @brief Handles display's backlight turn off
 * 
 */
static inline void mod_ui_backlight_turn_off(void)
{
	drv_display_screen_turn_off();
	
	g_sys_stt.is_disp_on = false;
}

/**
 * @brief Handles display backlight control
 * 
 */
static void mod_ui_backlight_handler(void)
{
	uint8_t turn_on;

	turn_on = 0;

	if (g_sys_stt.is_disp_on)
	{
		if ((get_absolute_time() > mod_ui_stanby_tmout) &&
			(mod_ui_screen != M_UI_SCRN_LOADING_C))
		{
			mod_ui_backlight_turn_off();
		}
	}

	if (g_drv_buttons_released)
	{
		if (!g_sys_stt.is_disp_on)
		{
			turn_on = 1;
		}

		mod_ui_stanby_tmout =  make_timeout_time_ms (M_UI_STANDBY_TM_MS_C);
	}

	if ((mod_ui_screen == M_UI_SCRN_LOADING_C) && !g_sys_stt.is_disp_on)
	{
		turn_on = 1;
	}

	if (turn_on)
	{
		mod_ui_backlight_turn_on();
	}
}

/**
 * @brief Handles splash screen at system bootup
 * 
 */
static void mod_ui_screen_handler(void)
{
	if (mod_ui_screen != g_mod_ui_new_screen) 									/* New screen is required? */
	{
		switch (g_mod_ui_new_screen)
		{
			case M_UI_SCRN_NONE_C:
			break;

			case M_UI_SCRN_SPLASH_C:
				ui_loading_splash_image_open(NULL);
			break;
			
			case M_UI_SCRN_LOADING_C:
				mod_ui_loading_retry_tmout = 0;
			break;
			
			case M_UI_SCRN_MAIN_C:
			default:
				if (!b_mod_ui_main_is_init)
				{
					mod_ui_main_init();

					b_mod_ui_main_is_init = true;
				}
				mod_ui_main_open();
			break;
			
			case M_UI_SCRN_DEBUG_C:
				mod_ui_debug_open();
			break;
		}
		mod_ui_screen = g_mod_ui_new_screen;
	}
	
	
	switch (mod_ui_screen)
	{
		case M_UI_SCRN_NONE_C:
		break;

		case M_UI_SCRN_SPLASH_C:
			mod_ui_splash_screen_handler();
		break;
		
		case M_UI_SCRN_LOADING_C:
			mod_ui_psu_screen_handler();
		break;
		
		case M_UI_SCRN_MAIN_C:
			mod_ui_main_screen_handler();
		break;
		
		case M_UI_SCRN_DEBUG_C:
			mod_ui_debug_screen_handler();
		break;
		
		default:
			g_mod_ui_new_screen = M_UI_SCRN_MAIN_C;
		break;
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
				mod_ui_splash_tmout = make_timeout_time_ms (M_UI_SPLASH_SCRN_TM_MS_C);

				splash_scrn_stt++;
			}
		break;

		case 1:
			if (!g_sys_stt.is_rails_powering_on && (get_absolute_time() > mod_ui_splash_tmout))
			{
				b_mod_ui_is_booting = false;

				if (!g_sys_stt.is_power_ok)
				{
					M_UI_DBG_PRINT_TXT("Switching to LOADING screen due to power levels at startup");

					g_mod_ui_new_screen = M_UI_SCRN_LOADING_C;
				
					splash_scrn_stt = 0;
				}
				else if ((g_sys_stt.lamp_type != M_LAMP_TYPE_UNKNOWN_C))
				{
					M_UI_DBG_PRINT_TXT("Switching to MAIN screen");

					g_mod_ui_new_screen = M_UI_SCRN_MAIN_C;
				
					splash_scrn_stt = 0;
				}
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
	if (g_sys_stt.is_power_ok) 
	{
		mod_ui_main_handler();
	}
	else
	{
		M_UI_DBG_PRINT_TXT("Switching to LOADING screen due to power levels");

		g_mod_ui_new_screen = M_UI_SCRN_LOADING_C;
	}
}

/**
 * @brief Handles main screen displaying
 * 
 */
static void mod_ui_debug_screen_handler(void)
{
	if (g_sys_stt.is_power_ok) 
	{
		mod_ui_debug_handler();
	}
	else
	{
		g_mod_ui_new_screen = M_UI_SCRN_LOADING_C;
	}
}

/**
 * @brief Handles loading screen displaying
 * 
 */
static void mod_ui_psu_screen_handler(void)
{
    static uint8_t stt_mchn = 0;

	ui_loading_show_psu();

	switch (stt_mchn)
	{
		case 0:
			if ((mod_ui_loading_retry_tmout == 0) || 
				(get_absolute_time() > mod_ui_loading_retry_tmout))
			{
				ui_loading_show_psu_status("Retrying...");

				g_sys_ctl.task.lamp_test_b = 1;

				stt_mchn++;
			}
		break;

		case 1:
			if (!g_sys_ctl.task.lamp_test_b && g_sys_stt.task.lamp_test_b)		// Lamp test is being executed?
			{
				stt_mchn++;
			}
		break;

		case 2:
			if (!g_sys_stt.task.lamp_test_b) 									// Lamp test procedure is finished ?
			{
				mod_ui_loading_retry_tmout = make_timeout_time_ms(M_UI_LOADING_RETRY_TM_MS_C);

				if (g_sys_stt.is_power_ok)
				{
					g_sys_ctl.lamp_req_pwr_level = M_LAMP_PWR_100PCT_C;
				
					g_mod_ui_new_screen = M_UI_SCRN_MAIN_C;
				}

				stt_mchn = 0;
			}
		break;

		default:
			stt_mchn = 0;
		break;
	}
}


/*** END OF FILE ***/