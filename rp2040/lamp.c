#include <stdio.h>
#include <pico/stdlib.h>
#include <hardware/gpio.h>
#include <hardware/pwm.h>

#include "lamp.h"
#include "pins.h"
#include "usbpd.h"
#include "sense.h"
#include "radar.h"

const int STEPCOUNT_SOFTSTART = 64;
const int STEPCOUNT_DIMMING = 100;

volatile int lamp_status_events = 0;

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
bool is_lamp_on = false;
int is_dim = 100;

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

uint64_t started_lamp = 0;

void set_lamp(bool on, int dim)
{
	if (dim < 0) dim = 0;
	if (dim > 100) dim = 100;

	is_dim = dim;

	if (!is_12v_on && on)
	{
		printf("Reject turn on lamp without 12V\n");
	}

	if (!is_lamp_on && on) pwm_set_gpio_level(PIN_PWM_LAMP, 0);

	gpio_put(PIN_ENABLE_LAMP, on);

	is_lamp_on = on;
	started_lamp = time_us_64();
	
}

bool get_lamp_command()
{
	return is_lamp_on;
}

bool get_lamp_status()
{
	return !gpio_get(PIN_STATUS_LAMP);
}

int get_lamp_dim()
{
	return is_dim;
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
	}

	if ((now - started_lamp) < (5*1000*1000)) // Full power for 5s
	{
		pwm_set_gpio_level(PIN_PWM_LAMP, 0);
	}
	else
	{
		pwm_set_gpio_level(PIN_PWM_LAMP, is_dim);
	}

	if (sense_12v < 10.5 || sense_12v > 13.5)
	{
		set_lamp(false, 0);
		set_switched_24v(false);
		set_switched_12v(false);
	}
}

int get_lamp_freq()
{
	return latched_lamp_hz;
}
