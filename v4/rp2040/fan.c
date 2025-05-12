#include <hardware/pwm.h>
#include <hardware/gpio.h>
#include "pins.h"

#define STEPCOUNT_FAN 1000

void init_fan()
{
	gpio_set_function(PIN_FAN_PWM, GPIO_FUNC_PWM);
	uint slice_num = pwm_gpio_to_slice_num(PIN_FAN_PWM);

	pwm_config config = pwm_get_default_config();
    // Set divider, reduces counter clock to sysclock/this value
    pwm_config_set_clkdiv(&config, 125);

    pwm_config_set_wrap(&config, STEPCOUNT_FAN); // 1kHz

    // Load the configuration into our PWM slice, and set it running.
    pwm_init(slice_num, &config, false);

    pwm_set_gpio_level(PIN_FAN_PWM, 0);

    pwm_set_enabled(slice_num, true);
}

int curr_speed = 0;

void set_fan(int speed)
{
	if (speed > 100) speed = 100;
	if (speed < 0) speed = 0;
	
	pwm_set_gpio_level(PIN_FAN_PWM, speed * STEPCOUNT_FAN / 100);
	curr_speed = speed;
}

int get_fan()
{
	return curr_speed;
}
