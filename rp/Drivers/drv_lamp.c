/**
 * @file      drv_lamp.c
 * @author    The OSLUV Project
 * @brief     Driver for lamp control
 * @schematic lamp_controller.SchDoc
 * @schematic power.SchDoc
 *  
 */


/* Includes ------------------------------------------------------------------*/

#include <stdio.h>
#include <pico/stdlib.h>
#include "pico/time.h"
#include <hardware/gpio.h>
#include <hardware/pwm.h>
#include "Drivers/drv_lamp.h"
#include "Drivers/drv_adc_volt.h"
#include "Drivers/drv_config.h"
#include "Drivers/drv_debug.h"
#include "Drivers/drv_usb_pd.h"
#include "Drivers/drv_radar.h"

#include <hardware/watchdog.h>


/* Private typedef -----------------------------------------------------------*/

typedef struct {
	int pwm;
	int power;
} D_LAMP_PWR_CTL_T;


/* Private define ------------------------------------------------------------*/

#define D_LAMP_DBG_ID_STR_C        	  		"drv_lamp            "
#define D_LAMP_DBG_PRINTF(...)    	  		debug_print_f(__VA_ARGS__)
#define D_LAMP_DBG_PRINT_TXT(...)    		debug_print_mod_f(D_LAMP_DBG_ID_STR_C, __VA_ARGS__)
#define D_LAMP_DBG_PRINT_ERR(...)			debug_print_err(D_LAMP_DBG_ID_STR_C, __VA_ARGS__)
#define D_LAMP_DBG_PRINT_WRN(...)			debug_print_warn(D_LAMP_DBG_ID_STR_C, __VA_ARGS__)
#define D_LAMP_DBG_PRINT_OK(...)			debug_print_ok(D_LAMP_DBG_ID_STR_C, __VA_ARGS__)

#define PIN_ENABLE_12V 						7	/* +12V_ENABLE */
#define PIN_ENABLE_24V 						15	/* +24V_ENABLE */
#define D_LAMP_ENABLE_PIN_C 				14 									/* D_LAMP_ENABLE */
#define D_LAMP_STATUS_PIN_C 				12 									/* D_LAMP_STATUS */
#define D_LAMP_PWM_PIN_C 					13 									/* LAMP_PWM */

#define D_LAMP_RESTRIKE_COOLDOWN_MS_TIME_C 	5000
#define D_LAMP_START_MS_TIME_C 				10000


/* Global variables  ---------------------------------------------------------*/

extern bool g_mod_pow_hw_is_rev1_2_b;


/* Private variables  --------------------------------------------------------*/

const int 				D_LAMP_STEPCOUNT_SOFTSTART_C = 64;
const int 				D_LAMP_STEPCOUNT_DIMMING_C   = 100;

const D_LAMP_PWR_CTL_T 	lamp_pwr_settings[D_LAMP_PWR_MAX_SETTINGS_C] = {
							[D_LAMP_PWR_OFF_C]    = {0,     0},
							[D_LAMP_PWR_20PCT_C]  = {100,  20},
							[D_LAMP_PWR_40PCT_C]  = {83,   40},
							[D_LAMP_PWR_70PCT_C]  = {50,   70},
							[D_LAMP_PWR_100PCT_C] = {0,   100},
						};

static bool 			b_lamp_is_12v_on = false;
static bool 			b_lamp_is_24v_on = false;

static D_LAMP_TYPE_E  	lamp_current_type = D_LAMP_TYPE_UNKNOWN_C;
static D_LAMP_STATE_E 	lamp_state 		  = D_LAMP_STATE_OFF_C;

static D_LAMP_PWR_LEVEL_E lamp_requested_power_level = D_LAMP_PWR_OFF_C;
static D_LAMP_PWR_LEVEL_E lamp_commanded_power_level = D_LAMP_PWR_OFF_C;
static D_LAMP_PWR_LEVEL_E lamp_reported_power_level  = D_LAMP_PWR_UNKNOWN_C;

static uint64_t 		lamp_state_transition_time = 0;

static uint64_t 		lamp_last_update = 0;
static int 				lamp_latched_freq_hz  = 0;

static absolute_time_t  drv_lamp_tmout;
static absolute_time_t  drv_lamp_delay_tmout;

volatile int 			lamp_status_events  = 0;


/* Callback prototypes -------------------------------------------------------*/

void lamp_status_gpio_callback(uint gpio, uint32_t events);


/* Private function prototypes -----------------------------------------------*/

static inline void lamp_go_to_state(D_LAMP_STATE_E state);
static void lamp_perform_type_test_inner(void);
static bool lamp_12v_in_range(void);
static bool lamp_24v_in_range(void);


/* Board-specific helpers ----------------------------------------------------*/

/**
 * @brief Shut down both rails in the correct order for this board.
 */
static void lamp_shutdown_rails(void)
{
	if (g_mod_pow_hw_is_rev1_2_b)
	{
		// V1.2: 12V off first (depends on 24V), then 24V
		drv_lamp_set_switched_12v(false);
		drv_lamp_set_switched_24v(false);
	}
	else
	{
		// V1.1: 24V off first (depends on 12V), then 12V
		drv_lamp_set_switched_24v(false);
		drv_lamp_set_switched_12v(false);
	}
}

/**
 * @brief Power-up both rails in the correct order for this board.
 *
 * V1.1: drv_adc_volt_update, pre-check 12V in range, enable 12V, enable 24V, verify.
 * V1.2: enable 24V, post-check 24V, enable 12V, post-check 12V, verify.
 *
 * On success both rails are on and verified. On failure rails are left off.
 *
 * @return 	void
 */
