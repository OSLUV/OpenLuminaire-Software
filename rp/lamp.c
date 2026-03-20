/**
 * @file      lamp.c
 * @author    The OSLUV Project
 * @brief     Driver for lamp control
 * @schematic lamp_controller.SchDoc
 * @schematic power.SchDoc
 *  
 */


/* Includes ------------------------------------------------------------------*/

#include <stdio.h>
#include <pico/stdlib.h>
#include <hardware/gpio.h>
#include <hardware/pwm.h>

#include "lamp.h"
#include "pins.h"
#include "usbpd.h"
#include "sense.h"
#include "radar.h"
#include "persistance.h"


/* Private typedef -----------------------------------------------------------*/

typedef struct {
	int pwm;
	int power;
} LAMP_PWR_CTL_T;


/* Private define ------------------------------------------------------------*/

#define LAMP_RESTRIKE_COOLDOWN_MS_TIME_C 	5000
#define LAMP_START_MS_TIME_C 				10000


/* Global variables  ---------------------------------------------------------*/


/* Private variables  --------------------------------------------------------*/

const int 				LAMP_STEPCOUNT_SOFTSTART_C = 64;
const int 				LAMP_STEPCOUNT_DIMMING_C   = 100;

const LAMP_PWR_CTL_T 	lamp_pwr_settings[LAMP_PWR_MAX_SETTINGS_C] = {
							[LAMP_PWR_OFF_C]    = {0,     0},
							[LAMP_PWR_20PCT_C]  = {100,  20},
							[LAMP_PWR_40PCT_C]  = {83,   40},
							[LAMP_PWR_70PCT_C]  = {50,   70},
							[LAMP_PWR_100PCT_C] = {0,   100},
						};

static bool 			b_lamp_is_12v_on = false;
static bool 			b_lamp_is_24v_on = false;

static LAMP_TYPE_E  	lamp_current_type = LAMP_TYPE_UNKNOWN_C;
static LAMP_STATE_E 	lamp_state 		  = LAMP_STATE_OFF_C;

static LAMP_PWR_LEVEL_E lamp_requested_power_level = LAMP_PWR_OFF_C;
static LAMP_PWR_LEVEL_E lamp_commanded_power_level = LAMP_PWR_OFF_C;
static LAMP_PWR_LEVEL_E lamp_reported_power_level  = LAMP_PWR_UNKNOWN_C;

static uint64_t 		lamp_state_transition_time = 0;

static uint64_t 		lamp_last_update = 0;
static int 				lamp_latched_freq_hz  = 0;

volatile int 			lamp_status_events  = 0;


/* Callback prototypes -------------------------------------------------------*/

void lamp_status_gpio_callback(uint gpio, uint32_t events);


/* Private function prototypes -----------------------------------------------*/

static inline void lamp_go_to_state(LAMP_STATE_E state);
static void lamp_perform_type_test_inner(void);
static bool lamp_is_test_state_failure(LAMP_STATE_E state);
static bool lamp_power_is_too_low(void);
static bool lamp_power_is_too_high(void);
static bool lamp_usb_power_is_too_low(void);
static bool lamp_usb_power_is_too_high(void);
static bool lamp_jack_power_is_too_high(void);
static bool lamp_jack_power_is_too_low(void);


/* Exported functions --------------------------------------------------------*/

/**
 * @brief 	Lamp control initialization procedure
 * 
 * @return 	void 
 */

