/**
 * @file      mod_pow_mgr.c
 * @author    The OSLUV Project
 * @brief     Power Manager module. This module handles system source power.
 */


/* Includes ------------------------------------------------------------------*/

#include <stdio.h>
#include <pico/stdlib.h>
#include "Modules/mod_pow_mgr.h"
#include "Modules/system.h"
#include "Drivers/drv_adc_volt.h"
#include "Drivers/drv_debug.h"
#include "Drivers/drv_usb_pd.h"


/* Private define ------------------------------------------------------------*/

#define M_POW_DBG_ID_STR_C        		"mod_pow             "
#define M_POW_DBG_PRINTF(...)    		debug_print_f(__VA_ARGS__)
#define M_POW_DBG_PRINT_TXT(...)		debug_print_mod_f(M_POW_DBG_ID_STR_C, __VA_ARGS__)
#define M_POW_DBG_PRINT_ERR(...)		debug_print_err(M_POW_DBG_ID_STR_C, __VA_ARGS__)
#define M_POW_DBG_PRINT_WRN(...)		debug_print_warn(M_POW_DBG_ID_STR_C, __VA_ARGS__)
#define M_POW_DBG_PRINT_OK(...)			debug_print_ok(M_POW_DBG_ID_STR_C, __VA_ARGS__)

#define M_POW_24V_PASSIVE_THRESHOLD_C	(float)(3.0)                            /* V — VSYS leaks through on V1.2, reads ~0V on V1.1 */
#define M_POW_USB_PLUGGED_C				true
#define M_POW_USB_UNPLUGGED_C			false

#define M_POW_HW_REV_1_1_NEG_MV_C		12000									/* Hw rev 1.1 milli-volts to negotiate */
#define M_POW_HW_REV_1_1_NEG_MA_C		1800									/* Hw rev 1.1 milli-amps to negotiate */
#define M_POW_HW_REV_1_2_NEG_MV_C		20000									/* Hw rev 1.2 milli-volts to negotiate */
#define M_POW_HW_REV_1_2_NEG_MA_C		1000									/* Hw rev 1.2 milli-amps to negotiate */


/* Private typedef -----------------------------------------------------------*/
/* Global variables  ---------------------------------------------------------*/

bool g_mod_pow_hw_is_rev1_2_b;
bool g_mod_pow_is_usb_connected_b;
uint32_t g_mod_pow_usb_negotiated_ma;
uint32_t g_mod_pow_usb_negotiated_mv;


/* Private variables  --------------------------------------------------------*/

uint8_t mod_pow_last_usb_conn_stt;


/* Callback prototypes -------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/

static void mod_pow_get_hw_revision(void);
static void mod_pow_usb_hot_plug_handler(void);


/* Exported functions --------------------------------------------------------*/

/**
 * @brief Power Manager module initialization procedure
 * 
 */
void mod_pow_init(void)
{
	drv_usb_pd_init();
    drv_adc_volt_init();

    g_mod_pow_hw_is_rev1_2_b = false; 											/* Assume hardware revision 1.1 */

    mod_pow_get_hw_revision();

	drv_usb_pd_negotiate(true);
	drv_usb_pd_init_update(); /* CLEAR: Checks if there is power over USB. Delete when drv_usb_pd_negotiate is fully refactored */

	mod_pow_last_usb_conn_stt   = drv_usb_pd_is_connected();
	g_mod_pow_usb_negotiated_ma = drv_usb_pd_get_negotiated_ma();
	g_mod_pow_usb_negotiated_mv = drv_usb_pd_get_negotiated_mv();
}

/**
 * @brief Power Manager module tasks
 * 
 */
void mod_pow_manager(void)
{
    drv_adc_volt_update();
	g_sys.v_vbus = g_adc_v_vbus;
	g_sys.v_12v  = g_adc_v_12v;
	g_sys.v_24v  = g_adc_v_24v;

	mod_pow_usb_hot_plug_handler();
}


/* Callback functions --------------------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/**
 * @brief Detects board version by passive ADC read.
 *
 * With both rail enables off, VSYS leaks through to the 24V sense on V1.2
 * (~4.6V on 5V USB) but reads ~0V on V1.1. No boost pulse needed.
 *
 * Must be called after drv_adc_volt_init(), but before drv_usb_pd_negotiate().
 */
static void mod_pow_get_hw_revision(void)
{
	drv_adc_volt_update();

	M_POW_DBG_PRINT_TXT("Board detection: 24V sense = %.2fV (threshold = %.1fV)",
		   g_adc_v_24v, M_POW_24V_PASSIVE_THRESHOLD_C);

	if (g_adc_v_24v > M_POW_24V_PASSIVE_THRESHOLD_C)
	{
		g_mod_pow_hw_is_rev1_2_b = true;
		M_POW_DBG_PRINT_TXT("Board detected: V1.2 (VSYS on 24V sense)");
	}
	else
	{
		g_mod_pow_hw_is_rev1_2_b = false;
		M_POW_DBG_PRINT_TXT("Board detected: V1.1 (no voltage on 24V sense)");
	}
}

/**
 * @brief Handles USB-C cable hot-plug
 * 
 */
static void mod_pow_usb_hot_plug_handler(void)
{
	if (mod_pow_last_usb_conn_stt == M_POW_USB_UNPLUGGED_C)
	{ 
	    if (drv_usb_pd_is_connected())
		{
			/* USB just hot-plugged — write board-safe PDOs and reset so the
			* STUSB4500 renegotiates with our values instead of NVM defaults.
			* On V1.1, NVM defaults may request 20V which would damage the
			* 12V rail. */
			M_POW_DBG_PRINT_TXT("MOD POW. USB-C hot-plug detected, configuring safe PDOs");

			if (g_mod_pow_hw_is_rev1_2_b)
			{
				drv_usb_pd_set_pdo(M_POW_HW_REV_1_2_NEG_MV_C, 
								   M_POW_HW_REV_1_2_NEG_MA_C);
				g_mod_pow_usb_negotiated_mv = M_POW_HW_REV_1_2_NEG_MV_C;
			}
			else
			{
				drv_usb_pd_set_pdo(M_POW_HW_REV_1_1_NEG_MV_C, 
								   M_POW_HW_REV_1_1_NEG_MA_C);
				g_mod_pow_usb_negotiated_mv = M_POW_HW_REV_1_1_NEG_MV_C;
			}
			drv_usb_pd_reset();

			mod_pow_last_usb_conn_stt    = M_POW_USB_PLUGGED_C;
			g_mod_pow_is_usb_connected_b = true;
		}
	}
	else
	{
		if (!drv_usb_pd_is_connected())
		{
			M_POW_DBG_PRINT_TXT("MOD POW. USB-C disconnected");

			mod_pow_last_usb_conn_stt    = M_POW_USB_UNPLUGGED_C;
			g_mod_pow_is_usb_connected_b = true;
		}
	}
}


/*** END OF FILE ***/