void drv_lamp_power_up_rails(void)
{
	sleep_ms(250);
	drv_adc_volt_update();
	watchdog_update();
	D_LAMP_DBG_PRINT_TXT("Pre-enable: VBUS=%.2f 12V=%.2f 24V=%.2f",
		   				 g_adc_v_vbus, g_adc_v_12v, g_adc_v_24v);

	if (g_mod_pow_hw_is_rev1_2_b)
	{
		if ((g_adc_v_24v < 7.0) || (g_adc_v_24v > 27.0))
		{
			D_LAMP_DBG_PRINT_ERR("FAIL: 24V pre-check out of range (%.2fV)", g_adc_v_24v);
			return;
		}
		
		// V1.2: 24V boost from VSYS first, then 12V buck from 24V
		drv_lamp_set_switched_24v(true);
		sleep_ms(200);
		drv_adc_volt_update();
		watchdog_update();
		D_LAMP_DBG_PRINT_TXT("24V post-enable: g_adc_v_24v=%.2f", g_adc_v_24v);
		if ((g_adc_v_24v < 21.0) || (g_adc_v_24v > 27.0))
		{
			D_LAMP_DBG_PRINT_ERR("FAIL: 24V out of range (%.2fV) — incompatible power supply",
				   				 g_adc_v_24v);
			drv_lamp_set_switched_24v(false);
			return;
		}

		drv_lamp_set_switched_12v(true);
		sleep_ms(50);
		drv_adc_volt_update();
		watchdog_update();
		D_LAMP_DBG_PRINT_TXT("12V post-enable: g_adc_v_12v=%.2f", g_adc_v_12v);
		if ((g_adc_v_12v < 10.8) || (g_adc_v_12v > 13.8))
		{
			D_LAMP_DBG_PRINT_ERR("FAIL: 12V out of range (%.2fV) after enable", g_adc_v_12v);
			drv_lamp_set_switched_12v(false);
			drv_lamp_set_switched_24v(false);
			return;
		}
	}
	else
	{
		// V1.1: 12V from barrel/USB, then 24V boost from 12V
		if ((g_adc_v_12v < 11.5) || (g_adc_v_12v > 12.5))
		{
			D_LAMP_DBG_PRINT_ERR("FAIL: 12V pre-check out of range (%.2fV)", g_adc_v_12v);
			return;
		}

		drv_lamp_set_switched_12v(true);
		sleep_ms(500);
		drv_adc_volt_update();
		watchdog_update();

		drv_lamp_set_switched_24v(true);
		sleep_ms(500);
		drv_adc_volt_update();
		watchdog_update();
	}

	D_LAMP_DBG_PRINT_TXT("After rails: VBUS=%.2f 12V=%.2f 24V=%.2f",
		   				 g_adc_v_vbus, g_adc_v_12v, g_adc_v_24v);
}


/* Exported functions --------------------------------------------------------*/

/**
 * @brief 	Lamp control initialization procedure
 * 
 * @return 	void 
 */

void drv_lamp_init(void)
{
	uint slice_num;
	pwm_config pwm_cfg;

	gpio_init(PIN_ENABLE_24V);
	gpio_set_dir(PIN_ENABLE_24V, GPIO_OUT);
	gpio_put(PIN_ENABLE_24V, false);

	gpio_init(D_LAMP_ENABLE_PIN_C);
	gpio_set_dir(D_LAMP_ENABLE_PIN_C, GPIO_OUT);
	gpio_put(D_LAMP_ENABLE_PIN_C, false);

	gpio_init(D_LAMP_STATUS_PIN_C);
	gpio_set_dir(D_LAMP_STATUS_PIN_C, GPIO_IN);
	gpio_set_pulls(D_LAMP_STATUS_PIN_C, true, false);

	gpio_set_irq_callback(lamp_status_gpio_callback);
    gpio_set_irq_enabled(D_LAMP_STATUS_PIN_C, GPIO_IRQ_EDGE_RISE, true);
    irq_set_enabled(IO_IRQ_BANK0, true);

	
	gpio_set_function(PIN_ENABLE_12V, GPIO_FUNC_PWM);
	slice_num = pwm_gpio_to_slice_num(PIN_ENABLE_12V);

	pwm_cfg = pwm_get_default_config();
	// Set divider, reduces counter clock to sysclock/this value
	pwm_config_set_clkdiv(&pwm_cfg, 8);

	pwm_config_set_wrap(&pwm_cfg, D_LAMP_STEPCOUNT_SOFTSTART_C); // ~244kHz

	// Load the configuration into our PWM slice, and set it running.
	pwm_init(slice_num, &pwm_cfg, false);

	pwm_set_gpio_level(PIN_ENABLE_12V, 0);

	pwm_set_enabled(slice_num, true);


	gpio_set_function(D_LAMP_PWM_PIN_C, GPIO_FUNC_PWM);
	slice_num = pwm_gpio_to_slice_num(D_LAMP_PWM_PIN_C);

	pwm_cfg = pwm_get_default_config();
	// Set divider, reduces counter clock to sysclock/this value
	pwm_config_set_clkdiv(&pwm_cfg, 8);

	pwm_config_set_wrap(&pwm_cfg, D_LAMP_STEPCOUNT_DIMMING_C - 1); // 244kHz

	// Load the configuration into our PWM slice, and set it running.
	pwm_init(slice_num, &pwm_cfg, false);

	pwm_set_gpio_level(D_LAMP_PWM_PIN_C, 0);

	pwm_set_enabled(slice_num, true);
}

/**
 * @brief 	Lamp state update procedure
 * 
 * @return 	void 
 */
