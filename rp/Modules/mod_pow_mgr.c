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
#include "Drivers/drv_usb_pd.h"


/* Private define ------------------------------------------------------------*/

#define MOD_POW_24V_PASSIVE_THRESHOLD_C	(float)(3.0)                            /* V — VSYS leaks through on V1.2, reads ~0V on V1.1 */
#define MOD_POW_USB_PLUGGED_C			true
#define MOD_POW_USB_UNPLUGGED_C			false

#define MOD_POW_HW_REV_1_1_NEG_MV_C		12000									/* Hw rev 1.1 milli-volts to negotiate */
#define MOD_POW_HW_REV_1_1_NEG_MA_C		1800									/* Hw rev 1.1 milli-amps to negotiate */
#define MOD_POW_HW_REV_1_2_NEG_MV_C		20000									/* Hw rev 1.2 milli-volts to negotiate */
#define MOD_POW_HW_REV_1_2_NEG_MA_C		1000									/* Hw rev 1.2 milli-amps to negotiate */


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

	printf("Board detection: 24V sense = %.2fV (threshold = %.1fV)\n",
		   g_adc_v_24v, MOD_POW_24V_PASSIVE_THRESHOLD_C);

	if (g_adc_v_24v > MOD_POW_24V_PASSIVE_THRESHOLD_C)
	{
		g_mod_pow_hw_is_rev1_2_b = true;
		printf("Board detected: V1.2 (VSYS on 24V sense)\n");
	}
	else
	{
		g_mod_pow_hw_is_rev1_2_b = false;
		printf("Board detected: V1.1 (no voltage on 24V sense)\n");
	}
}

/**
 * @brief Handles USB-C cable hot-plug
 * 
 */
static void mod_pow_usb_hot_plug_handler(void)
{
	if (mod_pow_last_usb_conn_stt == MOD_POW_USB_UNPLUGGED_C)
	{ 
	    if (drv_usb_pd_is_connected())
		{
			/* USB just hot-plugged — write board-safe PDOs and reset so the
			* STUSB4500 renegotiates with our values instead of NVM defaults.
			* On V1.1, NVM defaults may request 20V which would damage the
			* 12V rail. */
			printf("MOD POW. USB-C hot-plug detected, configuring safe PDOs\n");

			if (g_mod_pow_hw_is_rev1_2_b)
			{
				drv_usb_pd_set_pdo(MOD_POW_HW_REV_1_2_NEG_MV_C, 
								   MOD_POW_HW_REV_1_2_NEG_MA_C);
				g_mod_pow_usb_negotiated_mv = MOD_POW_HW_REV_1_2_NEG_MV_C;
			}
			else
			{
				drv_usb_pd_set_pdo(MOD_POW_HW_REV_1_1_NEG_MV_C, 
								   MOD_POW_HW_REV_1_1_NEG_MA_C);
				g_mod_pow_usb_negotiated_mv = MOD_POW_HW_REV_1_1_NEG_MV_C;
			}
			drv_usb_pd_reset();

			mod_pow_last_usb_conn_stt    = MOD_POW_USB_PLUGGED_C;
			g_mod_pow_is_usb_connected_b = true;
		}
	}
	else
	{
		if (!drv_usb_pd_is_connected())
		{
			printf("MOD POW. USB-C disconnected\n");

			mod_pow_last_usb_conn_stt    = MOD_POW_USB_UNPLUGGED_C;
			g_mod_pow_is_usb_connected_b = true;
		}
	}
}


/*** END OF FILE ***/