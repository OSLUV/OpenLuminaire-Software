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

const int STEPCOUNT_SOFTSTART = 64;
const int STEPCOUNT_DIMMING = 100;

volatile int lamp_status_events = 0;

struct {
	int pwm;
	int power;
} pwr_settings[NUM_REAL_PWR_SETTING] = {
	[PWR_OFF] = {0, 0},
	[PWR_20PCT] = {100, 20},
	[PWR_40PCT] = {83, 40},
	[PWR_70PCT] = {50, 70},
	[PWR_100PCT] = {0, 100},
};

enum lamp_type current_lamp_type = LAMP_TYPE_UNKNOWN;

enum lamp_type get_lamp_type()
{
	return current_lamp_type;
}

void gpio_callback(uint gpio, uint32_t events)
{
	if (gpio == PIN_STATUS_LAMP)
	{
		lamp_status_events++;
	}
}

void init_lamp()
{
	gpio_init(PIN_ENABLE_24V);
	gpio_set_dir(PIN_ENABLE_24V, GPIO_OUT);
	gpio_put(PIN_ENABLE_24V, false);

	gpio_init(PIN_ENABLE_LAMP);
	gpio_set_dir(PIN_ENABLE_LAMP, GPIO_OUT);
	gpio_put(PIN_ENABLE_LAMP, false);

	gpio_init(PIN_STATUS_LAMP);
	gpio_set_dir(PIN_STATUS_LAMP, GPIO_IN);
	gpio_set_pulls(PIN_STATUS_LAMP, true, false);

	gpio_set_irq_callback(gpio_callback);
    gpio_set_irq_enabled(PIN_STATUS_LAMP, GPIO_IRQ_EDGE_RISE, true);
    irq_set_enabled(IO_IRQ_BANK0, true);

	{
		gpio_set_function(PIN_ENABLE_12V, GPIO_FUNC_PWM);
		uint slice_num = pwm_gpio_to_slice_num(PIN_ENABLE_12V);

		pwm_config config = pwm_get_default_config();
	    // Set divider, reduces counter clock to sysclock/this value
	    pwm_config_set_clkdiv(&config, 8);

	    pwm_config_set_wrap(&config, STEPCOUNT_SOFTSTART); // ~244kHz

	    // Load the configuration into our PWM slice, and set it running.
	    pwm_init(slice_num, &config, false);

	    pwm_set_gpio_level(PIN_ENABLE_12V, 0);

	    pwm_set_enabled(slice_num, true);
	}

	{
		gpio_set_function(PIN_PWM_LAMP, GPIO_FUNC_PWM);
		uint slice_num = pwm_gpio_to_slice_num(PIN_PWM_LAMP);

		pwm_config config = pwm_get_default_config();
	    // Set divider, reduces counter clock to sysclock/this value
	    pwm_config_set_clkdiv(&config, 8);

	    pwm_config_set_wrap(&config, STEPCOUNT_DIMMING-1); // 244kHz

	    // Load the configuration into our PWM slice, and set it running.
	    pwm_init(slice_num, &config, false);

	    pwm_set_gpio_level(PIN_PWM_LAMP, 0);

	    pwm_set_enabled(slice_num, true);
	}
}

bool is_12v_on = false;
bool is_24v_on = false;
enum pwr_level requested_power_level = PWR_OFF;
enum pwr_level commanded_power_level = PWR_OFF;
enum pwr_level reported_power_level = PWR_UNKNOWN;

void set_switched_12v(bool on)
{
	if ((sense_12v < 11.5 || sense_12v > 12.5) && on)
	{
		printf("Reject turn on 12V when 12v not OK\n");
		return;
	}

	if (!is_12v_on && on)
	{
		for (int i = 0; i <= STEPCOUNT_SOFTSTART+1; i++)
		{
			pwm_set_gpio_level(PIN_ENABLE_12V, i);
			sleep_ms(8);
		}
	}
	else if (is_12v_on && !on)
	{
		for (int i = STEPCOUNT_SOFTSTART+1; i >= 0; i--)
		{
			pwm_set_gpio_level(PIN_ENABLE_12V, i);
			sleep_ms(1);
		}
	}

	is_12v_on = on;
}

void set_switched_24v(bool on)
{
	if (!is_12v_on && on)
	{
		printf("Reject turn on 24V when 12V not available\n");
		return;
	}

	gpio_put(PIN_ENABLE_24V, on);

	is_24v_on = on;
}

