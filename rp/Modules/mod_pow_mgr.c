/**
 * @file      mod_pow_mgr.c
 * @author    The OSLUV Project
 * @brief     Power Manager module. This module handles system source power.
 */


/* Includes ------------------------------------------------------------------*/

#include <stdio.h>
#include <pico/stdlib.h>
#include "pico/time.h"
#include "Modules/mod_pow_mgr.h"
#include "Modules/system.h"
#include "Drivers/drv_adc_volt.h"
#include "Drivers/drv_debug.h"
#include "Drivers/drv_power.h"
#include "Drivers/drv_usb_pd.h"


/* Private define ------------------------------------------------------------*/

#define M_POW_DBG_ID_STR_C        			"mod_pow             "
#define M_POW_DBG_PRINTF(...)    			debug_print_f(__VA_ARGS__)
#define M_POW_DBG_PRINT_TXT(...)			debug_print_mod_f(M_POW_DBG_ID_STR_C, __VA_ARGS__)
#define M_POW_DBG_PRINT_ERR(...)			debug_print_err(M_POW_DBG_ID_STR_C, __VA_ARGS__)
#define M_POW_DBG_PRINT_WRN(...)			debug_print_warn(M_POW_DBG_ID_STR_C, __VA_ARGS__)
#define M_POW_DBG_PRINT_OK(...)				debug_print_ok(M_POW_DBG_ID_STR_C, __VA_ARGS__)

#define M_POW_24V_PASSIVE_THRESHOLD_C		(float)(3.0)                        /* V — VSYS leaks through on V1.2, reads ~0V on V1.1 */
#define M_POW_USB_PLUGGED_C					true
#define M_POW_USB_UNPLUGGED_C				false

#define M_POW_12V_RAIL_MIN_V_C				(float)10.5
#define M_POW_12V_RAIL_MAX_V_C				(float)13.5
#define M_POW_24V_RAIL_MIN_V_C				(float)21.0
#define M_POW_24V_RAIL_MAX_V_C				(float)27.0


#define M_POW_HW_REV_1_1_NEG_MV_C			12000								/* Hw rev 1.1 milli-volts to negotiate */
#define M_POW_HW_REV_1_1_NEG_MA_C			1800								/* Hw rev 1.1 milli-amps to negotiate */
#define M_POW_HW_REV_1_2_NEG_MV_C			20000								/* Hw rev 1.2 milli-volts to negotiate */
#define M_POW_HW_REV_1_2_NEG_MA_C			1000								/* Hw rev 1.2 milli-amps to negotiate */

#define M_POW_RAILS_STEPCOUNT_SOFTSTART_C	64


/* Private typedef -----------------------------------------------------------*/

static absolute_time_t	mod_pow_rail_delay_tmout;


/* Global variables  ---------------------------------------------------------*/

bool 	 g_mod_pow_is_usb_connected_b;
uint32_t g_mod_pow_usb_negotiated_ma;
uint32_t g_mod_pow_usb_negotiated_mv;


/* Private variables  --------------------------------------------------------*/

uint8_t mod_pow_last_usb_conn_stt;


/* Callback prototypes -------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/

static inline void mod_pow_v_src_adc_monitor(void);
static inline uint8_t mod_pow_is_12v_rail_in_range(void);
static inline uint8_t mod_pow_is_24v_rail_in_range(void);
static void mod_pow_get_hw_revision(void);
static void mod_pow_monitor(void);
static void mod_pow_usb_hot_plug_handler(void);
static void mod_pow_rails_ctrl_handler(void);
static int8_t mod_pow_rails_on_v1_1_handler(void);
static int8_t mod_pow_rails_off_v1_1_handler(void);
static int8_t mod_pow_rails_on_v1_2_handler(void);
static int8_t mod_pow_rails_off_v1_2_handler(void);
static int8_t mod_pow_enable_12v_rail(void);
static int8_t mod_pow_disable_12v_rail(void);


/* Exported functions --------------------------------------------------------*/

/**
 * @brief Power Manager module initialization procedure
 * 
 */
