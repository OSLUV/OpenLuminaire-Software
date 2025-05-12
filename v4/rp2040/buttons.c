#include <hardware/gpio.h>
#include <pico/stdlib.h>
#include "buttons.h"
#include "pins.h"

#define PULSE_TIME_US (1000*80)
#define PULSE_TIME_INITIAL_US (1000*80)

struct {
	int pin;
	buttons_t button;
	uint64_t last_pulsed;
} buttons[] = {
	{PIN_BUTTON_UP, BUTTON_UP},
	{PIN_BUTTON_DOWN, BUTTON_DOWN},
	{PIN_BUTTON_LEFT, BUTTON_LEFT},
	{PIN_BUTTON_RIGHT, BUTTON_RIGHT},
	{PIN_BUTTON_CENTER, BUTTON_CENTER}
};

#define N_BUTTONS (sizeof(buttons)/sizeof(buttons[0]))

void init_buttons()
{
	for (int i = 0; i < N_BUTTONS; i++)
	{
		gpio_init(buttons[i].pin);
		gpio_set_dir(buttons[i].pin, GPIO_IN);
		gpio_set_pulls(buttons[i].pin, true, false);
	}
}

buttons_t buttons_pressed, buttons_released, buttons_down, buttons_pulsed = 0;
buttons_t last_frame = 0;

void update_buttons()
{
	buttons_pressed = 0;
	buttons_released = 0;
	buttons_down = 0;
	buttons_pulsed = 0;

	uint64_t now = time_us_64();

	for (int i = 0; i < N_BUTTONS; i++)
	{
		buttons_t b = buttons[i].button;
		bool d = !gpio_get(buttons[i].pin);
		bool last = last_frame & b;

		if (d)
		{
			if (!last)
			{
				buttons_pressed |= b;
				buttons[i].last_pulsed = now + PULSE_TIME_INITIAL_US;
			}
			else if ((now - buttons[i].last_pulsed) > PULSE_TIME_US)
			{
				buttons_pulsed |= b;
				buttons[i].last_pulsed = now;
			}

			buttons_down |= b;
		}
		else
		{
			if (last)
			{
				buttons_released |= b;
			}

			buttons[i].last_pulsed = 0;
		}
	}

	last_frame = buttons_down;
}

