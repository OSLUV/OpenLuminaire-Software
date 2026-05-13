/**
 * @file      mod_system.c
 * @author    The OSLUV Project
 * @brief     System module. This module handles all tasks related to system 
 *            supervision and application services.
 */


/* Includes ------------------------------------------------------------------*/

#include <stdio.h>
#include <string.h>
#include <hardware/watchdog.h>
#include <hardware/vreg.h>
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

SYS_STATUS_T g_sys_stt;
SYS_CTRL_T   g_sys_ctl;


/* Private variables  --------------------------------------------------------*/
/* Callback prototypes -------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/

static void mod_sys_reset(void);


/* Exported functions --------------------------------------------------------*/

/**
 * @brief System initialization procedure
 * 
 */
void mod_sys_init(void)
{
    uint32_t reset_reason;

    memset((void*)&g_sys_stt, 0, sizeof(SYS_STATUS_T));
    memset((void*)&g_sys_ctl, 0, sizeof(SYS_CTRL_T));

    debug_init();

    reset_reason = vreg_and_chip_reset_hw->chip_reset;
    
    if (reset_reason & VREG_AND_CHIP_RESET_CHIP_RESET_HAD_PSM_RESTART_BITS)
    {
        /* Debug reset */
    }
    else if (watchdog_caused_reboot() && watchdog_enable_caused_reboot())
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
	M_SYS_DBG_PRINT_TXT("g_drv_cfg.factory_lamp_type = %s", 
		                mod_lamp_get_lamp_type_str(drv_cfg_get_factory_lamp_type()));
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

    if (g_sys_ctl.task.reboot)
    {
        mod_sys_reset();
    }
}


/* Callback functions --------------------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/**
 * @brief Performs a controlled system reset
 * 
 */
static void mod_sys_reset(void)
{
    M_SYS_DBG_PRINT_WRN("Reseting system by WDT...");

    watchdog_hw->scratch[0] = M_SYS_RESET_MAGIC_KEY_C;
    watchdog_reboot(0, 0, 0);
}


/*** END OF FILE ***/