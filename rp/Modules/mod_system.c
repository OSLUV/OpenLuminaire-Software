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
#include "Modules/system.h"
#include "Drivers/drv_config.h"
#include "Drivers/drv_debug.h"


/* Private define ------------------------------------------------------------*/

#define M_SYS_DBG_ID_STR_C          "mod_system          "
#define M_SYS_DBG_PRINTF(...)    	debug_print_f(__VA_ARGS__)
#define M_SYS_DBG_PRINT_TXT(...)    debug_print_mod_f(M_SYS_DBG_ID_STR_C, __VA_ARGS__)
#define M_SYS_DBG_PRINT_ERR(...)    debug_print_err(M_SYS_DBG_ID_STR_C, __VA_ARGS__)
#define M_SYS_DBG_PRINT_WRN(...)    debug_print_warn(M_SYS_DBG_ID_STR_C, __VA_ARGS__)
#define M_SYS_DBG_PRINT_OK(...)     debug_print_ok(M_SYS_DBG_ID_STR_C, __VA_ARGS__)

#define M_SYS_RESET_MAGIC_KEY_C     0xBABA1A6A


/* Private typedef -----------------------------------------------------------*/
/* Global variables  ---------------------------------------------------------*/

SYS_STATUS_T g_sys;


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
    debug_init();
    
    if (watchdog_enable_caused_reboot())
    {
        if (watchdog_hw->scratch[0] == M_SYS_RESET_MAGIC_KEY_C)                 /* Was it a controlled reset ? */
        {
            watchdog_hw->scratch[0] = 0;
        }
        else
        {
            M_SYS_DBG_PRINT_WRN("Rebooted by Watchdog!");
        }
    }

    drv_cfg_init();
	M_SYS_DBG_PRINT_TXT("g_drv_cfg.factory_lamp_type = %d", 
		                drv_cfg_get_factory_lamp_type());
}

/**
 * @brief   Enables WDT
 * @note    Watchdog: catches runtime hangs (brownout gray zone, stuck loops).
 *          Enabled after drv_usb_pd_negotiate() (long blocking) but before
 *          drv_lamp_power_up_rails() (lamp could be on after this point).
 *          Feeds: main loop, type test loops, drv_lamp_power_up_rails sleeps,
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

/**
 * @brief Performs a controlled system reset
 * 
 */
void mod_sys_reset(void)
{
    watchdog_hw->scratch[0] = M_SYS_RESET_MAGIC_KEY_C;
    watchdog_reboot(0, 0, 0);
}


/* Callback functions --------------------------------------------------------*/
/* Private functions ---------------------------------------------------------*/


/*** END OF FILE ***/