void lamp_init(void)
{
	uint slice_num;
	pwm_config pwm_cfg;

	gpio_init(PIN_ENABLE_24V);
	gpio_set_dir(PIN_ENABLE_24V, GPIO_OUT);
	gpio_put(PIN_ENABLE_24V, false);

	gpio_init(PIN_ENABLE_LAMP);
	gpio_set_dir(PIN_ENABLE_LAMP, GPIO_OUT);
	gpio_put(PIN_ENABLE_LAMP, false);

	gpio_init(PIN_STATUS_LAMP);
	gpio_set_dir(PIN_STATUS_LAMP, GPIO_IN);
	gpio_set_pulls(PIN_STATUS_LAMP, true, false);

	gpio_set_irq_callback(lamp_status_gpio_callback);
    gpio_set_irq_enabled(PIN_STATUS_LAMP, GPIO_IRQ_EDGE_RISE, true);
    irq_set_enabled(IO_IRQ_BANK0, true);

	
	gpio_set_function(PIN_ENABLE_12V, GPIO_FUNC_PWM);
	slice_num = pwm_gpio_to_slice_num(PIN_ENABLE_12V);

	pwm_cfg = pwm_get_default_config();
	// Set divider, reduces counter clock to sysclock/this value
	pwm_config_set_clkdiv(&pwm_cfg, 8);

	pwm_config_set_wrap(&pwm_cfg, LAMP_STEPCOUNT_SOFTSTART_C); // ~244kHz

	// Load the configuration into our PWM slice, and set it running.
	pwm_init(slice_num, &pwm_cfg, false);

	pwm_set_gpio_level(PIN_ENABLE_12V, 0);

	pwm_set_enabled(slice_num, true);


	gpio_set_function(PIN_PWM_LAMP, GPIO_FUNC_PWM);
	slice_num = pwm_gpio_to_slice_num(PIN_PWM_LAMP);

	pwm_cfg = pwm_get_default_config();
	// Set divider, reduces counter clock to sysclock/this value
	pwm_config_set_clkdiv(&pwm_cfg, 8);

	pwm_config_set_wrap(&pwm_cfg, LAMP_STEPCOUNT_DIMMING_C-1); // 244kHz

	// Load the configuration into our PWM slice, and set it running.
	pwm_init(slice_num, &pwm_cfg, false);

	pwm_set_gpio_level(PIN_PWM_LAMP, 0);

	pwm_set_enabled(slice_num, true);
}

/**
 * @brief 	Lamp state update procedure
 * 
 * @return 	void 
 */