void mod_pow_init(void)
{
	g_sys_stt.task.rails_on  = 0;
	g_sys_stt.task.rails_off = 1;
	g_sys_ctl.task.rails_on  = 0;
	g_sys_ctl.task.rails_off = 0;

	drv_power_init();
	drv_usb_pd_init();
    drv_adc_volt_init();

    g_sys_stt.hw_is_1_2 = false; 												/* Assume hardware revision 1.1 */

	mod_pow_get_hw_revision();

	g_mod_pow_is_usb_connected_b = false;
	mod_pow_last_usb_conn_stt = drv_usb_pd_is_connected();
	if (mod_pow_last_usb_conn_stt)
	{
		drv_usb_pd_negotiate(true);

		g_mod_pow_is_usb_connected_b = true;
	}

	g_mod_pow_usb_negotiated_ma = drv_usb_pd_get_negotiated_ma();
	g_mod_pow_usb_negotiated_mv = drv_usb_pd_get_negotiated_mv();

	g_sys_ctl.task.rails_on = true; 											// Request power rails
	g_sys_stt.is_pwr_starting_up = true;
}

/**
 * @brief Power Manager module tasks
 * 
 */
void mod_pow_manager(void)
{
    mod_pow_v_src_adc_monitor();

	mod_pow_monitor();

	mod_pow_usb_hot_plug_handler();

	mod_pow_rails_ctrl_handler();
}


/* Callback functions --------------------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/**
 * @brief Updates ADC power sources voltage readings
 * 
 */
static inline void mod_pow_v_src_adc_monitor(void)
{
    drv_adc_volt_update();
	g_sys_stt.v_vbus = g_adc_v_vbus;
	g_sys_stt.v_12v  = g_adc_v_12v;
	g_sys_stt.v_24v  = g_adc_v_24v;
}

/**
 * @brief Checks if 12V power rail is the accepted range
 * 
 * @return uint8_t 0: false, 1: true
 */
static inline uint8_t mod_pow_is_12v_rail_in_range(void)
{
	if ((g_sys_stt.v_12v >= M_POW_12V_RAIL_MIN_V_C) &&
		(g_sys_stt.v_12v <= M_POW_12V_RAIL_MAX_V_C))
	{
		return 1;
	}

	return 0;
}

/**
 * @brief Checks if 24V power rail is the accepted range
 * 
 * @return uint8_t 0: false, 1: true
 */
static inline uint8_t mod_pow_is_24v_rail_in_range(void)
{
	if ((g_sys_stt.v_24v >= M_POW_24V_RAIL_MIN_V_C) && 
		(g_sys_stt.v_24v <= M_POW_24V_RAIL_MAX_V_C))
	{
		return 1;
	}

	return 0;
}

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
	mod_pow_v_src_adc_monitor();

	M_POW_DBG_PRINT_TXT("Board detection: 24V sense = %.2fV (threshold = %.1fV)",
		   				g_sys_stt.v_24v, M_POW_24V_PASSIVE_THRESHOLD_C);

	if (g_sys_stt.v_24v > M_POW_24V_PASSIVE_THRESHOLD_C)
	{
		g_sys_stt.hw_is_1_2 = true;
		M_POW_DBG_PRINT_OK("Board detected: V1.2 (VSYS on 24V sense)");
	}
	else
	{
		g_sys_stt.hw_is_1_2 = false;
		M_POW_DBG_PRINT_OK("Board detected: V1.1 (no voltage on 24V sense)");
	}
}

/**
 * @brief Monitor power rails voltages
 * 
 */
