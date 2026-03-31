/**
 * @file      mod_pow_mgr.c
 * @author    The OSLUV Project
 * @brief     Power Manager module. This module handles system source power.
 */


/* Includes ------------------------------------------------------------------*/

#include <stdio.h>
#include <pico/stdlib.h>
#include "Modules/mod_pow_mgr.h"
#include "Drivers/drv_board.h"
#include "Drivers/drv_adc_volt.h"
#include "Drivers/drv_usb_pd.h"


/* Private define ------------------------------------------------------------*/
/* Private typedef -----------------------------------------------------------*/
/* Global variables  ---------------------------------------------------------*/

bool g_mod_pow_is_v1_2_b;

/* Private variables  --------------------------------------------------------*/
/* Callback prototypes -------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/

/* Exported functions --------------------------------------------------------*/

/**
 * @brief Power Manager module initialization procedure
 * 
 */
void mod_pow_init(void)
{
    adc_volt_init();

    g_mod_pow_is_v1_2_b = false;

    board_init();

    g_mod_pow_is_v1_2_b = board_is_v1_2();

	usbpd_negotiate(true);
	usbpd_init_update();
}

/**
 * @brief Power Manager module tasks
 * 
 */
void mod_pow_manager(void)
{
    adc_volt_update();

	usbpd_update();
}

/* Callback functions --------------------------------------------------------*/
/* Private functions ---------------------------------------------------------*/


/*** END OF FILE ***/