void lamp_update(void)
{
	uint64_t now = time_us_64();

	if ((now - lamp_last_update) > (1000*1000))
	{
		lamp_latched_freq_hz = lamp_status_events;
		lamp_status_events = 0;
		lamp_last_update = now;

		lamp_reported_power_level = LAMP_PWR_UNKNOWN_C;

		if (lamp_current_type == LAMP_TYPE_NON_DIMMABLE_C)
		{
			lamp_reported_power_level = (!gpio_get(PIN_STATUS_LAMP)) ? LAMP_PWR_100PCT_C : LAMP_PWR_OFF_C;
		}
		else // Includes unknown case because this is used while testing
		{
			if (lamp_commanded_power_level == LAMP_PWR_OFF_C) 
			{
				lamp_reported_power_level = LAMP_PWR_OFF_C;
			}
			else if (lamp_latched_freq_hz < 100)
			{
				if (lamp_commanded_power_level != LAMP_PWR_OFF_C && (!gpio_get(PIN_STATUS_LAMP))) 
				{
					lamp_reported_power_level = LAMP_PWR_100PCT_C;
				}
				else if (gpio_get(PIN_STATUS_LAMP)) 
				{
					lamp_reported_power_level = LAMP_PWR_OFF_C;
				}
			}
			else if (lamp_latched_freq_hz > 900 && lamp_latched_freq_hz < 1100) 
			{
				lamp_reported_power_level = LAMP_PWR_70PCT_C;
			}
			else if (lamp_latched_freq_hz > 400 && lamp_latched_freq_hz < 600)
			{
				lamp_reported_power_level = LAMP_PWR_40PCT_C;
			}
			else if (lamp_latched_freq_hz > 150 && lamp_latched_freq_hz < 250) 
			{
				lamp_reported_power_level = LAMP_PWR_20PCT_C;
			}
		}
	}

	uint64_t elapsed_ms_in_state = (time_us_64() - lamp_state_transition_time) / 1000;

	switch (lamp_state)
	{
		case LAMP_STATE_STARTING_C:
			lamp_commanded_power_level = LAMP_PWR_100PCT_C;

			if (lamp_reported_power_level == LAMP_PWR_100PCT_C)
			{
				lamp_go_to_state(LAMP_STATE_RUNNING_C);
			}

			if (elapsed_ms_in_state > LAMP_START_MS_TIME_C)
			{
				lamp_go_to_state(LAMP_STATE_RESTRIKE_COOLDOWN_1_C);
			}

			if (lamp_requested_power_level == LAMP_PWR_OFF_C)
			{
				lamp_go_to_state(LAMP_PWR_OFF_C);
			}

			// Don't go to off once starting to avoid short cycling
		break;

		case LAMP_STATE_RUNNING_C:
			lamp_commanded_power_level = lamp_requested_power_level;

			if (lamp_get_type() == LAMP_TYPE_DIMMABLE_C && elapsed_ms_in_state > (2*60*60*1000))
			{
				printf("Initiate full-power test\n");
				lamp_go_to_state(LAMP_STATE_FULLPOWER_TEST_C);
			}

			if (lamp_get_type() == LAMP_TYPE_NON_DIMMABLE_C && elapsed_ms_in_state > 1000 & lamp_reported_power_level != LAMP_PWR_100PCT_C)
			{
				// Can tell immediately if a non-dimmable lamp has gone out
				lamp_go_to_state(LAMP_STATE_RESTRIKE_COOLDOWN_1_C);
			}

			if (lamp_requested_power_level == LAMP_PWR_OFF_C)
			{
				lamp_go_to_state(LAMP_STATE_OFF_C);
			}
		break;

		case LAMP_STATE_FULLPOWER_TEST_C:
			lamp_commanded_power_level = LAMP_PWR_100PCT_C;

			if (lamp_reported_power_level == LAMP_PWR_100PCT_C)
			{
				printf("Got a reported 100%% power, OK\n");
				lamp_go_to_state(LAMP_STATE_RUNNING_C);
			}
			else if (elapsed_ms_in_state > LAMP_START_MS_TIME_C)
			{
				printf("Timed out for 100%% test\n");
				lamp_go_to_state(LAMP_STATE_RESTRIKE_COOLDOWN_1_C);
			}

			if (lamp_requested_power_level == LAMP_PWR_OFF_C)
			{
				lamp_go_to_state(LAMP_PWR_OFF_C);
			}

			// Don't go to off while fullpower test -- open question?
		break;

		// WARNING: Don't go OFF in the middle of restrike attempt to avoid short cycling

		case LAMP_STATE_RESTRIKE_COOLDOWN_1_C:
			lamp_commanded_power_level = LAMP_PWR_OFF_C;
			if (lamp_requested_power_level == LAMP_PWR_OFF_C)
			{
				lamp_go_to_state(LAMP_STATE_OFF_C);
			}
			if (elapsed_ms_in_state > LAMP_RESTRIKE_COOLDOWN_MS_TIME_C)
			{
				printf("Going to restrike attempt #1\n");
				lamp_go_to_state(LAMP_STATE_RESTRIKE_ATTEMPT_1_C);
			}
		break;

		case LAMP_STATE_RESTRIKE_ATTEMPT_1_C:
			lamp_commanded_power_level = LAMP_PWR_100PCT_C;
			if (lamp_reported_power_level == LAMP_PWR_100PCT_C)
			{
				printf("Restrike succeeded on attempt #1\n");
				lamp_go_to_state(LAMP_STATE_STARTING_C);
			}
			if (elapsed_ms_in_state > LAMP_START_MS_TIME_C)
			{
				printf("Timed out on restrike attempt #1\n");
				lamp_go_to_state(LAMP_STATE_RESTRIKE_COOLDOWN_2_C);
			}
			if (lamp_requested_power_level == LAMP_PWR_OFF_C)
			{
				lamp_go_to_state(LAMP_PWR_OFF_C);
			}
		break;

		case LAMP_STATE_RESTRIKE_COOLDOWN_2_C:
			lamp_commanded_power_level = LAMP_PWR_OFF_C;
			if (lamp_requested_power_level == LAMP_PWR_OFF_C)
			{
				lamp_go_to_state(LAMP_STATE_OFF_C);
			}
			if (elapsed_ms_in_state > LAMP_RESTRIKE_COOLDOWN_MS_TIME_C)
			{
				printf("Going to restrike attempt #2\n");
				lamp_go_to_state(LAMP_STATE_RESTRIKE_ATTEMPT_2_C);
			}
		break;

		case LAMP_STATE_RESTRIKE_ATTEMPT_2_C:
			lamp_commanded_power_level = LAMP_PWR_100PCT_C;
			if (lamp_reported_power_level == LAMP_PWR_100PCT_C)
			{
				printf("Restrike succeeded on attempt #2\n");
				lamp_go_to_state(LAMP_STATE_STARTING_C);
			}
			if (elapsed_ms_in_state > LAMP_START_MS_TIME_C)
			{
				printf("Timed out on restrike attempt #2\n");
				lamp_go_to_state(LAMP_STATE_RESTRIKE_COOLDOWN_3_C);
			}
			if (lamp_requested_power_level == LAMP_PWR_OFF_C)
			{
				lamp_go_to_state(LAMP_PWR_OFF_C);
			}
		break;

		case LAMP_STATE_RESTRIKE_COOLDOWN_3_C:
			lamp_commanded_power_level = LAMP_PWR_OFF_C;
			if (lamp_requested_power_level == LAMP_PWR_OFF_C)
			{
				lamp_go_to_state(LAMP_STATE_OFF_C);
			}
			if (elapsed_ms_in_state > LAMP_RESTRIKE_COOLDOWN_MS_TIME_C)
			{
				printf("Going to restrike attempt #3\n");
				lamp_go_to_state(LAMP_STATE_RESTRIKE_ATTEMPT_3_C);
			}
		break;

		case LAMP_STATE_RESTRIKE_ATTEMPT_3_C:
			lamp_commanded_power_level = LAMP_PWR_100PCT_C;
			if (lamp_reported_power_level == LAMP_PWR_100PCT_C)
			{
				printf("Restrike succeeded on attempt #3\n");
				lamp_go_to_state(LAMP_STATE_STARTING_C);
			}
			if (elapsed_ms_in_state > LAMP_START_MS_TIME_C)
			{
				printf("Timed out on restrike attempt #3\n");
				lamp_go_to_state(LAMP_STATE_FAILED_OFF_C);
			}
			if (lamp_requested_power_level == LAMP_PWR_OFF_C)
			{
				lamp_go_to_state(LAMP_PWR_OFF_C);
			}
		break;

		case LAMP_STATE_FAILED_OFF_C:
			lamp_commanded_power_level = LAMP_PWR_OFF_C;

			if (lamp_requested_power_level == LAMP_PWR_OFF_C)
			{
				lamp_go_to_state(LAMP_STATE_OFF_C);
			}
		break;

		case LAMP_STATE_OFF_C:
			lamp_commanded_power_level = LAMP_PWR_OFF_C;
		break;

		default:
		break;
	}

	pwm_set_gpio_level(PIN_PWM_LAMP, lamp_pwr_settings[lamp_commanded_power_level].pwm);
	gpio_put(PIN_ENABLE_LAMP, lamp_commanded_power_level != LAMP_PWR_OFF_C);

	// V1.2: Shut down 12V first (depends on 24V), then 24V.
	// Only check 12V fault when the rail is intentionally on — during the type test
	// the buck is deliberately disabled and g_sense_12v ≈ 0V is expected, not a fault.
	// TODO Phase 2: Add 24V range check once thresholds confirmed under load
	if (b_lamp_is_12v_on && ((g_sense_12v < 10.5) || (g_sense_12v > 13.5)))
	{
		printf("[V1.2] FAULT: 12V=%.2f 24V=%.2f — emergency shutdown\n",
			   g_sense_12v, g_sense_24v);
		gpio_put(PIN_ENABLE_LAMP, true);  // Possible intentional discharge — preserved from V1.1
		sleep_ms(10);
		// V1.2: Reverse order — 12V off first (depends on 24V), then 24V
		// V1.1: lamp_set_switched_24v(false);
		// V1.1: lamp_set_switched_12v(false);
		lamp_set_switched_12v(false);
		lamp_set_switched_24v(false);
		lamp_go_to_state(LAMP_STATE_OFF_C);
	}
}

