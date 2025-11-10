/**
 * @file      buttons.c
 * @author    The OSLUV Project
 * @brief     Driver for system's buttons state monitoring
 * @hwref     U3 (RP2040)
 * @ref       lamp_controller.SchDoc
 *  
 */


/* Includes ------------------------------------------------------------------*/

#include <stdio.h>
#include <stdbool.h>
#include <hardware/gpio.h>
#include <pico/stdlib.h>
#include "buttons.h"
#include "pins.h"


/* Private typedef -----------------------------------------------------------*/

typedef struct {
	int			pin;
	BUTTONS_E	button;
	uint64_t	last_pulsed_us;
	uint64_t	first_down_us;
	bool		down_last_frame;
} BTN_CTRL_T;


/* Private define ------------------------------------------------------------*/

#define BUTTONS_PULSE_TIME_US_C           (1000 * 200) 							/* Pulse time in micro seconds */
#define BUTTONS_PULSE_TIME_INITIAL_US_C   (1000 * 300)							/* Pulse time in micro seconds after system startup */
#define BUTTONS_DEBOUNCE_TIME_US_C        (1000 * 5)							/* Debounce time in micro seconds */
#define BUTTONS_COUNT_C 				  5										/* System's buttons count */


/* Global variables  ---------------------------------------------------------*/

BUTTONS_E g_buttons_pressed, g_buttons_released, g_buttons_down, g_buttons_pulsed = 0;


/* Private variables  --------------------------------------------------------*/

BTN_CTRL_T buttons[BUTTONS_COUNT_C] = {
	{PIN_BUTTON_UP, 	BUTTON_UP_C,     0, 0, false},
	{PIN_BUTTON_DOWN, 	BUTTON_DOWN_C,   0, 0, false},
	{PIN_BUTTON_LEFT, 	BUTTON_LEFT_C,   0, 0, false},
	{PIN_BUTTON_RIGHT,	BUTTON_RIGHT_C,  0, 0, false},
	{PIN_BUTTON_CENTER,	BUTTON_CENTER_C, 0, 0, false}
};


/* Private function prototypes -----------------------------------------------*/

static const char * p_buttons_get_name_string(BUTTONS_E a_btn);


/* Exported functions --------------------------------------------------------*/


/**
 * @brief 	Hardware's buttons gpios initialization procedure
 * 
 * @return 	void 
 */
void buttons_init(void)
{
	for (int idx = 0; idx < BUTTONS_COUNT_C; idx++)
	{
		gpio_init(buttons[idx].pin);
		gpio_set_dir(buttons[idx].pin, GPIO_IN);
		gpio_set_pulls(buttons[idx].pin, true, false);
	}
}

/**
 * @brief   Updates all system's buttons state
 * 
 * A debounce filter is applied to all buttons.
 * 
 * Buttons current state will be available on global variables.
 * 
 * @return 	void  
 */
void buttons_update(void)
{
	g_buttons_pressed  = 0;
	g_buttons_released = 0;
	g_buttons_down 	   = 0;
	g_buttons_pulsed   = 0;

	uint64_t now = time_us_64();

	for (int idx = 0; idx < BUTTONS_COUNT_C; idx++)
	{
		BUTTONS_E btn   = buttons[idx].button;
		bool gpio_state = !gpio_get(buttons[idx].pin);
		
		if (gpio_state && (buttons[idx].first_down_us == 0))
		{
			buttons[idx].first_down_us = now;
		}
		else if (!gpio_state)
		{
			buttons[idx].first_down_us = 0;
		}

		bool btn_is_down =  (buttons[idx].first_down_us != 0) &&  \
			 				((now - buttons[idx].first_down_us) > \
							 BUTTONS_DEBOUNCE_TIME_US_C);

		if (btn_is_down)
		{
			if (!buttons[idx].down_last_frame)
			{
				g_buttons_pressed |= btn;

				buttons[idx].last_pulsed_us = now + BUTTONS_PULSE_TIME_INITIAL_US_C;
			}
			else if ((now - buttons[idx].last_pulsed_us) > BUTTONS_PULSE_TIME_US_C)
			{
				g_buttons_pulsed |= btn;

				buttons[idx].last_pulsed_us = now;
			}

			g_buttons_down |= btn;
		}
		else
		{
			if (buttons[idx].down_last_frame)
			{
				g_buttons_released |= btn;
			}

			buttons[idx].last_pulsed_us = 0;
		}

		buttons[idx].down_last_frame = btn_is_down;
	}
}

/**
 * @brief Outputs the button's states
 * 
 * This function is only needed for debugging purposes
 * 
 * @return 	void 
 */
void buttons_print_states(void)
{
    if (g_buttons_pressed)
	{
        printf("pressed:  ");
        for (uint32_t bit = 1; bit; bit <<= 1)
		{
            if (g_buttons_pressed & bit) 
			{
				printf(" %s", p_buttons_get_name_string(bit));
			}
		}
        printf("\n");
    }

    if (g_buttons_released)
	{
        printf("released: ");
        for (uint32_t bit = 1; bit; bit <<= 1)
		{
            if (g_buttons_released & bit) 
			{
				printf(" %s", p_buttons_get_name_string(bit));
			}
		}
        printf("\n");
    }

    if (g_buttons_down)
	{
        printf("btn_is_down    : ");
        for (uint32_t bit = 1; bit; bit <<= 1)
		{
            if (g_buttons_down & bit)
			{
				printf(" %s", p_buttons_get_name_string(bit));
			}
		}
        printf("\n");
    }
}


/* Private functions ---------------------------------------------------------*/

/**
 * @brief 	Gets a button's name string
 * 
 * This function is only needed for debugging purposes
 * 
 * @param  	a_btn			Button mask
 * @return 	[const char*] 	Button name string
 */
static const char * p_buttons_get_name_string(BUTTONS_E a_btn)
{
    switch (a_btn)
	{
        case BUTTON_UP_C:
			return "UP";
		break;

        case BUTTON_DOWN_C:
			return "btn_is_down";
		break;

        case BUTTON_LEFT_C:
			return "LEFT";
		break;

        case BUTTON_RIGHT_C:
			return "RIGHT";
		break;

        case BUTTON_CENTER_C:
			return "CENTER";
		break;
        
		default: 
			return "?";
		break;
    }

	return "?";
}

/*** END OF FILE ***/