void drv_lamp_update(void)
{
	uint64_t now = time_us_64();

	if ((now - lamp_last_update) > (1000*1000))
	{
		lamp_latched_freq_hz = lamp_status_events;
		lamp_status_events = 0;
		lamp_last_update = now;

		lamp_reported_power_level = D_LAMP_PWR_UNKNOWN_C;

		if (lamp_current_type == D_LAMP_TYPE_NON_DIMMABLE_C)
		{
			lamp_reported_power_level = (!gpio_get(D_LAMP_STATUS_PIN_C)) ? D_LAMP_PWR_100PCT_C : D_LAMP_PWR_OFF_C;
		}
		else // Includes unknown case because this is used while testing
		{
			if (lamp_commanded_power_level == D_LAMP_PWR_OFF_C) 
			{
				lamp_reported_power_level = D_LAMP_PWR_OFF_C;
			}
			else if (lamp_latched_freq_hz < 100)
			{
				if (lamp_commanded_power_level != D_LAMP_PWR_OFF_C && (!gpio_get(D_LAMP_STATUS_PIN_C))) 
				{
					lamp_reported_power_level = D_LAMP_PWR_100PCT_C;
				}
				else if (gpio_get(D_LAMP_STATUS_PIN_C)) 
				{
					lamp_reported_power_level = D_LAMP_PWR_OFF_C;
				}
			}
			else if (lamp_latched_freq_hz > 900 && lamp_latched_freq_hz < 1100) 
			{
				lamp_reported_power_level = D_LAMP_PWR_70PCT_C;
			}
			else if (lamp_latched_freq_hz > 400 && lamp_latched_freq_hz < 600)
			{
				lamp_reported_power_level = D_LAMP_PWR_40PCT_C;
			}
			else if (lamp_latched_freq_hz > 150 && lamp_latched_freq_hz < 250) 
			{
				lamp_reported_power_level = D_LAMP_PWR_20PCT_C;
			}
		}
	}

	uint64_t elapsed_ms_in_state = (time_us_64() - lamp_state_transition_time) / 1000;

	switch (lamp_state)
	{
		case D_LAMP_STATE_STARTING_C:
			lamp_commanded_power_level = D_LAMP_PWR_100PCT_C;

			if (lamp_reported_power_level == D_LAMP_PWR_100PCT_C)
			{
				lamp_go_to_state(D_LAMP_STATE_RUNNING_C);
			}

			if (elapsed_ms_in_state > D_LAMP_START_MS_TIME_C)
			{
				lamp_go_to_state(D_LAMP_STATE_RESTRIKE_COOLDOWN_1_C);
			}

			if (lamp_requested_power_level == D_LAMP_PWR_OFF_C)
			{
				lamp_go_to_state(D_LAMP_PWR_OFF_C);
			}

			// Don't go to off once starting to avoid short cycling
		break;

		case D_LAMP_STATE_RUNNING_C:
			lamp_commanded_power_level = lamp_requested_power_level;

			if (drv_lamp_get_type() == D_LAMP_TYPE_DIMMABLE_C && elapsed_ms_in_state > (2*60*60*1000))
			{
				D_LAMP_DBG_PRINT_TXT("Initiate full-power test");
				lamp_go_to_state(D_LAMP_STATE_FULLPOWER_TEST_C);
			}

			if (drv_lamp_get_type() == D_LAMP_TYPE_NON_DIMMABLE_C && elapsed_ms_in_state > 1000 & lamp_reported_power_level != D_LAMP_PWR_100PCT_C)
			{
				// Can tell immediately if a non-dimmable lamp has gone out
				lamp_go_to_state(D_LAMP_STATE_RESTRIKE_COOLDOWN_1_C);
			}

			if (lamp_requested_power_level == D_LAMP_PWR_OFF_C)
			{
				lamp_go_to_state(D_LAMP_STATE_OFF_C);
			}
		break;

		case D_LAMP_STATE_FULLPOWER_TEST_C:
			lamp_commanded_power_level = D_LAMP_PWR_100PCT_C;

			if (lamp_reported_power_level == D_LAMP_PWR_100PCT_C)
			{
				D_LAMP_DBG_PRINT_TXT("Got a reported 100%% power, OK");
				lamp_go_to_state(D_LAMP_STATE_RUNNING_C);
			}
			else if (elapsed_ms_in_state > D_LAMP_START_MS_TIME_C)
			{
				D_LAMP_DBG_PRINT_TXT("Timed out for 100%% test");
				lamp_go_to_state(D_LAMP_STATE_RESTRIKE_COOLDOWN_1_C);
			}

			if (lamp_requested_power_level == D_LAMP_PWR_OFF_C)
			{
				lamp_go_to_state(D_LAMP_PWR_OFF_C);
			}

			// Don't go to off while fullpower test -- open question?
		break;

		// WARNING: Don't go OFF in the middle of restrike attempt to avoid short cycling

		case D_LAMP_STATE_RESTRIKE_COOLDOWN_1_C:
			lamp_commanded_power_level = D_LAMP_PWR_OFF_C;
			if (lamp_requested_power_level == D_LAMP_PWR_OFF_C)
			{
				lamp_go_to_state(D_LAMP_STATE_OFF_C);
			}
			if (elapsed_ms_in_state > D_LAMP_RESTRIKE_COOLDOWN_MS_TIME_C)
			{
				D_LAMP_DBG_PRINT_TXT("Going to restrike attempt #1");
				lamp_go_to_state(D_LAMP_STATE_RESTRIKE_ATTEMPT_1_C);
			}
		break;

		case D_LAMP_STATE_RESTRIKE_ATTEMPT_1_C:
			lamp_commanded_power_level = D_LAMP_PWR_100PCT_C;
			if (lamp_reported_power_level == D_LAMP_PWR_100PCT_C)
			{
				D_LAMP_DBG_PRINT_TXT("Restrike succeeded on attempt #1");
				lamp_go_to_state(D_LAMP_STATE_STARTING_C);
			}
			if (elapsed_ms_in_state > D_LAMP_START_MS_TIME_C)
			{
				D_LAMP_DBG_PRINT_TXT("Timed out on restrike attempt #1");
				lamp_go_to_state(D_LAMP_STATE_RESTRIKE_COOLDOWN_2_C);
			}
			if (lamp_requested_power_level == D_LAMP_PWR_OFF_C)
			{
				lamp_go_to_state(D_LAMP_PWR_OFF_C);
			}
		break;

		case D_LAMP_STATE_RESTRIKE_COOLDOWN_2_C:
			lamp_commanded_power_level = D_LAMP_PWR_OFF_C;
			if (lamp_requested_power_level == D_LAMP_PWR_OFF_C)
			{
				lamp_go_to_state(D_LAMP_STATE_OFF_C);
			}
			if (elapsed_ms_in_state > D_LAMP_RESTRIKE_COOLDOWN_MS_TIME_C)
			{
				D_LAMP_DBG_PRINT_TXT("Going to restrike attempt #2");
				lamp_go_to_state(D_LAMP_STATE_RESTRIKE_ATTEMPT_2_C);
			}
		break;

		case D_LAMP_STATE_RESTRIKE_ATTEMPT_2_C:
			lamp_commanded_power_level = D_LAMP_PWR_100PCT_C;
			if (lamp_reported_power_level == D_LAMP_PWR_100PCT_C)
			{
				D_LAMP_DBG_PRINT_TXT("Restrike succeeded on attempt #2");
				lamp_go_to_state(D_LAMP_STATE_STARTING_C);
			}
			if (elapsed_ms_in_state > D_LAMP_START_MS_TIME_C)
			{
				D_LAMP_DBG_PRINT_TXT("Timed out on restrike attempt #2");
				lamp_go_to_state(D_LAMP_STATE_RESTRIKE_COOLDOWN_3_C);
			}
			if (lamp_requested_power_level == D_LAMP_PWR_OFF_C)
			{
				lamp_go_to_state(D_LAMP_PWR_OFF_C);
			}
		break;

		case D_LAMP_STATE_RESTRIKE_COOLDOWN_3_C:
			lamp_commanded_power_level = D_LAMP_PWR_OFF_C;
			if (lamp_requested_power_level == D_LAMP_PWR_OFF_C)
			{
				lamp_go_to_state(D_LAMP_STATE_OFF_C);
			}
			if (elapsed_ms_in_state > D_LAMP_RESTRIKE_COOLDOWN_MS_TIME_C)
			{
				D_LAMP_DBG_PRINT_TXT("Going to restrike attempt #3");
				lamp_go_to_state(D_LAMP_STATE_RESTRIKE_ATTEMPT_3_C);
			}
		break;

		case D_LAMP_STATE_RESTRIKE_ATTEMPT_3_C:
			lamp_commanded_power_level = D_LAMP_PWR_100PCT_C;
			if (lamp_reported_power_level == D_LAMP_PWR_100PCT_C)
			{
				D_LAMP_DBG_PRINT_TXT("Restrike succeeded on attempt #3");
				lamp_go_to_state(D_LAMP_STATE_STARTING_C);
			}
			if (elapsed_ms_in_state > D_LAMP_START_MS_TIME_C)
			{
				D_LAMP_DBG_PRINT_TXT("Timed out on restrike attempt #3");
				lamp_go_to_state(D_LAMP_STATE_FAILED_OFF_C);
			}
			if (lamp_requested_power_level == D_LAMP_PWR_OFF_C)
			{
				lamp_go_to_state(D_LAMP_PWR_OFF_C);
			}
		break;

		case D_LAMP_STATE_FAILED_OFF_C:
			lamp_commanded_power_level = D_LAMP_PWR_OFF_C;

			if (lamp_requested_power_level == D_LAMP_PWR_OFF_C)
			{
				lamp_go_to_state(D_LAMP_STATE_OFF_C);
			}
		break;

		case D_LAMP_STATE_OFF_C:
			lamp_commanded_power_level = D_LAMP_PWR_OFF_C;
		break;

		default:
		break;
	}

	pwm_set_gpio_level(D_LAMP_PWM_PIN_C, lamp_pwr_settings[lamp_commanded_power_level].pwm);
	gpio_put(D_LAMP_ENABLE_PIN_C, lamp_commanded_power_level != D_LAMP_PWR_OFF_C);

	if (b_lamp_is_12v_on && b_lamp_is_24v_on &&
		(!lamp_12v_in_range() || !lamp_24v_in_range()))
	{
		D_LAMP_DBG_PRINT_ERR("FAULT: VBUS=%.2f 12V=%.2f 24V=%.2f — emergency shutdown",
			   				g_adc_v_vbus, g_adc_v_12v, g_adc_v_24v);
		gpio_put(D_LAMP_ENABLE_PIN_C, true);  // Possible intentional discharge
		sleep_ms(10);
		lamp_shutdown_rails();
		lamp_go_to_state(D_LAMP_STATE_OFF_C);
	}
}

