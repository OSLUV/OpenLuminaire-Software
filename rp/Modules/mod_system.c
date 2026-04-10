/**
 * @file      mod_system.c
 * @author    The OSLUV Project
 * @brief     System module. This module handles all tasks related to system 
 *            supervision and application services.
 */


/* Includes ------------------------------------------------------------------*/

#include <stdio.h>
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
    if (watchdog_enable_caused_reboot())
    {
        printf("Rebooted by Watchdog!\n");
    }

    drv_cfg_init();
	printf("g_drv_cfg.factory_lamp_type = %d\n", 
		   g_drv_cfg.factory_lamp_type);
}

/**
 * @brief   Enables WDT
 * @note    Watchdog: catches runtime hangs (brownout gray zone, stuck loops).
 *          Enabled after drv_usb_pd_negotiate() (long blocking) but before
 *          lamp_power_up_rails() (lamp could be on after this point).
 *          Feeds: main loop, type test loops, lamp_power_up_rails sleeps,
 *          drv_usb_pd_negotiate loop (for hot-plug re-negotiation).
 * 
 */
void mod_sys_startup_wdt(void)
{
    watchdog_enable(1500, true);
}

/**
 * @brief System services
 * 
 */
void mod_sys_services(void)
{
    watchdog_update();
    drv_cfg_save();
}

/* Callback functions --------------------------------------------------------*/
/* Private functions ---------------------------------------------------------*/


/*** END OF FILE ***/