static void mod_pow_monitor(void)
{
#ifdef DEBUG_BUILD
	static uint8_t warning_flg = false;
#endif

	if (!g_sys_stt.is_pwr_starting_up)
	{
		if (g_sys_stt.is_12v_rail_on && mod_pow_is_12v_rail_in_range() && 
			g_sys_stt.is_24v_rail_on && mod_pow_is_24v_rail_in_range())
		{
			g_sys_stt.is_power_ok = true;

#ifdef DEBUG_BUILD
			warning_flg = false;
#endif
		}
		else
		{
			g_sys_stt.is_power_ok = false;

#ifdef DEBUG_BUILD
			if (!warning_flg)
			{
				warning_flg = true;

				M_POW_DBG_PRINT_WRN("Power fail detected: 24V rail: %s (%.2fV %s range), 12V rail: %s (%.2fV %s range)",
									g_sys_stt.is_24v_rail_on ? "On":"Off",
									g_sys_stt.v_24v,
									mod_pow_is_24v_rail_in_range() ? "In":"Out of",
									g_sys_stt.is_12v_rail_on ? "On":"Off",
									g_sys_stt.v_12v,
									mod_pow_is_12v_rail_in_range() ? "In":"Out of");
			}
#endif
		}
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
			M_POW_DBG_PRINT_TXT("USB-C hot-plug detected, configuring safe PDOs");

			if (g_sys_stt.hw_is_1_2)
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
			M_POW_DBG_PRINT_TXT("USB-C disconnected");

			mod_pow_last_usb_conn_stt    = M_POW_USB_UNPLUGGED_C;
			g_mod_pow_is_usb_connected_b = false;
		}
	}
}

/**
 * @brief Voltage rails power up handler
 * 
 */