/**
 * @brief Loads the preset factory lamp type @ref D_LAMP_TYPE_E
 * 
 * @return 	void  
 * 
 */
void drv_lamp_load_type_from_flash(void)
{
	D_LAMP_DBG_PRINT_TXT("Determined type from flash");

	lamp_current_type = drv_cfg_get_factory_lamp_type();
}

/**
 * @brief Returnd the current lamp type
 * 
 * @return @ref D_LAMP_TYPE_E
 */
D_LAMP_TYPE_E drv_lamp_get_type(void)
{
	return lamp_current_type;
}

#if 0
/**
 * @brief 
 * 
 * @return 	void  
 * 
 */
void drv_lamp_perform_type_test(void)
{
	if (drv_lamp_get_type() == D_LAMP_TYPE_UNKNOWN_C)
	{
		D_LAMP_DBG_PRINT_TXT("Performing lamp type test");
		lamp_perform_type_test_inner();
		D_LAMP_DBG_PRINT_TXT("Done");

		if (drv_lamp_get_type() != D_LAMP_TYPE_UNKNOWN_C)
		{
			D_LAMP_DBG_PRINT_TXT("Writing concluded type");
			drv_cfg_set_factory_lamp_type(drv_lamp_get_type());
			drv_cfg_save();
		}
		else
		{
			D_LAMP_DBG_PRINT_WRN("WARNING: lamp type still UNKNOWN after type test");
		}
	}
}
#else
/**
 * @brief Performs a test on the lamp to get its type
 * 
 * @return int8_t 0: In progress, -1: error, 1: succeed
 */