/**
 * @brief Loads the preset factory lamp type @ref LAMP_TYPE_E
 * 
 * @return 	void  
 * 
 */
void lamp_load_type_from_flash(void)
{
	printf("Determined type from flash\n");

	lamp_current_type = g_persistance_region.factory_lamp_type;
}

/**
 * @brief Returnd the current lamp type
 * 
 * @return @ref LAMP_TYPE_E
 */
LAMP_TYPE_E lamp_get_type(void)
{
	return lamp_current_type;
}

/**
 * @brief 
 * 
 * @return 	void  
 * 
 */
void lamp_perform_type_test(void)
{
	if (lamp_get_type() == LAMP_TYPE_UNKNOWN_C)
	{
		printf("Performing lamp type test\n");
		lamp_perform_type_test_inner();
		printf("Done\n");

		if (lamp_get_type() != LAMP_TYPE_UNKNOWN_C)
		{
			printf("Writing concluded type\n");
			persistance_set_factory_lamp_type(lamp_get_type());
			persistance_write_region();
		}
	}
}

/**
 * @brief Resets the lamp type to UNKNOWN and clears persistence.
 *        Allows lamp_perform_type_test() to re-run detection.
 */
void lamp_reset_type(void)
{
	lamp_current_type = LAMP_TYPE_UNKNOWN_C;
	persistance_set_factory_lamp_type(LAMP_TYPE_UNKNOWN_C);
	persistance_write_region();
	printf("Lamp type reset to UNKNOWN\n");
}