static void mod_pow_rails_ctrl_handler(void)
{
	static uint8_t stt_mchn = 0;

	enum {
		PWR_RAILS_STBY_C = 0,
		PWR_RAILS_REV1_1_ON_C,
		PWR_RAILS_REV1_1_OFF_C,
		PWR_RAILS_REV1_2_ON_C,
		PWR_RAILS_REV1_2_OFF_C
	};

	switch (stt_mchn)
	{
		case PWR_RAILS_STBY_C:
			if (g_sys_ctl.task.rails_on) 										// Powering up voltage rails is required ?
			{
				M_POW_DBG_PRINT_TXT("Pre-enable: VBUS=%.2f 12V=%.2f 24V=%.2f",
		   				 			g_sys_stt.v_vbus, g_sys_stt.v_12v, g_sys_stt.v_24v);

				if (g_sys_stt.hw_is_1_2)
				{
					if ((g_sys_stt.v_24v < 7.0) || (g_sys_stt.v_24v > 27.0))
					{
						M_POW_DBG_PRINT_ERR("FAIL: 24V pre-check out of range (%.2fV)", g_sys_stt.v_24v);

						g_sys_ctl.task.rails_on = 0;

						g_sys_stt.is_pwr_starting_up = false;
					}
					else 
					{
						g_sys_stt.is_rails_powering_on  = 1;
						g_sys_stt.is_rails_powering_off = 0;

						stt_mchn = PWR_RAILS_REV1_2_ON_C;
					}
				}
				else
				{
					if ((g_sys_stt.v_12v < 11.5) || (g_sys_stt.v_12v > 12.5))
					{
						M_POW_DBG_PRINT_ERR("FAIL: 12V pre-check out of range (%.2fV)", g_sys_stt.v_12v);
						
						g_sys_ctl.task.rails_on = 0;

						g_sys_stt.is_pwr_starting_up = false;
					}
					else 
					{
						g_sys_stt.is_rails_powering_on = 1;
						g_sys_stt.is_rails_powering_off = 0;

						stt_mchn = PWR_RAILS_REV1_1_ON_C;
					}
				}
			}
			else if (g_sys_ctl.task.rails_off)									// Powering down voltage rails is required ?
			{
				g_sys_stt.is_rails_powering_on  = 0;
				g_sys_stt.is_rails_powering_off = 1;

				if (g_sys_stt.hw_is_1_2)
				{
					stt_mchn = PWR_RAILS_REV1_2_OFF_C;
				}
				else
				{
					stt_mchn = PWR_RAILS_REV1_1_OFF_C;
				}
			}
		break;

		case PWR_RAILS_REV1_1_ON_C:
			g_sys_stt.is_rails_powering_on  = 1;
			g_sys_stt.is_rails_powering_off = 0;

			switch (mod_pow_rails_on_v1_1_handler())
			{
				case -1: // ERROR
					g_sys_stt.task.rails_on = 0;
					g_sys_ctl.task.rails_on = 0;

					g_sys_stt.is_rails_powering_on  = 0;
					g_sys_stt.is_rails_powering_off = 1;

					g_sys_stt.is_pwr_starting_up = false;

					stt_mchn = PWR_RAILS_REV1_1_OFF_C;
				break;

				case 0:
				break;

				case 1:
					g_sys_stt.task.rails_off = 0;
					g_sys_stt.task.rails_on  = 1;
					g_sys_ctl.task.rails_on  = 0;

					g_sys_stt.is_rails_powering_on  = 0;
					g_sys_stt.is_rails_powering_off = 0;

					g_sys_stt.is_pwr_starting_up = false;

					mod_pow_v_src_adc_monitor();

					M_POW_DBG_PRINT_TXT("After rails: VBUS=%.2f 12V=%.2f 24V=%.2f",
		   								 g_sys_stt.v_vbus, g_sys_stt.v_12v, g_sys_stt.v_24v);

					stt_mchn = PWR_RAILS_STBY_C;
				break;

				default:
				break;
			}
		break;

		case PWR_RAILS_REV1_1_OFF_C:
			g_sys_stt.is_rails_powering_on  = 0;
			g_sys_stt.is_rails_powering_off = 1;

			switch (mod_pow_rails_off_v1_1_handler())
			{
				case -1: // ERROR
					g_sys_stt.task.rails_on  = 0;
					g_sys_stt.task.rails_off = 1;
					g_sys_ctl.task.rails_off = 0;

					g_sys_stt.is_rails_powering_on  = 0;
					g_sys_stt.is_rails_powering_off = 0;

					stt_mchn = PWR_RAILS_STBY_C;
				break;

				case 0:
				break;

				case 1:
					g_sys_stt.task.rails_on  = 0;
					g_sys_stt.task.rails_off = 1;
					g_sys_ctl.task.rails_off = 0;

					M_POW_DBG_PRINT_TXT("Voltage rails are off");

					stt_mchn = PWR_RAILS_STBY_C;
				break;

				default:
				break;
			}
		break;

		case PWR_RAILS_REV1_2_ON_C:
			g_sys_stt.is_rails_powering_on  = 1;
			g_sys_stt.is_rails_powering_off = 0;

			switch (mod_pow_rails_on_v1_2_handler())
			{
				case -1: // ERROR
					g_sys_stt.task.rails_off = 1;
					g_sys_stt.task.rails_on  = 0;
					g_sys_ctl.task.rails_on  = 0;

					g_sys_stt.is_rails_powering_on  = 0;
					g_sys_stt.is_rails_powering_off = 1;

					g_sys_stt.is_pwr_starting_up = false;

					stt_mchn = PWR_RAILS_REV1_2_OFF_C;
				break;

				case 0:
				break;

				case 1:
					g_sys_stt.task.rails_off = 0;
					g_sys_stt.task.rails_on  = 1;
					g_sys_ctl.task.rails_on  = 0;

					g_sys_stt.is_rails_powering_on  = 0;
					g_sys_stt.is_rails_powering_off = 0;

					g_sys_stt.is_pwr_starting_up = false;

					mod_pow_v_src_adc_monitor();

					M_POW_DBG_PRINT_TXT("After rails: VBUS=%.2f 12V=%.2f 24V=%.2f",
		   								 g_sys_stt.v_vbus, g_sys_stt.v_12v, g_sys_stt.v_24v);

					stt_mchn = PWR_RAILS_STBY_C;
				break;

				default:
				break;
			}
		break;

		case PWR_RAILS_REV1_2_OFF_C:
			g_sys_stt.is_rails_powering_on  = 0;
			g_sys_stt.is_rails_powering_off = 1;

			switch (mod_pow_rails_off_v1_2_handler())
			{
				case -1: // ERROR
					g_sys_stt.task.rails_on  = 0;
					g_sys_stt.task.rails_off = 1;
					g_sys_ctl.task.rails_off = 0;

					g_sys_stt.is_rails_powering_on  = 0;
					g_sys_stt.is_rails_powering_off = 0;

					stt_mchn = PWR_RAILS_STBY_C;
				break;

				case 0:
				break;

				case 1:
					g_sys_stt.task.rails_on  = 0;
					g_sys_stt.task.rails_off = 1;
					g_sys_ctl.task.rails_off = 0;

					g_sys_stt.is_rails_powering_on  = 0;
					g_sys_stt.is_rails_powering_off = 0;

					M_POW_DBG_PRINT_TXT("Voltage rails are off");

					stt_mchn = PWR_RAILS_STBY_C;
				break;

				default:
				break;
			}
		break;

		default:
			stt_mchn = PWR_RAILS_STBY_C;
		break;
	}
}