int8_t drv_lamp_perform_type_test(void)
{
    static uint8_t     stt_mchn = 0;
	D_LAMP_PWR_LEVEL_E reported;
	D_LAMP_STATE_E     state;

	switch (stt_mchn)
	{
		case 0:
			if (drv_lamp_get_type() == D_LAMP_TYPE_UNKNOWN_C)
			{
				D_LAMP_DBG_PRINT_TXT("Performing lamp type test");

				drv_lamp_request_power_level(D_LAMP_PWR_OFF_C);

				//drv_lamp_update();
				//drv_lamp_update();

				drv_lamp_delay_tmout = make_timeout_time_ms(100);

				// TODO: Is this delay required or was for making sure that D_LAMP_PWR_OFF_C state was applied?

				stt_mchn++;
			}
			else
			{
				return 1;
			}
		break;

		case 1:
			if (get_absolute_time() > drv_lamp_delay_tmout)
			{
				D_LAMP_DBG_PRINT_TXT("Type test (dimming-response)");
				D_LAMP_DBG_PRINT_TXT("Requesting 70%%, striking at 100%%...");

				drv_lamp_request_power_level(D_LAMP_PWR_70PCT_C);

				drv_lamp_tmout 		 = make_timeout_time_ms(30 * 1000);
				drv_lamp_delay_tmout = make_timeout_time_ms(10); // TODO: Is this delay required or was for making sure that D_LAMP_PWR_70PCT_C state was applied?

				stt_mchn++;
			}
		break;

		case 2:
			if (get_absolute_time() < drv_lamp_tmout)
			{
				if (get_absolute_time() > drv_lamp_delay_tmout)
				{
					//drv_lamp_update();

					state = drv_lamp_get_lamp_state();

					if ((state == D_LAMP_STATE_FAILED_OFF_C         ) ||
						(state == D_LAMP_STATE_RESTRIKE_COOLDOWN_1_C))
					{
						D_LAMP_DBG_PRINT_ERR("Type test: failed to strike");

						lamp_current_type = D_LAMP_TYPE_UNKNOWN_C;

						stt_mchn = (uint8_t)-1;
					}
					else if (state == D_LAMP_STATE_RUNNING_C)
					{
						stt_mchn++;
					}
				}
			}
			else
			{
				// Timeout
				stt_mchn = (uint8_t)-1;
			}
		break;

		case 3:
			if (drv_lamp_get_lamp_state() != D_LAMP_STATE_RUNNING_C)
			{
				D_LAMP_DBG_PRINT_TXT("Type test: strike timeout");

				lamp_current_type = D_LAMP_TYPE_UNKNOWN_C;

				drv_lamp_request_power_level(D_LAMP_PWR_100PCT_C);

				stt_mchn = (uint8_t)-1;
			}
			else
			{
				// Wait for warmup — ballast ignores DIMMING during warmup
				D_LAMP_DBG_PRINT_TXT("Type test: lamp running, waiting for warmup...");

				if (drv_lamp_is_warming())
				{
					//drv_lamp_update();

					drv_lamp_delay_tmout = make_timeout_time_ms(10);// TODO: Is this delay required ?

					stt_mchn++;
				}
			}
		break;

		case 4:
			if (get_absolute_time() > drv_lamp_delay_tmout)
			{
				//drv_lamp_update();

				if (!drv_lamp_is_warming())
				{
					// Check dimming response — wait for reported == commanded
					D_LAMP_DBG_PRINT_TXT("Type test: warmup done, checking dimming response...");

					drv_lamp_tmout 		 = make_timeout_time_ms(5 * 1000);
					drv_lamp_delay_tmout = make_timeout_time_ms(10);// TODO: Is this delay required or was for making sure that D_LAMP_PWR_100PCT_C state was applied?

					//drv_lamp_update();

					stt_mchn++;
				}
			}
		break;

		case 5:
			if (get_absolute_time() < drv_lamp_tmout)
			{
				if (get_absolute_time() > drv_lamp_delay_tmout)
				{
					drv_lamp_get_reported_power_level(&reported);

					D_LAMP_DBG_PRINT_TXT("Type test: polling freq=%dHz reported=%s",
										 drv_lamp_get_raw_freq(),
										 drv_lamp_get_power_level_string(reported));

					if (reported == drv_lamp_get_commanded_power_level())
					{
						stt_mchn++;
					}
					else
					{
						//drv_lamp_update();

						drv_lamp_delay_tmout = make_timeout_time_ms(10);// TODO: Is this delay required?
					}
				}
			}
			else
			{
				// Timeout
				stt_mchn = (uint8_t)-1;
			}
		break;

		case 6:
			drv_lamp_get_reported_power_level(&reported);
			D_LAMP_DBG_PRINT_TXT("Type test: commanded=%s, reported=%s, freq=%dHz",
				   				 drv_lamp_get_power_level_string(drv_lamp_get_commanded_power_level()),
				   				 drv_lamp_get_power_level_string(reported),
								 drv_lamp_get_raw_freq());

			if (reported == D_LAMP_PWR_70PCT_C)
			{
				D_LAMP_DBG_PRINT_TXT("Determined dimmable (responded to 70%% dimming)");
				lamp_current_type = D_LAMP_TYPE_DIMMABLE_C;
			}
			else if (reported == D_LAMP_PWR_100PCT_C)
			{
				D_LAMP_DBG_PRINT_TXT("Determined non-dimmable (ignored dimming)");
				lamp_current_type = D_LAMP_TYPE_NON_DIMMABLE_C;
				drv_lamp_request_power_level(D_LAMP_PWR_100PCT_C);
			}
			else
			{
				D_LAMP_DBG_PRINT_TXT("Type test inconclusive (reported=%s) — UNKNOWN",
									 drv_lamp_get_power_level_string(reported));
				lamp_current_type = D_LAMP_TYPE_UNKNOWN_C;
				drv_lamp_request_power_level(D_LAMP_PWR_100PCT_C);
			}
			stt_mchn++;
		break;

		default:
			stt_mchn = 0;
			
			if (drv_lamp_get_type() != D_LAMP_TYPE_UNKNOWN_C)
			{
				D_LAMP_DBG_PRINT_TXT("Saving concluded lamp type");

				drv_cfg_set_factory_lamp_type(drv_lamp_get_type());
				drv_cfg_save();

				return 1;
			}
			else
			{
				D_LAMP_DBG_PRINT_WRN("WARNING: lamp type still UNKNOWN after type test");

				return -1;
			}
		break;
	}

	return 0;
}
#endif