/**
 * @brief Enables/disables Switched 12V Enable pin
 *
 * Enabling requires voltage to be in a range 11.5V to 12.5V
 * 
 * @param b_on 
 * 
 * @return 	void
 */
void lamp_set_switched_12v(bool b_on)
{
	// V1.1: if (((g_sense_12v < 11.5) || (g_sense_12v > 12.5)) && b_on)
	// V1.1: {
	// V1.1: 	printf("Reject turn on 12V when 12v not OK\n");
	// V1.1: }

	// V1.2: Can't pre-check 12V voltage — it's generated by the buck converter
	// we're about to enable. Instead, require 24V (the source) to be on first.
	if (!b_lamp_is_24v_on && b_on)
	{
		printf("[V1.2] Reject 12V enable: 24V not on\n");
		return;
	}

	if (!b_lamp_is_12v_on && b_on)
	{
		printf("[V1.2] Soft-starting 12V...\n");
		for (int idx = 0; idx <= LAMP_STEPCOUNT_SOFTSTART_C + 1; idx++)
		{
			pwm_set_gpio_level(PIN_ENABLE_12V, idx);
			sleep_ms(8);
		}

		// V1.2: Post-enable verification (12V now exists)
		sleep_ms(50);
		sense_update();
		printf("[V1.2] 12V post-enable: g_sense_12v=%.2f\n", g_sense_12v);
		if (g_sense_12v < 10.5 || g_sense_12v > 13.5)
		{
			printf("[V1.2] WARNING: 12V out of range after enable! Ramping down.\n");
			for (int idx = LAMP_STEPCOUNT_SOFTSTART_C + 1; idx >= 0; idx--)
			{
				pwm_set_gpio_level(PIN_ENABLE_12V, idx);
				sleep_ms(1);
			}
			// b_lamp_is_12v_on stays false — enable failed
		}
		else
		{
			b_lamp_is_12v_on = b_on;
		}
	}
	else if (b_lamp_is_12v_on && !b_on)
	{
		for (int idx = LAMP_STEPCOUNT_SOFTSTART_C + 1; idx >= 0; idx--)
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
bool lamp_get_switched_12v(void)
{
	return b_lamp_is_12v_on;
}

/**
 * @brief Enables/disables Switched 24V Enable pin
 * 
 * Enabling Switched 24 requires that Switched 12V is previously enabled
 * 
 * @param b_on 
 * 
 * @return 	void
 */
void lamp_set_switched_24v(bool b_on)
{
	// V1.1: if (!b_lamp_is_12v_on && b_on)
	// V1.1: {
	// V1.1: 	printf("Reject turn on 24V when 12V not available\n");
	// V1.1: }

	// V1.2: 24V is independent (boost from VSYS). But 12V depends on 24V,
	// so reject disabling 24V while 12V is still on.
	if (!b_on && b_lamp_is_12v_on)
	{
		printf("[V1.2] Reject 24V disable: 12V still on (depends on 24V)\n");
		return;
	}

	gpio_put(PIN_ENABLE_24V, b_on);
	b_lamp_is_24v_on = b_on;
	printf("[V1.2] 24V %s\n", b_on ? "enabled" : "disabled");
}

/**
 * @brief Returns whether the Switched 24V is enabled or not
 * 
 * @return true 
 * @return false 
 */
bool lamp_get_switched_24v(void)
{
	return b_lamp_is_24v_on;
}

/**
 * @brief Request lamp to set a power level
 * 
 * If requested power level is already satisfied, the process will return true
 * 
 * @param pwr_level @ref LAMP_PWR_LEVEL_E
 * @return true 
 * @return false 
 */
bool lamp_request_power_level(LAMP_PWR_LEVEL_E pwr_level)
{
	if (lamp_requested_power_level == pwr_level) 
	{
		return true;
	}
	if (lamp_state == LAMP_STATE_FAILED_OFF_C) 
	{
		return false; // dead
	}

	bool requested_on = pwr_level != LAMP_PWR_OFF_C;

	if (lamp_get_type() == LAMP_TYPE_NON_DIMMABLE_C)
	{
		if (pwr_level != LAMP_PWR_OFF_C && pwr_level != LAMP_PWR_100PCT_C)
		{
			printf("Reject dimmed control point for lamp not known to dim\n");
			return false;
		}

		// V1.1: if (!b_lamp_is_24v_on && pwr_level != LAMP_PWR_OFF_C)
		// V1.1: {
		// V1.1: 	printf("Reject turn on for non-dimmable lamp without 24V\n");
		// V1.1: 	return false;
		// V1.1: }
	}

	// V1.2: Both rails required for all lamp types (24V is upstream of 12V)
	if (!b_lamp_is_24v_on && pwr_level != LAMP_PWR_OFF_C)
	{
		printf("[V1.2] Reject turn on lamp without 24V\n");
		return false;
	}

	if (!b_lamp_is_12v_on && pwr_level != LAMP_PWR_OFF_C)
	{
		printf("Reject turn on lamp without 12V\n");
		return false;
	}

	if (lamp_requested_power_level == LAMP_PWR_OFF_C && pwr_level != LAMP_PWR_OFF_C)
	{
		printf("Lamp goes to LAMP_STATE_STARTING_C\n");
		lamp_state = LAMP_STATE_STARTING_C;
		lamp_state_transition_time = time_us_64();
	}

	lamp_requested_power_level = pwr_level;

	return true;
}

/**
 * @brief Returns the previously requested power level
 * 
 * @return LAMP_PWR_LEVEL_E 
 */
LAMP_PWR_LEVEL_E lamp_get_requested_power_level()
{
	return lamp_requested_power_level;
}

/**
 * @brief Returns the previously commanded power level
 * 
 * @return LAMP_PWR_LEVEL_E 
 */
LAMP_PWR_LEVEL_E lamp_get_commanded_power_level()
{
	return lamp_commanded_power_level;
}

/**
 * @brief Returns whether the reported power level is a valid one or not
 * 
 * Returns false if unsure
 * 
 * @param p_pwr_level The current reported power level @ref LAMP_PWR_LEVEL_E
 * @return true 
 * @return false 
 */
bool lamp_get_reported_power_level(LAMP_PWR_LEVEL_E *p_pwr_level)
{
	*p_pwr_level = lamp_reported_power_level;

	return (lamp_reported_power_level >= LAMP_PWR_OFF_C         ) && \
		   (lamp_reported_power_level <  LAMP_PWR_MAX_SETTINGS_C);
}

/**
 * @brief 
 * 
 * @return true 
 * @return false 
 */
bool lamp_is_power_ok(void)
{
	return (!lamp_power_is_too_low() && !lamp_power_is_too_high());
}

/**
 * @brief Return the string ID for a power level
 * 
 * @param pwr_level @ref LAMP_PWR_LEVEL_E
 * @return const char* 
 */
const char* lamp_get_power_level_string(LAMP_PWR_LEVEL_E pwr_level)
{
	static const char* names[] = {
		[LAMP_PWR_OFF_C]     = "OFF",
		[LAMP_PWR_20PCT_C]   = "20%",
		[LAMP_PWR_40PCT_C]   = "40%",
		[LAMP_PWR_70PCT_C]   = "70%",
		[LAMP_PWR_100PCT_C]  = "100%",
		[LAMP_PWR_UNKNOWN_C] = "??%"
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
int lamp_get_raw_freq(void)
{
	return lamp_latched_freq_hz;
}

/**
 * @brief Return the current lamp state
 * 
 * @return LAMP_STATE_E 
 */
LAMP_STATE_E lamp_get_lamp_state(void)
{
	return lamp_state;
}

/**
 * @brief Return the string ID for a lamp state
 * 
 * @param state @ref LAMP_STATE_E
 * @return const char* 
 */
const char* lamp_get_lamp_state_str(LAMP_STATE_E state)
{
	static const char* names[] = {
		[LAMP_STATE_OFF_C] 					= "OFF",
		[LAMP_STATE_STARTING_C] 			= "STARTING",
		[LAMP_STATE_RUNNING_C] 				= "RUNNING",
		[LAMP_STATE_FULLPOWER_TEST_C] 	   	= "FULLPOWER_TEST",
		[LAMP_STATE_RESTRIKE_COOLDOWN_1_C] 	= "RESTRIKE_COOLDOWN_1",
		[LAMP_STATE_RESTRIKE_ATTEMPT_1_C]  	= "RESTRIKE_ATTEMPT_1",
		[LAMP_STATE_RESTRIKE_COOLDOWN_2_C] 	= "RESTRIKE_COOLDOWN_2",
		[LAMP_STATE_RESTRIKE_ATTEMPT_2_C] 	= "RESTRIKE_ATTEMPT_2",
		[LAMP_STATE_RESTRIKE_COOLDOWN_3_C] 	= "RESTRIKE_COOLDOWN_3",
		[LAMP_STATE_RESTRIKE_ATTEMPT_3_C] 	= "RESTRIKE_ATTEMPT_3",
		[LAMP_STATE_FAILED_OFF_C] 			= "FAILED_OFF",
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
int lamp_get_state_elapsed_ms(void)
{
	return (time_us_64() - lamp_state_transition_time) / 1000;
}

/**
 * @brief Returns whether the lamp is warming or not
 * 
 * @return true 
 * @return false 
 */
bool lamp_is_warming(void)
{
    uint32_t ms = lamp_get_state_elapsed_ms();

    return (lamp_state == LAMP_STATE_STARTING_C) ||
           ((lamp_state == LAMP_STATE_RUNNING_C) && 
		    (ms < LAMP_START_MS_TIME_C));
}


/* Callback functions --------------------------------------------------------*/

/**
 * @brief Callback for lamp status ISR
 * 
 * Counts the status events
 * 
 * @param a_gpio 
 * @param a_events 
 * 
 * @return 	void
 */
void lamp_status_gpio_callback(uint gpio, uint32_t events)
{
	if (gpio == PIN_STATUS_LAMP)
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
 * @related LAMP_STATE_E
 * 
 * @return 	void
 */
static inline void lamp_go_to_state(LAMP_STATE_E state)
{
	if (state != lamp_state) 
	{
		printf("State transition to %s\n", lamp_get_lamp_state_str(state));
	}
	lamp_state = state;
	lamp_state_transition_time = time_us_64();
}

/**
 * @brief 
 * 
 * @return 	void
 */
static void lamp_perform_type_test_inner(void)
{
	// V1.1: Rail-selection test (commented out — doesn't work on V1.2 due to
	// 12V rail staying active when buck disabled with 24V on, causing false detection).
	// V1.1: lamp_set_switched_24v(false); lamp_set_switched_12v(false);
	// V1.1: lamp_set_switched_12v(true); → test dimmable (12V only)
	// V1.1: lamp_set_switched_24v(true); → test non-dimmable (add 24V)

	// V1.2: Dimming-response test. Both rails stay on. Request 70% —
	// STARTING forces 100% to strike. Once RUNNING, commanded drops to 70%.
	// Then wait for reported to match commanded:
	//   - Match (reported=70%) → dimmable (I3 responded to DIMMING pin)
	//   - Timeout (reported=100%) → non-dimmable (I2 has no DIMMING pin)

	lamp_current_type = LAMP_TYPE_UNKNOWN_C;
	lamp_request_power_level(LAMP_PWR_OFF_C);
	lamp_update();
	lamp_update();
	sleep_ms(100);

	// Phase 1: Strike the lamp
	printf("[V1.2] Type test: requesting 70%%, striking at 100%%...\n");
	lamp_request_power_level(LAMP_PWR_70PCT_C);

	// Wait for strike (STARTING → RUNNING) with safety timeout
	uint64_t start = time_us_64();
	while ((time_us_64() - start) < (30ULL * 1000 * 1000))
	{
		lamp_update();
		sleep_ms(10);

		if (lamp_is_test_state_failure(lamp_get_lamp_state()))
		{
			printf("[V1.2] Type test: failed to strike\n");
			lamp_current_type = LAMP_TYPE_UNKNOWN_C;
			return;
		}
		if (lamp_get_lamp_state() == LAMP_STATE_RUNNING_C)
		{
			break;
		}
	}

	if (lamp_get_lamp_state() != LAMP_STATE_RUNNING_C)
	{
		printf("[V1.2] Type test: strike timeout\n");
		lamp_current_type = LAMP_TYPE_UNKNOWN_C;
		lamp_request_power_level(LAMP_PWR_100PCT_C);
		return;
	}

	// Wait for warmup to complete before testing dimming response —
	// the ballast ignores DIMMING commands during its ~10s warmup period.
	printf("[V1.2] Type test: lamp running, waiting for warmup...\n");
	while (lamp_is_warming())
	{
		lamp_update();
		sleep_ms(10);
	}

	// Phase 2: Check dimming response — wait for reported == commanded
	// Dimmable (I3) responds to DIMMING pin → reported settles to 70%
	// Non-dimmable (I2) ignores it → reported stays at 100%
	printf("[V1.2] Type test: warmup done, checking dimming response...\n");

	uint64_t dim_start = time_us_64();
	LAMP_PWR_LEVEL_E reported;

	while ((time_us_64() - dim_start) < (5ULL * 1000 * 1000))
	{
		lamp_update();
		sleep_ms(10);

		lamp_get_reported_power_level(&reported);
		if (reported == lamp_get_commanded_power_level())
		{
			break;
		}
	}

	lamp_get_reported_power_level(&reported);
	printf("[V1.2] Type test: commanded=%s, reported=%s, freq=%dHz\n",
		   lamp_get_power_level_string(lamp_get_commanded_power_level()),
		   lamp_get_power_level_string(reported),
		   lamp_get_raw_freq());

	if (reported == LAMP_PWR_70PCT_C)
	{
		printf("[V1.2] Determined dimmable (I3, responded to 70%% dimming)\n");
		lamp_current_type = LAMP_TYPE_DIMMABLE_C;
	}
	else if (reported == LAMP_PWR_100PCT_C)
	{
		printf("[V1.2] Determined non-dimmable (I2, ignored dimming)\n");
		lamp_current_type = LAMP_TYPE_NON_DIMMABLE_C;
		lamp_request_power_level(LAMP_PWR_100PCT_C);
	}
	else
	{
		printf("[V1.2] Type test inconclusive (reported=%s) — UNKNOWN\n",
			   lamp_get_power_level_string(reported));
		lamp_current_type = LAMP_TYPE_UNKNOWN_C;
		lamp_request_power_level(LAMP_PWR_100PCT_C);
	}
}

/**
 * @brief Returns whether a lamp state is cosidered failure or not
 * 
 * @param state (LAMP_STATE_E)
 * @return true 
 * @return false 
 */
static bool lamp_is_test_state_failure(LAMP_STATE_E state)
{
	if (state == LAMP_STATE_FAILED_OFF_C)
	{
		return true;
	}

	if (state == LAMP_STATE_RESTRIKE_COOLDOWN_1_C)
	{
		// TODO: If we want to be really conservative, allow three restrikes while determining
		return true; 
	}

	return false;
}

/**
 * @brief Returns whether the sensed power is too low
 * 
 * @return true 
 * @return false 
 */
static bool lamp_power_is_too_low(void)
{
	return g_sense_12v < 10.5;
}

/**
 * @brief Returns whether the sensed power is too high
 * 
 * @return true 
 * @return false 
 */
static bool lamp_power_is_too_high(void)
{
	return g_sense_12v > 13.5;
}

/**
 * @brief Returns whether the sensed power is too low while USB is trying for 12V
 * 
 * @return true 
 * @return false 
 */
static bool lamp_usb_power_is_too_low(void)
{
	return usbpd_get_is_trying_for_12v() && lamp_power_is_too_low();
}

/**
 * @brief Returns whether the sensed power is too high while USB is trying for 
 * 12V
 * 
 * @return true 
 * @return false 
 */
static bool lamp_usb_power_is_too_high(void)
{
	return usbpd_get_is_trying_for_12v() && lamp_power_is_too_high();
}

/**
 * @brief Returns whether the sensed power is too high while USB is not trying 
 * for 12V
 * 
 * @return true 
 * @return false 
 */
static bool lamp_jack_power_is_too_high(void)
{
	return lamp_power_is_too_high() && !usbpd_get_is_trying_for_12v();
}

/**
 * @brief Returns whether the sensed power is too low while USB is not trying 
 * for 12V
 * 
 * @return true 
 * @return false 
 */
static bool lamp_jack_power_is_too_low(void)
{
	return lamp_power_is_too_low() && !usbpd_get_is_trying_for_12v();
}

/*** END OF FILE ***/
