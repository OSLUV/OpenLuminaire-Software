/**
 * @file      lamp.c
 * @author    The OSLUV Project
 * @brief     Driver for lamp control
 * @ref       lamp_controller.SchDoc
 * @ref       power.SchDoc
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

const int LAMP_STEPCOUNT_SOFTSTART_C = 64;
const int LAMP_STEPCOUNT_DIMMING_C   = 100;

const LAMP_PWR_CTL_T lamp_pwr_settings[LAMP_PWR_MAX_SETTINGS_C] = {
	[LAMP_PWR_OFF_C]    = {0,     0},
	[LAMP_PWR_20PCT_C]  = {100,  20},
	[LAMP_PWR_40PCT_C]  = {83,   40},
	[LAMP_PWR_70PCT_C]  = {50,   70},
	[LAMP_PWR_100PCT_C] = {0,   100},
};

static bool b_lamp_is_12v_on = false;
static bool b_lamp_is_24v_on = false;

static LAMP_TYPE_E  lamp_current_type = LAMP_TYPE_UNKNOWN_C;
static LAMP_STATE_E lamp_state 		  = LAMP_STATE_OFF_C;

static LAMP_PWR_LEVEL_E lamp_requested_power_level = LAMP_PWR_OFF_C;
static LAMP_PWR_LEVEL_E lamp_commanded_power_level = LAMP_PWR_OFF_C;
static LAMP_PWR_LEVEL_E lamp_reported_power_level  = LAMP_PWR_UNKNOWN_C;

static uint64_t lamp_state_transition_time = 0;

static uint64_t lamp_last_update = 0;
static int lamp_latched_freq_hz  = 0;

volatile int lamp_status_events  = 0;


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

	if (sense_12v < 10.5 || sense_12v > 13.5)
	{
		gpio_put(PIN_ENABLE_LAMP, true);
		sleep_ms(10);
		lamp_set_switched_24v(false);
		lamp_set_switched_12v(false);
		lamp_go_to_state(LAMP_STATE_OFF_C);
	}
}

/**
 * @brief Loads the preset factory lamp type @ref LAMP_TYPE_E
 * 
 */
void lamp_load_type_from_flash(void)
{
	printf("Determined type from flash\n");

	lamp_current_type = persistance_region.factory_lamp_type;
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
			persistance_region.factory_lamp_type = lamp_get_type();
			write_persistance_region();
		}
	}
}

/**
 * @brief Enables/disables Switched 12V Enable pin
 * 
 * Enabling requires voltage to be in a range 11.5V to 12.5V
 * 
 * @param b_on 
 */
void lamp_set_switched_12v(bool b_on)
{
	if (((sense_12v < 11.5) || (sense_12v > 12.5)) && b_on)
	{
		printf("Reject turn on 12V when 12v not OK\n");
	}
	else if (!b_lamp_is_12v_on && b_on)
	{
		for (int idx = 0; idx <= LAMP_STEPCOUNT_SOFTSTART_C + 1; idx++)
		{
			pwm_set_gpio_level(PIN_ENABLE_12V, idx);
			sleep_ms(8);
		}
		b_lamp_is_12v_on = b_on;
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
 */
void lamp_set_switched_24v(bool b_on)
{
	if (!b_lamp_is_12v_on && b_on)
	{
		printf("Reject turn on 24V when 12V not available\n");
	}
	else {

		gpio_put(PIN_ENABLE_24V, b_on);

		b_lamp_is_24v_on = b_on;
	}
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

		if (!b_lamp_is_24v_on && pwr_level != LAMP_PWR_OFF_C)
		{
			printf("Reject turn on for non-dimmable lamp without 24V\n");
			return false;
		}
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
 */
static void lamp_perform_type_test_inner(void)
{
	lamp_current_type = LAMP_TYPE_UNKNOWN_C;
	lamp_request_power_level(LAMP_PWR_OFF_C);
	lamp_update();
	lamp_update();
	sleep_ms(100);
	
	lamp_set_switched_24v(false);
	sleep_ms(100);
	lamp_set_switched_12v(false);
	sleep_ms(100);
	lamp_set_switched_12v(true);
	sleep_ms(1000);
	lamp_request_power_level(LAMP_PWR_100PCT_C);

	while (true)
	{
		lamp_update();
		sleep_ms(10);

		if (lamp_is_test_state_failure(lamp_get_lamp_state()))
		{
			break;
		}
		else if (lamp_get_lamp_state() == LAMP_STATE_RUNNING_C)
		{
			printf("Determined dimmable\n");
			lamp_current_type = LAMP_TYPE_DIMMABLE_C;
			return;
		}
	}

	lamp_request_power_level(LAMP_PWR_OFF_C);
	lamp_update();
	lamp_update();
	sleep_ms(100);

	lamp_set_switched_24v(true);
	sleep_ms(1000);

	lamp_request_power_level(LAMP_PWR_100PCT_C);

	while (true)
	{
		lamp_update();
		sleep_ms(10);

		if (lamp_is_test_state_failure(lamp_get_lamp_state()))
		{
			break;
		}
		else if (lamp_get_lamp_state() == LAMP_STATE_RUNNING_C)
		{
			printf("Determined non-dimmable\n");
			lamp_current_type = LAMP_TYPE_NON_DIMMABLE_C;
			return;
		}
	}

	printf("Determined unknown\n");
	lamp_current_type = LAMP_TYPE_UNKNOWN_C;
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
	return sense_12v < 10.5;
}

/**
 * @brief Returns whether the sensed power is too high
 * 
 * @return true 
 * @return false 
 */
static bool lamp_power_is_too_high(void)
{
	return sense_12v > 13.5;
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