bool get_switched_12v()
{
	return is_12v_on;
}

bool get_switched_24v()
{
	return is_24v_on;
}

enum lamp_state lamp_state = STATE_OFF;

#define RESTRIKE_COOLDOWN_TIME 5000
#define START_TIME 10000

uint64_t lamp_state_transition_time = 0;

bool request_lamp_power(enum pwr_level power)
{
	if (requested_power_level == power) return true; // already satisfied
	if (lamp_state == STATE_FAILED_OFF) return false; // dead

	bool requested_on = power != PWR_OFF;

	if (get_lamp_type() == LAMP_TYPE_NONDIMMABLE)
	{
		if (power != PWR_OFF && power != PWR_100PCT)
		{
			printf("Reject dimmed control point for lamp not known to dim\n");
			return false;
		}

		if (!is_24v_on && power != PWR_OFF)
		{
			printf("Reject turn on for non-dimmable lamp without 24V\n");
			return false;
		}
	}

	if (!is_12v_on && power != PWR_OFF)
	{
		printf("Reject turn on lamp without 12V\n");
		return false;
	}

	if (requested_power_level == PWR_OFF && power != PWR_OFF)
	{
		printf("Lamp goes to STATE_STARTING\n");
		lamp_state = STATE_STARTING;
		lamp_state_transition_time = time_us_64();
	}

	requested_power_level = power;	
}

enum pwr_level get_lamp_requested_power()
{
	return requested_power_level;
}

enum pwr_level get_lamp_commanded_power()
{
	return commanded_power_level;
}

bool get_lamp_reported_power(enum pwr_level* out)
{
	*out = reported_power_level;
	return reported_power_level >= PWR_OFF && reported_power_level < NUM_REAL_PWR_SETTING;
}

uint64_t last_lamp_update = 0;
int latched_lamp_hz = 0;