/**
 * @brief Resets the lamp type to UNKNOWN and clears persistence.
 *        Allows drv_lamp_perform_type_test() to re-run detection.
 */
void drv_lamp_reset_type(void)
{
	lamp_current_type = D_LAMP_TYPE_UNKNOWN_C;
	drv_cfg_set_factory_lamp_type(D_LAMP_TYPE_UNKNOWN_C);
	drv_cfg_save();

	D_LAMP_DBG_PRINT_TXT("drv_lamp_reset_type: Lamp type reset to UNKNOWN");
}

/**
 * @brief Enables/disables Switched 12V Enable pin
 *
 * Ramps PWM up on enable, ramps down on disable. No precondition checks —
 * callers (board_power_up) are responsible for correct ordering and
 * voltage verification.
 *
 * @param b_on
 *
 * @return 	void
 */
void drv_lamp_set_switched_12v(bool b_on)
{
	if (!b_lamp_is_12v_on && b_on)
	{
		for (int idx = 0; idx <= D_LAMP_STEPCOUNT_SOFTSTART_C + 1; idx++)
		{
			pwm_set_gpio_level(PIN_ENABLE_12V, idx);
			sleep_ms(8);
		}
		b_lamp_is_12v_on = b_on;
	}
	else if (b_lamp_is_12v_on && !b_on)
	{
		for (int idx = D_LAMP_STEPCOUNT_SOFTSTART_C + 1; idx >= 0; idx--)
		{
			pwm_set_gpio_level(PIN_ENABLE_12V, idx);
			sleep_ms(1);
		}
		b_lamp_is_12v_on = b_on;
	}
}

/**
 * @brief Returns whether the Switched 12V is enabled or not
 * 
 * @return true 
 * @return false 
 */
bool drv_lamp_get_switched_12v(void)
{
	return b_lamp_is_12v_on;
}

/**
 * @brief Enables/disables Switched 24V Enable pin
 *
 * Sets GPIO and flag. No precondition checks — callers (board_power_up)
 * are responsible for correct ordering and voltage verification.
 *
 * @param b_on
 *
 * @return 	void
 */
void drv_lamp_set_switched_24v(bool b_on)
{
	gpio_put(PIN_ENABLE_24V, b_on);
	b_lamp_is_24v_on = b_on;
}

/**
 * @brief Returns whether the Switched 24V is enabled or not
 * 
 * @return true 
 * @return false 
 */
bool drv_lamp_get_switched_24v(void)
{
	return b_lamp_is_24v_on;
}

/**
 * @brief Request lamp to set a power level
 * 
 * If requested power level is already satisfied, the process will return true
 * 
 * @param pwr_level @ref D_LAMP_PWR_LEVEL_E
 * @return true 
 * @return false 
 */
bool drv_lamp_request_power_level(D_LAMP_PWR_LEVEL_E pwr_level)
{
	if (lamp_requested_power_level == pwr_level) 
	{
		return true;
	}
	if (lamp_state == D_LAMP_STATE_FAILED_OFF_C) 
	{
		return false; // dead
	}

	bool requested_on = pwr_level != D_LAMP_PWR_OFF_C;

	if (drv_lamp_get_type() == D_LAMP_TYPE_NON_DIMMABLE_C)
	{
		if (pwr_level != D_LAMP_PWR_OFF_C && pwr_level != D_LAMP_PWR_100PCT_C)
		{
			D_LAMP_DBG_PRINT_TXT("Reject dimmed control point for lamp not known to dim");
			return false;
		}
	}

	if (pwr_level != D_LAMP_PWR_OFF_C && (!b_lamp_is_12v_on || !b_lamp_is_24v_on))
	{
		D_LAMP_DBG_PRINT_TXT("Reject turn on lamp without both rails");
		return false;
	}

	if (lamp_requested_power_level == D_LAMP_PWR_OFF_C && pwr_level != D_LAMP_PWR_OFF_C)
	{
		D_LAMP_DBG_PRINT_TXT("Lamp goes to D_LAMP_STATE_STARTING_C");
		lamp_state = D_LAMP_STATE_STARTING_C;
		lamp_state_transition_time = time_us_64();
	}

	lamp_requested_power_level = pwr_level;

	return true;
}

