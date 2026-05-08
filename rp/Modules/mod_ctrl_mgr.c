/**
 * @file      mod_ctrl_mgr.c
 * @author    The OSLUV Project
 * @brief     Control Manager module. This module handles all control tasks,
 *            such as lamp, fan, mmWave radar, accelerometer and magnetometer.
 */


/* Includes ------------------------------------------------------------------*/

#include <stdio.h>
#include "pico/time.h"
#include "Modules/mod_ctrl_mgr.h"
#include "Modules/mod_system.h"
#include "Modules/system.h"
#include "Drivers/drv_accelerometer.h"
#include "Drivers/drv_debug.h"
#include "Drivers/drv_magnetometer.h"
#include "Drivers/drv_lamp.h"
#include "Drivers/drv_radar.h"
#include "Drivers/drv_fan.h"
#include "safety_logic.h"

#include <hardware/watchdog.h> // TODO: Remove when mod_ui_psu_screen_handler is updated


/* Private define ------------------------------------------------------------*/

#define M_CTRL_DBG_ID_STR_C        "mod_ctrl            "
#define M_CTRL_DBG_PRINTF(...)    	debug_print_f(__VA_ARGS__)
#define M_CTRL_DBG_PRINT_TXT(...)	debug_print_mod_f(M_CTRL_DBG_ID_STR_C, __VA_ARGS__)
#define M_CTRL_DBG_PRINT_ERR(...)	debug_print_err(M_CTRL_DBG_ID_STR_C, __VA_ARGS__)
#define M_CTRL_DBG_PRINT_WRN(...)	debug_print_warn(M_CTRL_DBG_ID_STR_C, __VA_ARGS__)
#define M_CTRL_DBG_PRINT_OK(...)	debug_print_ok(M_CTRL_DBG_ID_STR_C, __VA_ARGS__)


/* Private typedef -----------------------------------------------------------*/
/* Global variables  ---------------------------------------------------------*/
/* Private variables  --------------------------------------------------------*/

static bool 			b_mod_ctrl_is_startup;
static bool 			b_mod_ctrl_make_retest;
static absolute_time_t  mod_ctrl_delay_tmout;


/* Callback prototypes -------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/

static void mod_ctrl_lamp_handler(void);
static void mod_ctrl_lamp_test_handler(void);
static void mod_ctrl_lamp_test_n_reboot_handler(void);


/* Exported functions --------------------------------------------------------*/

/**
 * @brief Control Manager module initialization procedure
 * 
 */
void mod_ctrl_init(void)
{
	b_mod_ctrl_is_startup  = true;
	b_mod_ctrl_make_retest = false;

	drv_acc_init();
	drv_mag_init();
	drv_lamp_init();
	
	drv_radar_init();
	drv_fan_init();
	drv_fan_set_speed(100);
}

/**
 * @brief Control Manager module tasks
 * 
 */
void mod_ctrl_manager(void)
{
    drv_acc_update();
	g_sys_stt.acc_x = g_drv_acc_x;
	g_sys_stt.acc_y = g_drv_acc_y;
	g_sys_stt.acc_z = g_drv_acc_z;
	g_sys_stt.acc_pointing_down_angle = drv_acc_get_pointing_down_angle();

    drv_mag_update();
	g_sys_stt.mag_x = g_drv_mag_x;
	g_sys_stt.mag_y = g_drv_mag_y;
	g_sys_stt.mag_z = g_drv_mag_z;

    drv_radar_update();

    mod_ctrl_lamp_handler();

	if (drv_lamp_get_type() != D_LAMP_TYPE_UNKNOWN_C)
	{
		safety_logic_update();
	}

	if (b_mod_ctrl_is_startup)
	{
		b_mod_ctrl_is_startup = false;
	}
}


/* Callback functions --------------------------------------------------------*/

// Execute drv_lamp_reset_type from UI command

/* Private functions ---------------------------------------------------------*/

/**
 * @brief Handles lamp control
 * 
 */
static void mod_ctrl_lamp_handler(void)
{
    drv_lamp_update();

	mod_ctrl_lamp_test_handler();
	mod_ctrl_lamp_test_n_reboot_handler();
}

/**
 * @brief Tests lamp to get its type
 * 
 */
static void mod_ctrl_lamp_test_handler(void)
{
    static uint8_t stt_mchn = 0;
    static bool    power_up_lamp_at_end_b = false;

	switch (stt_mchn)
	{
		case 0:
			if (b_mod_ctrl_is_startup)
			{
				if (drv_lamp_get_type() == D_LAMP_TYPE_UNKNOWN_C)
				{
					g_sys_ctl.task.lamp_test_b = 1;

					power_up_lamp_at_end_b = true;
				}
			}

			if (g_sys_ctl.task.lamp_test_b)
			{
				M_CTRL_DBG_PRINT_TXT("Performing lamp test");

				g_sys_ctl.task.lamp_test_b = 0;
				g_sys_stt.task.lamp_test_b = 1;

				drv_lamp_power_up_rails();

				stt_mchn++;
			}
		break;

		case 1:
			if (drv_lamp_is_power_ok()) 
			{
				stt_mchn++;
			}
		break;

		case 2:
			if (drv_lamp_perform_type_test() != 0)
			{
				g_sys_stt.task.lamp_test_b = 0;

				if (power_up_lamp_at_end_b)
				{
					power_up_lamp_at_end_b = false;

					drv_lamp_request_power_level(D_LAMP_PWR_100PCT_C);
				}

				stt_mchn = 0;
			}
		break;

		default:
			stt_mchn = 0;
		break;
	}
}

/**
 * @brief Starts a lamp type test and issues a system reboot when finished
 * 
 */
static void mod_ctrl_lamp_test_n_reboot_handler(void)
{
    static uint8_t stt_mchn = 0;

	switch (stt_mchn)
	{
		case 0:
			if (g_sys_ctl.task.lamp_test_n_reboot_b)
			{
				g_sys_ctl.task.lamp_test_n_reboot_b = 0;
				g_sys_stt.task.lamp_test_n_reboot_b = 1;

				M_CTRL_DBG_PRINT_WRN("Performing lamp test & reboot");

				drv_lamp_request_power_level(D_LAMP_PWR_OFF_C);

				mod_ctrl_delay_tmout = make_timeout_time_ms(100);

				// TODO: Is this delay required or was for making sure that D_LAMP_PWR_OFF_C state was applied?

				stt_mchn++;
			}
		break;

		case 1:
			if (get_absolute_time() > mod_ctrl_delay_tmout)
			{
				drv_lamp_reset_type();

				g_sys_ctl.task.lamp_test_b = 1;

				stt_mchn++;
			}
		break;

		case 2:
			if (!g_sys_ctl.task.lamp_test_b && g_sys_stt.task.lamp_test_b)		/* Lamp test is being executed? */
			{
				stt_mchn++;
			}
		break;

		case 3:
			if (!g_sys_stt.task.lamp_test_b)
			{
				M_CTRL_DBG_PRINT_OK("Retest complete, type=%d. Rebooting to apply new UI layout...",
									drv_lamp_get_type());

				g_sys_stt.task.lamp_test_n_reboot_b = 0;

				g_sys_ctl.task.reboot_b = 1; 									/* Issue a system reboot */

				stt_mchn = 0;
			}
		break;

		default:
			stt_mchn = 0;
		break;
	}
}


/*** END OF FILE ***/