void update_lamp()
{
	uint64_t now = time_us_64();

	if ((now - last_lamp_update) > (1000*1000))
	{
		latched_lamp_hz = lamp_status_events;
		lamp_status_events = 0;
		last_lamp_update = now;

		reported_power_level = PWR_UNKNOWN;

		if (current_lamp_type == LAMP_TYPE_NONDIMMABLE)
		{
			reported_power_level = (!gpio_get(PIN_STATUS_LAMP)) ? PWR_100PCT : PWR_OFF;
		}
		else // Includes unknown case because this is used while testing
		{
			if (commanded_power_level == PWR_OFF) reported_power_level = PWR_OFF;
			else if (latched_lamp_hz < 100)
			{
				if (commanded_power_level != PWR_OFF && (!gpio_get(PIN_STATUS_LAMP))) reported_power_level = PWR_100PCT;
				else if (gpio_get(PIN_STATUS_LAMP)) reported_power_level = PWR_OFF;
			}
			else if (latched_lamp_hz > 900 && latched_lamp_hz < 1100) reported_power_level = PWR_70PCT;
			else if (latched_lamp_hz > 400 && latched_lamp_hz < 600) reported_power_level = PWR_40PCT;
			else if (latched_lamp_hz > 150 && latched_lamp_hz < 250) reported_power_level = PWR_20PCT;
		}
	}

	uint64_t elapsed_ms_in_state = (time_us_64() - lamp_state_transition_time) / 1000;

	#define GOTO_STATE(x) {if (x != lamp_state) printf("State transition to %s\n", lamp_state_str(x)); lamp_state = x; lamp_state_transition_time = time_us_64();}

	if (lamp_state == STATE_STARTING)
	{
		commanded_power_level = PWR_100PCT;

		if (reported_power_level == PWR_100PCT)
		//if (latched_lamp_hz > 0) 
		{
			GOTO_STATE(STATE_RUNNING);
		}

		if (elapsed_ms_in_state > START_TIME)
		{
			GOTO_STATE(STATE_RESTRIKE_COOLDOWN_1);
		}

		if (requested_power_level == PWR_OFF)
		{
			GOTO_STATE(PWR_OFF);
		}

		// Don't go to off once starting to avoid short cycling
	}
	else if (lamp_state == STATE_RUNNING)
	{
		commanded_power_level = requested_power_level;

		if (get_lamp_type() == LAMP_TYPE_DIMMABLE && elapsed_ms_in_state > (30*1000))
		{
			printf("Initiate full-power test\n");
			GOTO_STATE(STATE_FULLPOWER_TEST);
		}

		if (get_lamp_type() == LAMP_TYPE_NONDIMMABLE && elapsed_ms_in_state > 1000 & reported_power_level != PWR_100PCT)
		{
			// Can tell immediately if a non-dimmable lamp has gone out
			GOTO_STATE(STATE_RESTRIKE_COOLDOWN_1);
		}

		if (requested_power_level == PWR_OFF)
		{
			GOTO_STATE(STATE_OFF);
		}
	}
	else if (lamp_state == STATE_FULLPOWER_TEST)
	{
		commanded_power_level = PWR_100PCT;

		if (reported_power_level == PWR_100PCT)
		{
			printf("Got a reported 100%% power, OK\n");
			GOTO_STATE(STATE_RUNNING);
		}
		else if (elapsed_ms_in_state > START_TIME)
		{
			printf("Timed out for 100%% test\n");
			GOTO_STATE(STATE_RESTRIKE_COOLDOWN_1);
		}

		if (requested_power_level == PWR_OFF)
		{
			GOTO_STATE(PWR_OFF);
		}

		// Don't go to off while fullpower test -- open question?
	}

	#define RESTRIKE_CASE_N(n, goto) \
	else if (lamp_state == STATE_RESTRIKE_COOLDOWN_##n)\
	{\
		commanded_power_level = PWR_OFF;\
		if (requested_power_level == PWR_OFF)\
		{\
			GOTO_STATE(STATE_OFF);\
		}\
		if (elapsed_ms_in_state > RESTRIKE_COOLDOWN_TIME)\
		{\
			printf("Going to restrike attempt #" #n "\n");\
			GOTO_STATE(STATE_RESTRIKE_ATTEMPT_##n);\
		}\
	}\
	else if (lamp_state == STATE_RESTRIKE_ATTEMPT_##n)\
	{\
		commanded_power_level = PWR_100PCT;\
		if (reported_power_level == PWR_100PCT)\
		{\
			printf("Restrike succeeded on attempt #" #n "\n");\
			GOTO_STATE(STATE_STARTING);\
		}\
		if (elapsed_ms_in_state > START_TIME)\
		{\
			printf("Timed out on restrike attempt #" #n "\n");\
			GOTO_STATE(goto);\
		}\
		if (requested_power_level == PWR_OFF)\
		{\
			GOTO_STATE(PWR_OFF);\
		}\
	}

	// Don't go OFF in the middle of restrike attempt to avoid short cycling

	RESTRIKE_CASE_N(1, STATE_RESTRIKE_COOLDOWN_2)
	RESTRIKE_CASE_N(2, STATE_RESTRIKE_COOLDOWN_3)
	RESTRIKE_CASE_N(3, STATE_FAILED_OFF)

	else if (lamp_state == STATE_FAILED_OFF)
	{
		commanded_power_level = PWR_OFF;

		if (requested_power_level == PWR_OFF)
		{
			GOTO_STATE(STATE_OFF);
		}
	}
	else if (lamp_state == STATE_OFF)
	{
		commanded_power_level = PWR_OFF;
	}

	pwm_set_gpio_level(PIN_PWM_LAMP, pwr_settings[commanded_power_level].pwm);
	gpio_put(PIN_ENABLE_LAMP, commanded_power_level != PWR_OFF);

	if (sense_12v < 10.5 || sense_12v > 13.5)
	{
		gpio_put(PIN_ENABLE_LAMP, true);
		sleep_ms(10);
		set_switched_24v(false);
		set_switched_12v(false);
		GOTO_STATE(STATE_OFF);
	}
}

int get_lamp_raw_freq()
{
	return latched_lamp_hz;
}

bool consider_state_test_failure(enum lamp_state state)
{
	if (state == STATE_FAILED_OFF)
		return true;

	if (state == STATE_RESTRIKE_COOLDOWN_1)
		return true; // TODO: If we want to be really conservative, allow three restrikes while determining

	return false;
}

void lamp_perform_type_test_inner()
{
	current_lamp_type = LAMP_TYPE_UNKNOWN;
	request_lamp_power(PWR_OFF);
	update_lamp();
	update_lamp();
	sleep_ms(100);
	
	set_switched_24v(false);
	sleep_ms(100);
	set_switched_12v(false);
	sleep_ms(100);
	set_switched_12v(true);
	sleep_ms(1000);
	request_lamp_power(PWR_100PCT);

	while (true)
	{
		update_lamp();
		sleep_ms(10);

		if (consider_state_test_failure(get_lamp_state()))
		{
			break;
		}
		else if (get_lamp_state() == STATE_RUNNING)
		{
			printf("Determined dimmable\n");
			current_lamp_type = LAMP_TYPE_DIMMABLE;
			return;
		}
	}

	request_lamp_power(PWR_OFF);
	update_lamp();
	update_lamp();
	sleep_ms(100);

	set_switched_24v(true);
	sleep_ms(1000);

	request_lamp_power(PWR_100PCT);

	while (true)
	{
		update_lamp();
		sleep_ms(10);

		if (consider_state_test_failure(get_lamp_state()))
		{
			break;
		}
		else if (get_lamp_state() == STATE_RUNNING)
		{
			printf("Determined non-dimmable\n");
			current_lamp_type = LAMP_TYPE_NONDIMMABLE;
			return;
		}
	}

	printf("Determined unknown\n");
	current_lamp_type = LAMP_TYPE_UNKNOWN;
	return;
}

void load_lamp_type_from_flash()
{
	printf("Determined type from flash\n");
	current_lamp_type = persistance_region.factory_lamp_type;
}

void lamp_perform_type_test()
{
	if (get_lamp_type() == LAMP_TYPE_UNKNOWN)
	{
		printf("Performing lamp type test\n");
		lamp_perform_type_test_inner();
		printf("Done\n");

		if (get_lamp_type() != LAMP_TYPE_UNKNOWN)
		{
			printf("Writing concluded type\n");
			persistance_region.factory_lamp_type = get_lamp_type();
			write_persistance_region();
		}
	}
}

const char* pwr_level_str(enum pwr_level l)
{
	static const char* names[] = {
		[PWR_OFF] = "OFF",
		[PWR_20PCT] = "20%",
		[PWR_40PCT] = "40%",
		[PWR_70PCT] = "70%",
		[PWR_100PCT] = "100%",
		[PWR_UNKNOWN] = "??%"
	};

	if (l <= (sizeof(names)/sizeof(names[0])))
	{
		return names[l];
	}

	return "!?!";
}

enum lamp_state get_lamp_state()
{
	return lamp_state;
}

const char* lamp_state_str(enum lamp_state s)
{
	static const char* names[] = {
		[STATE_OFF] = "OFF",
		[STATE_STARTING] = "STARTING",
		[STATE_RUNNING] = "RUNNING",
		[STATE_FULLPOWER_TEST] = "FULLPOWER_TEST",
		[STATE_RESTRIKE_COOLDOWN_1] = "RESTRIKE_COOLDOWN_1",
		[STATE_RESTRIKE_ATTEMPT_1] = "RESTRIKE_ATTEMPT_1",
		[STATE_RESTRIKE_COOLDOWN_2] = "RESTRIKE_COOLDOWN_2",
		[STATE_RESTRIKE_ATTEMPT_2] = "RESTRIKE_ATTEMPT_2",
		[STATE_RESTRIKE_COOLDOWN_3] = "RESTRIKE_COOLDOWN_3",
		[STATE_RESTRIKE_ATTEMPT_3] = "RESTRIKE_ATTEMPT_3",
		[STATE_FAILED_OFF] = "FAILED_OFF",
	};

	if (s <= (sizeof(names)/sizeof(names[0])))
	{
		return names[s];
	}

	return "!?!";
}

int get_lamp_state_elapsed_ms()
{
	return (time_us_64() - lamp_state_transition_time) / 1000;
}

// power flags - for more verbose error messages later on

bool power_too_low(){
	return sense_12v < 10.5;
}

bool power_too_high(){
	return sense_12v > 13.5;
}

bool power_ok()
{
	return (!power_too_low() && !power_too_high());
}

bool usb_too_low()
{
	return usbpd_get_is_trying_for_12v() && power_too_low();
}

bool usb_too_high()
{
	return usbpd_get_is_trying_for_12v() && power_too_high();
}

bool jack_too_high()
{
	return power_too_high() && !usbpd_get_is_trying_for_12v();
}
bool jack_too_low()
{
	return power_too_low() && !usbpd_get_is_trying_for_12v();
}
