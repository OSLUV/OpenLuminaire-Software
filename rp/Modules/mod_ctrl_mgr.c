/**
 * @file      mod_ctrl_mgr.c
 * @author    The OSLUV Project
 * @brief     Control Manager module. This module handles all control tasks,
 *            such as lamp, fan, mmWave radar, accelerometer and magnetometer.
 */


/* Includes ------------------------------------------------------------------*/

#include <stdio.h>
#include "Modules/mod_ctrl_mgr.h"
#include "Modules/system.h"
#include "Drivers/drv_accelerometer.h"
#include "Drivers/drv_magnetometer.h"
#include "Drivers/drv_lamp.h"
#include "Drivers/drv_radar.h"
#include "Drivers/drv_fan.h"
#include "safety_logic.h"


/* Private define ------------------------------------------------------------*/
/* Private typedef -----------------------------------------------------------*/
/* Global variables  ---------------------------------------------------------*/
/* Private variables  --------------------------------------------------------*/
/* Callback prototypes -------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/

static void mod_ctrl_lamp_handler(void);


/* Exported functions --------------------------------------------------------*/

/**
 * @brief Control Manager module initialization procedure
 * 
 */
void mod_ctrl_init(void)
{
	drv_acc_init();
	drv_mag_init();
	drv_lamp_init();
	
	drv_radar_init();
	drv_fan_init();
	drv_fan_set_speed(100);

    drv_lamp_power_up_rails();

	printf("Scripted start...\n");

	if (drv_lamp_is_power_ok()) 
	{
		drv_lamp_perform_type_test();
		drv_lamp_request_power_level(D_LAMP_PWR_100PCT_C);
	}
}

/**
 * @brief Control Manager module tasks
 * 
 */
void mod_ctrl_manager(void)
{
    drv_acc_update();
	g_sys.acc_x = g_drv_acc_x;
	g_sys.acc_y = g_drv_acc_y;
	g_sys.acc_z = g_drv_acc_z;
	g_sys.acc_pointing_down_angle = drv_acc_get_pointing_down_angle();

    drv_mag_update();
	g_sys.mag_x = g_drv_mag_x;
	g_sys.mag_y = g_drv_mag_y;
	g_sys.mag_z = g_drv_mag_z;

    drv_radar_update();

    mod_ctrl_lamp_handler();

	safety_logic_update();
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
}


/*** END OF FILE ***/