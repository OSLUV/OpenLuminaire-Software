/**
 * @file      mod_system.c
 * @author    The OSLUV Project
 * @brief     System module. This module handles all tasks related to system 
 *            supervision and application services.
 */


/* Includes ------------------------------------------------------------------*/

#include <hardware/watchdog.h>
#include "Modules/mod_system.h"
#include "Drivers/drv_config.h"


/* Private define ------------------------------------------------------------*/
/* Private typedef -----------------------------------------------------------*/
/* Global variables  ---------------------------------------------------------*/
/* Private variables  --------------------------------------------------------*/
/* Callback prototypes -------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/

/* Exported functions --------------------------------------------------------*/

/**
 * @brief System initialization procedure
 * 
 */
void mod_sys_init(void)
{
    drv_cfg_init();
	printf("g_drv_cfg.factory_lamp_type = %d\n", 
		   g_drv_cfg.factory_lamp_type);
}

/**
 * @brief System services
 * 
 */
void mod_system_services(void)
{
    watchdog_update();
    drv_cfg_save();
}

/* Callback functions --------------------------------------------------------*/
/* Private functions ---------------------------------------------------------*/


/*** END OF FILE ***/