/**
 * @brief Voltage rails power-up handler for hardware revision 1.1
 * 
 * @note V1.1: 12V from barrel/USB, then 24V boost from 12V
 * 
 * @return int8_t  0: in progress, 1: finished, -1: error
 */
static int8_t mod_pow_rails_on_v1_1_handler(void)
{
	static uint8_t stt_mchn = 0;

	switch (stt_mchn)
	{
		case 0:
			if (mod_pow_enable_12v_rail())
			{
				g_sys_stt.is_12v_rail_on = 1;

				mod_pow_rail_delay_tmout = make_timeout_time_ms(500);

				stt_mchn++;
			}
		break;

		case 1:
			if (get_absolute_time() >= mod_pow_rail_delay_tmout) 				// Is delay finished ?
			{
				drv_power_set_switched_24v(true);

				g_sys_stt.is_24v_rail_on = 1;

				mod_pow_rail_delay_tmout = make_timeout_time_ms(500);

				stt_mchn++;
			}
		break;

		case 2:
			if (get_absolute_time() >= mod_pow_rail_delay_tmout) 				// Is delay finished ?
			{
				stt_mchn = 0;

				return 1;
			}
		break;

		default:
			stt_mchn = 0;

			return -1;
		break;
	}

	return 0;
}

/**
 * @brief Voltage rails power-down handler for hardware revision 1.1
 * 
 * @note V1.1: 24V off first (depends on 12V), then 12V
 * 
 * @return int8_t  0: in progress, 1: finished, -1: error 
 */
static int8_t mod_pow_rails_off_v1_1_handler(void)
{
	static uint8_t stt_mchn = 0;

	switch (stt_mchn)
	{
		case 0:
			drv_power_set_switched_24v(false);

			g_sys_stt.is_24v_rail_on = 0;

			stt_mchn++;
		break;

		case 1:
			if (mod_pow_disable_12v_rail())
			{
				g_sys_stt.is_12v_rail_on = 0;

				stt_mchn = 0;

				return 1;
			}
		break;

		default:
			stt_mchn = 0;

			return -1;
		break;
	}

	return 0;
}

/**
 * @brief Voltage rails power-up handler for hardware revision 1.2
 * 
 * @note V1.2: 24V boost from VSYS first, then 12V buck from 24V
 * 
 * @return int8_t  0: in progress, 1: finished, -1: error
 */
