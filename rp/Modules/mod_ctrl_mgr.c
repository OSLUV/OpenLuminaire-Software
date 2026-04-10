/**
 * @file      mod_ctrl_mgr.c
 * @author    The OSLUV Project
 * @brief     Control Manager module. This module handles all control tasks,
 *            such as lamp, fan, mmWave radar, accelerometer and magnetometer.
 */


/* Includes ------------------------------------------------------------------*/

#include <stdio.h>
#include "Modules/mod_ctrl_mgr.h"
#include "Drivers/drv_accelerometer.h"
#include "Drivers/drv_magnetometer.h"
#include "Drivers/drv_lamp.h"
#include "Drivers/drv_radar.h"
#include "Drivers/drv_fan.h"


/* Private define ------------------------------------------------------------*/
/* Private typedef -----------------------------------------------------------*/
/* Global variables  ---------------------------------------------------------*/
/* Private variables  --------------------------------------------------------*/
/* Callback prototypes -------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/

/* Exported functions --------------------------------------------------------*/

/**
 * @brief Control Manager module initialization procedure
 * 
 */
void mod_ctrl_init(void)
{
	drv_acc_init();
	drv_mag_init();
	lamp_init();
	
	drv_radar_init();
	drv_fan_init();
	drv_fan_set_speed(100);

    lamp_power_up_rails();

	printf("Scripted start...\n");

	if (lamp_is_power_ok()) 
	{
		lamp_perform_type_test();
		lamp_request_power_level(LAMP_PWR_100PCT_C);
	}
}

/**
 * @brief Control Manager module tasks
 * 
 */
void mod_ctrl_manager(void)
{
    drv_acc_update();
    drv_mag_update();
    drv_radar_update();
    lamp_update();
}

/* Callback functions --------------------------------------------------------*/
/* Private functions ---------------------------------------------------------*/


/*** END OF FILE ***/