/**
 * @brief Returns the previously requested power level
 * 
 * @return D_LAMP_PWR_LEVEL_E 
 */
D_LAMP_PWR_LEVEL_E drv_lamp_get_requested_power_level()
{
	return lamp_requested_power_level;
}

/**
 * @brief Returns the previously commanded power level
 * 
 * @return D_LAMP_PWR_LEVEL_E 
 */
D_LAMP_PWR_LEVEL_E drv_lamp_get_commanded_power_level()
{
	return lamp_commanded_power_level;
}

/**
 * @brief Returns whether the reported power level is a valid one or not
 * 
 * Returns false if unsure
 * 
 * @param p_pwr_level The current reported power level @ref D_LAMP_PWR_LEVEL_E
 * @return true 
 * @return false 
 */
bool drv_lamp_get_reported_power_level(D_LAMP_PWR_LEVEL_E *p_pwr_level)
{
	*p_pwr_level = lamp_reported_power_level;

	return (lamp_reported_power_level >= D_LAMP_PWR_OFF_C         ) && \
		   (lamp_reported_power_level <  D_LAMP_PWR_MAX_SETTINGS_C);
}

/**
 * @brief 
 * 
 * @return true 
 * @return false 
 */
bool drv_lamp_is_power_ok(void)
{
	return b_lamp_is_12v_on && b_lamp_is_24v_on &&
		   lamp_12v_in_range() && lamp_24v_in_range();
}

/**
 * @brief Return the string ID for a power level
 * 
 * @param pwr_level @ref D_LAMP_PWR_LEVEL_E
 * @return const char* 
 */
const char* drv_lamp_get_power_level_string(D_LAMP_PWR_LEVEL_E pwr_level)
{
	static const char* names[] = {
		[D_LAMP_PWR_OFF_C]     = "OFF",
		[D_LAMP_PWR_20PCT_C]   = "20%",
		[D_LAMP_PWR_40PCT_C]   = "40%",
		[D_LAMP_PWR_70PCT_C]   = "70%",
		[D_LAMP_PWR_100PCT_C]  = "100%",
		[D_LAMP_PWR_UNKNOWN_C] = "??%"
	};

	if (pwr_level <= (sizeof(names)/sizeof(names[0])))
	{
		return names[pwr_level];
	}

	return "!?!";
}

/**
 * @brief Get the lamp latched frequency in Hertz
 * 
 * @return int 
 */
int drv_lamp_get_raw_freq(void)
{
	return lamp_latched_freq_hz;
}

/**
 * @brief Return the current lamp state
 * 
 * @return D_LAMP_STATE_E 
 */
D_LAMP_STATE_E drv_lamp_get_lamp_state(void)
{
	return lamp_state;
}

/**
 * @brief Return the string ID for a lamp state
 * 
 * @param state @ref D_LAMP_STATE_E
 * @return const char* 
 */
const char* drv_lamp_get_lamp_state_str(D_LAMP_STATE_E state)
{
	static const char* names[] = {
		[D_LAMP_STATE_OFF_C] 					= "OFF",
		[D_LAMP_STATE_STARTING_C] 				= "STARTING",
		[D_LAMP_STATE_RUNNING_C] 				= "RUNNING",
		[D_LAMP_STATE_FULLPOWER_TEST_C] 	   	= "FULLPOWER_TEST",
		[D_LAMP_STATE_RESTRIKE_COOLDOWN_1_C]	= "RESTRIKE_COOLDOWN_1",
		[D_LAMP_STATE_RESTRIKE_ATTEMPT_1_C]  	= "RESTRIKE_ATTEMPT_1",
		[D_LAMP_STATE_RESTRIKE_COOLDOWN_2_C] 	= "RESTRIKE_COOLDOWN_2",
		[D_LAMP_STATE_RESTRIKE_ATTEMPT_2_C] 	= "RESTRIKE_ATTEMPT_2",
		[D_LAMP_STATE_RESTRIKE_COOLDOWN_3_C] 	= "RESTRIKE_COOLDOWN_3",
		[D_LAMP_STATE_RESTRIKE_ATTEMPT_3_C] 	= "RESTRIKE_ATTEMPT_3",
		[D_LAMP_STATE_FAILED_OFF_C] 			= "FAILED_OFF",
	};

	if (state <= (sizeof(names)/sizeof(names[0])))
	{
		return names[state];
	}

	return "!?!";
}

/**
 * @brief Returns the state transition time in miliseconds
 * 
 * @return int 
 */
int drv_lamp_get_state_elapsed_ms(void)
{
	return (time_us_64() - lamp_state_transition_time) / 1000;
}

/**
 * @brief Returns whether the lamp is warming or not
 * 
 * @return true 
 * @return false 
 */
bool drv_lamp_is_warming(void)
{
    uint32_t ms = drv_lamp_get_state_elapsed_ms();

    return (lamp_state == D_LAMP_STATE_STARTING_C) ||
           ((lamp_state == D_LAMP_STATE_RUNNING_C) && 
		    (ms < D_LAMP_START_MS_TIME_C));
}


/* Callback functions --------------------------------------------------------*/

/**
 * @brief Callback for lamp status ISR
 * 
 * Counts the status events
 * 
 * @param a_gpio 
 * @param a_events 
 * e
 * @return 	void
 */
