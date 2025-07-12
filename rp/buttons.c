#include <stdio.h>
#include <hardware/gpio.h>
#include <pico/stdlib.h>
#include "buttons.h"
#include "pins.h"

#define PULSE_TIME_US (1000*200)
#define PULSE_TIME_INITIAL_US (1000*300)
#define DEBOUNCE_TIME_US (1000*5)

struct {
	int pin;
	buttons_t button;
	uint64_t last_pulsed_us;
	uint64_t first_down_us;
	bool down_last_frame;
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
		bool raw = !gpio_get(buttons[i].pin);
		
		if (raw && buttons[i].first_down_us==0)
		{
			buttons[i].first_down_us = now;
		}
		else if (!raw)
		{
			buttons[i].first_down_us = 0;
		}

		bool down = buttons[i].first_down_us != 0 && ((now - buttons[i].first_down_us) > DEBOUNCE_TIME_US);

		if (down)
		{
			if (!buttons[i].down_last_frame)
			{
				buttons_pressed |= b;
				buttons[i].last_pulsed_us = now + PULSE_TIME_INITIAL_US;
			}
			else if ((now - buttons[i].last_pulsed_us) > PULSE_TIME_US)
			{
				buttons_pulsed |= b;
				buttons[i].last_pulsed_us = now;
			}

			buttons_down |= b;
		}
		else
		{
			if (buttons[i].down_last_frame)
			{
				buttons_released |= b;
			}

			buttons[i].last_pulsed_us = 0;
		}

		buttons[i].down_last_frame = down;
	}
}

static const char * name_from_mask(uint32_t m)
{
    switch(m) {
        case BUTTON_UP   : return "UP";
        case BUTTON_DOWN : return "DOWN";
        case BUTTON_LEFT : return "LEFT";
        case BUTTON_RIGHT: return "RIGHT";
        case BUTTON_CENTER: return "CENTER";
        default          : return "?";
    }
}

void dump_buttons() // for debugging
{
    if(buttons_pressed) {
        printf("pressed :");
        for(uint32_t bit = 1; bit; bit <<= 1)
            if(buttons_pressed & bit) printf(" %s", name_from_mask(bit));
        printf("\n");
    }

    if(buttons_released) {
        printf("released:");
        for(uint32_t bit = 1; bit; bit <<= 1)
            if(buttons_released & bit) printf(" %s", name_from_mask(bit));
        printf("\n");
    }

    if(buttons_down) {
        printf("down    :");
        for(uint32_t bit = 1; bit; bit <<= 1)
            if(buttons_down & bit) printf(" %s", name_from_mask(bit));
        printf("\n");
    }
}