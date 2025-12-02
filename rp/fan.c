/**
 * @file      fan.c
 * @author    The OSLUV Project
 * @brief     Driver for external fan control
 * @schematic lamp_controller.SchDoc
 *  
 */


/* Includes ------------------------------------------------------------------*/

#include <hardware/pwm.h>
#include <hardware/gpio.h>
#include "pins.h"


/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/

#define FAN_STEPCOUNT_C		1000


/* Global variables  ---------------------------------------------------------*/
/* Private variables  --------------------------------------------------------*/

static int fan_curr_speed = 0;


/* Private function prototypes -----------------------------------------------*/
/* Exported functions --------------------------------------------------------*/

/**
 * @brief External fan initialization procedure
 * 
 * @return 	void  
 * 
 */
void fan_init(void)
{
	gpio_set_function(PIN_FAN_PWM, GPIO_FUNC_PWM);
	uint slice_num = pwm_gpio_to_slice_num(PIN_FAN_PWM);

	pwm_config config = pwm_get_default_config();
    // Set divider, reduces counter clock to sysclock/this value
    pwm_config_set_clkdiv(&config, 125);

    pwm_config_set_wrap(&config, FAN_STEPCOUNT_C); // 1kHz

    // Load the configuration into our PWM slice, and set it running.
    pwm_init(slice_num, &config, false);

    pwm_set_gpio_level(PIN_FAN_PWM, 0);

    pwm_set_enabled(slice_num, true);
}

/**
 * @brief Sets fan speed
 * 
 * Speed must be a value between 0 and 100
 * 
 * @param speed 
 */
void fan_set_speed(int speed)
{
	if (speed > 100) 
	{
		speed = 100;
	}
	if (speed < 0) 
	{
		speed = 0;
	}
	
	pwm_set_gpio_level(PIN_FAN_PWM, speed * FAN_STEPCOUNT_C / 100);

	fan_curr_speed = speed;
}

/**
 * @brief Returns the current fan speed
 * 
 * @return int [0 - 100]
 */
int fan_get_speed(void)
{
	return fan_curr_speed;
}


/* Private functions ---------------------------------------------------------*/

/*** END OF FILE ***/

