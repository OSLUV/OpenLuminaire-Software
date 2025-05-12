#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "pins.h"
#include "radar.h"

const int output_pins[] = {LED0_PIN, LED1_PIN, FAN_ENABLE_PIN};

bool get_lamp_status_on()
{
	return !gpio_get(LAMP_STATUS_PIN);
}

void set_lamp_commanded_on(bool on)
{
	gpio_put(LAMP_ENABLE_PIN, on);
}

bool get_lamp_commanded_on()
{
	return gpio_get(LAMP_ENABLE_PIN);
}

void set_leds(bool on0, bool on1)
{
	gpio_put(LED0_PIN, on0);
	gpio_put(LED1_PIN, on1);
}

void init_fan()
{
	const uint pin = FAN_ENABLE_PIN;
	const uint count_top = (1<<16) - 1;
    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_wrap(&cfg, count_top);
    pwm_init(pwm_gpio_to_slice_num(pin), &cfg, true);
    gpio_set_function(pin, GPIO_FUNC_PWM);
    
}

void set_fan(float speed)
{
	const uint count_top = (1<<16) - 1;
	uint req_speed = speed * count_top;
	const uint pin = FAN_ENABLE_PIN;
	int ch = pwm_gpio_to_channel(pin);
    pwm_set_both_levels(pwm_gpio_to_slice_num(pin), (ch==0)?req_speed:0, (ch==1)?req_speed:0);
}


int main() {
	
	gpio_init(LAMP_ENABLE_PIN);
	gpio_set_dir(LAMP_ENABLE_PIN, GPIO_OUT);
	set_lamp_commanded_on(false);

	stdio_init_all();

	for (int i = 0; i < (sizeof(output_pins) / sizeof(output_pins[0])); i++)
	{
		gpio_init(output_pins[i]);
    	gpio_set_dir(output_pins[i], GPIO_OUT);
	}

	gpio_init(LAMP_STATUS_PIN);
	gpio_set_dir(LAMP_STATUS_PIN, GPIO_IN);
	gpio_set_pulls(LAMP_STATUS_PIN, true, false);

	set_leds(1, 0);

	radar_init();

	init_fan();

start:
	set_fan(1);
    int retry_no = 0;
    while (retry_no < 3)
    {
    	// Indicate waiting charge state
    	set_leds(1, 1);

    	sleep_ms(750);
    	
    	set_lamp_commanded_on(true);

    	for (int i = 0; i < 10; i++)
    	{
    		// Indicate waiting to see if struck
    		set_leds(1, (i%2)==1);
    		sleep_ms(1000);

    		if (radar_get_presence())
			{
				goto lockout;
			}
    	}

    	if (get_lamp_status_on())
    	{
    		// Success, we're lit and all is well
    		goto lit;
    	}

    	// Failed to strike
    	set_lamp_commanded_on(false);
    	// Loop
    	retry_no++;
    }

    // Did not strike after 3 tries
    goto fail;

lit:
	while (1)
	{
		set_leds(0, 1);
		sleep_ms(1000);
		if (radar_get_presence())
		{
			goto lockout;
		}

		sleep_ms(1000);
		if (radar_get_presence())
		{
			goto lockout;
		}

		set_leds(1, 0);
		sleep_ms(1000);
		if (radar_get_presence())
		{
			goto lockout;
		}

		sleep_ms(1000);
		if (radar_get_presence())
		{
			goto lockout;
		}

		if (!get_lamp_status_on())
		{
			// Lamp died, command off, wait 10s, retry
			// Indicate with blink pattern
			for (int i = 0; i < 10; i++)
			{
				set_leds(1, 1);
				sleep_ms(100);
				set_leds(1, 0);
				sleep_ms(100);
				set_leds(1, 1);
				sleep_ms(100);
				set_leds(1, 0);
				sleep_ms(100);
				set_leds(1, 1);
				sleep_ms(100);
				set_leds(1, 0);
				sleep_ms(500);
			}

			goto start;
		}
	}

lockout:
	set_fan(0);
	set_lamp_commanded_on(false);
	while (radar_get_presence())
	{
		set_leds(1, 1);
		sleep_ms(100);
		set_leds(1, 0);
		sleep_ms(900);
	}

	goto start;

fail:
	set_fan(0);
	while (1)
	{
		set_leds(1, 1);
		sleep_ms(100);
		set_leds(0, 0);
		sleep_ms(100);
		set_leds(1, 1);
		sleep_ms(100);
		set_leds(0, 0);
		sleep_ms(100);
		set_leds(1, 1);
		sleep_ms(100);
		set_leds(0, 0);
		sleep_ms(500);
	}
}


// LED codes:
// off, off = total failure
// on, off = radar failure
// on, on = charging before striking
// on, blinking 1s on, 1s off = waiting to see if struck
// alternating blinking slowly = operating normally
// on, three quick blinks repeating = lamp died. waiting to restart
// three quick blinks repeating, three quick blinks repeating = failed to strike 3x and gave up