void lamp_status_gpio_callback(uint gpio, uint32_t events)
{
	if (gpio == D_LAMP_STATUS_PIN_C)
	{
		lamp_status_events++;
	}
}


/* Private functions ---------------------------------------------------------*/

/**
 * @brief Sets a new lamp state
 * 
 * @param state The new state to set
 * 
 * @related D_LAMP_STATE_E
 * 
 * @return 	void
 */
static inline void lamp_go_to_state(D_LAMP_STATE_E state)
{
	if (state != lamp_state) 
	{
		D_LAMP_DBG_PRINT_TXT("State transition to %s", drv_lamp_get_lamp_state_str(state));
	}
	lamp_state = state;
	lamp_state_transition_time = time_us_64();
}

/**
 * @brief Dimming-response type test (board-agnostic).
 *        Both rails must be on. Request 70% — STARTING forces 100% to strike.
 *        Once RUNNING + warmup, check if ballast responds to dimming:
 *        reported=70% → dimmable, reported=100% → non-dimmable.
 */
static void lamp_perform_type_test_inner(void)
{
#if 1
#else
	lamp_current_type = D_LAMP_TYPE_UNKNOWN_C;
	drv_lamp_request_power_level(D_LAMP_PWR_OFF_C);
	drv_lamp_update();
	drv_lamp_update();
	sleep_ms(100);

	D_LAMP_DBG_PRINT_TXT("Type test (dimming-response)");
	D_LAMP_DBG_PRINT_TXT("Requesting 70%%, striking at 100%%...");
	drv_lamp_request_power_level(D_LAMP_PWR_70PCT_C);

	// Wait for strike (STARTING → RUNNING) with safety timeout
	uint64_t start = time_us_64();
	while ((time_us_64() - start) < (30ULL * 1000 * 1000))
	{
		watchdog_update();
		drv_lamp_update();
		sleep_ms(10);

		D_LAMP_STATE_E state = drv_lamp_get_lamp_state();
		if (state == D_LAMP_STATE_FAILED_OFF_C ||
			state == D_LAMP_STATE_RESTRIKE_COOLDOWN_1_C)
		{
			D_LAMP_DBG_PRINT_TXT("Type test: failed to strike");
			lamp_current_type = D_LAMP_TYPE_UNKNOWN_C;
			return;
		}
		if (state == D_LAMP_STATE_RUNNING_C)
		{
			break;
		}
	}

	if (drv_lamp_get_lamp_state() != D_LAMP_STATE_RUNNING_C)
	{
		D_LAMP_DBG_PRINT_TXT("Type test: strike timeout");
		lamp_current_type = D_LAMP_TYPE_UNKNOWN_C;
		drv_lamp_request_power_level(D_LAMP_PWR_100PCT_C);
		return;
	}

	// Wait for warmup — ballast ignores DIMMING during warmup
	D_LAMP_DBG_PRINT_TXT("Type test: lamp running, waiting for warmup...");
	while (drv_lamp_is_warming())
	{
		watchdog_update();
		drv_lamp_update();
		sleep_ms(10);
	}

	// Check dimming response — wait for reported == commanded
	D_LAMP_DBG_PRINT_TXT("Type test: warmup done, checking dimming response...");

	uint64_t dim_start = time_us_64();
	D_LAMP_PWR_LEVEL_E reported;

	while ((time_us_64() - dim_start) < (5ULL * 1000 * 1000))
	{
		watchdog_update();
		drv_lamp_update();
		sleep_ms(10);

		drv_lamp_get_reported_power_level(&reported);
		D_LAMP_DBG_PRINT_TXT("Type test: polling freq=%dHz reported=%s",
							 drv_lamp_get_raw_freq(),
			   				 drv_lamp_get_power_level_string(reported));
		if (reported == drv_lamp_get_commanded_power_level())
		{
			break;
		}
	}

	drv_lamp_get_reported_power_level(&reported);
	D_LAMP_DBG_PRINT_TXT("Type test: commanded=%s, reported=%s, freq=%dHz",
						 drv_lamp_get_power_level_string(drv_lamp_get_commanded_power_level()),
						 drv_lamp_get_power_level_string(reported),
						 drv_lamp_get_raw_freq());

	if (reported == D_LAMP_PWR_70PCT_C)
	{
		D_LAMP_DBG_PRINT_TXT("Determined dimmable (responded to 70%% dimming)");
		lamp_current_type = D_LAMP_TYPE_DIMMABLE_C;
	}
	else if (reported == D_LAMP_PWR_100PCT_C)
	{
		D_LAMP_DBG_PRINT_TXT("Determined non-dimmable (ignored dimming)");
		lamp_current_type = D_LAMP_TYPE_NON_DIMMABLE_C;
		drv_lamp_request_power_level(D_LAMP_PWR_100PCT_C);
	}
	else
	{
		D_LAMP_DBG_PRINT_TXT("Type test inconclusive (reported=%s) — UNKNOWN",
			   drv_lamp_get_power_level_string(reported));
		lamp_current_type = D_LAMP_TYPE_UNKNOWN_C;
		drv_lamp_request_power_level(D_LAMP_PWR_100PCT_C);
	}
#endif
}

/**
 * @brief Returns whether the 12V rail is within acceptable range (10.5–13.5V)
 */
static bool lamp_12v_in_range(void)
{
	return (g_adc_v_12v >= 10.5) && (g_adc_v_12v <= 13.5);
}

/**
 * @brief Returns whether the 24V rail is within acceptable range (21.0–27.0V)
 */
static bool lamp_24v_in_range(void)
{
	return (g_adc_v_24v >= 21.0) && (g_adc_v_24v <= 27.0);
}

/*** END OF FILE ***/