static int8_t mod_pow_rails_on_v1_2_handler(void)
{
	static uint8_t stt_mchn = 0;

	switch (stt_mchn)
	{
		case 0:
			drv_power_set_switched_24v(true);

			mod_pow_rail_delay_tmout = make_timeout_time_ms(200);

			stt_mchn++;
		break;

		case 1:
			if (get_absolute_time() >= mod_pow_rail_delay_tmout) 				// Is delay finished ?
			{
				M_POW_DBG_PRINT_TXT("24V post-enable: g_sys_stt.v_24v = %.2fV", 
									g_sys_stt.v_24v);

				if (mod_pow_is_24v_rail_in_range())
				{
					g_sys_stt.is_24v_rail_on = 1;

					stt_mchn++;
				}
				else
				{
					M_POW_DBG_PRINT_ERR("FAIL: 24V out of range (%.2fV) — incompatible power supply",
										g_sys_stt.v_24v);

					drv_power_set_switched_24v(false);

					g_sys_stt.is_24v_rail_on = 0;

					stt_mchn = 0;

					return -1; // ERROR
				}
			}
		break;

		case 2:
			if (mod_pow_enable_12v_rail())
			{
				mod_pow_rail_delay_tmout = make_timeout_time_ms(50);

				stt_mchn++;
			}
		break;

		case 3:
			if (get_absolute_time() >= mod_pow_rail_delay_tmout) 				// Is delay finished ?
			{
				M_POW_DBG_PRINT_TXT("12V post-enable: g_sys_stt.v_12v = %.2fV", 
									g_sys_stt.v_12v);

				if (mod_pow_is_12v_rail_in_range())
				{
					g_sys_stt.is_12v_rail_on = 1;

					stt_mchn = 0;

					return 1;
				}
				else
				{
					g_sys_stt.is_12v_rail_on = 0;

					M_POW_DBG_PRINT_ERR("FAIL: 12V out of range (%.2fV) after enable", g_sys_stt.v_12v);

					stt_mchn++;
				}
			}
		break;

		case 4:
			if (mod_pow_disable_12v_rail())
			{
				g_sys_stt.is_12v_rail_on = 0;

				drv_power_set_switched_24v(false);

				g_sys_stt.is_24v_rail_on = 0;

				stt_mchn = 0;

				return -1;
			}
		break;

		default:
			stt_mchn = 0;

			return -1;
		break;
	}

	return 0;
}

/**
 * @brief Voltage rails power-down handler for hardware revision 1.2
 * 
 * @note V1.2: 12V off first (depends on 24V), then 24V
 * 
 * @return int8_t  0: in progress, 1: finished 
 */
static int8_t mod_pow_rails_off_v1_2_handler(void)
{
	if (mod_pow_disable_12v_rail())
	{
		g_sys_stt.is_12v_rail_on = 0;

		drv_power_set_switched_24v(false);

		g_sys_stt.is_24v_rail_on = 0;

		return 1;
	}

	return 0;
}

/**
 * @brief Enables Switched 12V rail
 *
 * @note Ramps PWM down.
 * *
 * @return 	int8_t  0: in progress, 1: finished
 */
static int8_t mod_pow_enable_12v_rail(void)
{
	static uint8_t  stt_mchn = 0;
	static uint16_t level    = 0;

	switch (stt_mchn)
	{
		case 0:
			if (level <= (M_POW_RAILS_STEPCOUNT_SOFTSTART_C + 1))
			{
				drv_power_set_switched_12v_level(level++);

				mod_pow_rail_delay_tmout = make_timeout_time_ms(8);

				stt_mchn++;
			}
		break;

		case 1:
			if (get_absolute_time() >= mod_pow_rail_delay_tmout) 				// Is delay finished ?
			{
				stt_mchn = 0;
				if (level > (M_POW_RAILS_STEPCOUNT_SOFTSTART_C + 1))
				{
					level = 0;

					return 1;
				}
			}
		break;

		default:
			level = 0;

			stt_mchn = 0;
		break;
	}

	return 0;
}

/**
 * @brief Disables Switched 12V rail
 *
 * @note Ramps PWM down.
 * 
 * @return 	uint8_t  0: in progress, 1: finished
 */
static int8_t mod_pow_disable_12v_rail(void)
{
	static uint8_t  stt_mchn = 0;
	static uint16_t level    = M_POW_RAILS_STEPCOUNT_SOFTSTART_C + 1;

	switch (stt_mchn)
	{
		case 0:
			if (level > 0) 
			{
				drv_power_set_switched_12v_level(level--);

				mod_pow_rail_delay_tmout = make_timeout_time_ms(1);

				stt_mchn++;
			}
		break;

		case 1:
			if (get_absolute_time() >= mod_pow_rail_delay_tmout) 				// Is delay finished ?
			{
				stt_mchn = 0;
				if (level == 0) 
				{
					level = M_POW_RAILS_STEPCOUNT_SOFTSTART_C + 1;

					return 1;
				}
			}
		break;

		default:
			level = M_POW_RAILS_STEPCOUNT_SOFTSTART_C + 1;

			stt_mchn = 0;
		break;
	}

	return 0;
}


/*** END OF FILE ***/