/**
 * @file      drv_lamp.c
 * @author    The OSLUV Project
 * @brief     Driver for lamp control
 * @schematic lamp_controller.SchDoc
 * @schematic power.SchDoc
 *  
 */


/* Includes ------------------------------------------------------------------*/

#include <stdio.h>
#include <pico/stdlib.h>
#include "pico/time.h"
#include <hardware/gpio.h>
#include <hardware/pwm.h>
#include "Drivers/drv_lamp.h"


/* Private typedef -----------------------------------------------------------*/



/* Private define ------------------------------------------------------------*/

#define D_LAMP_DBG_ID_STR_C        	  		"drv_lamp            "
#define D_LAMP_DBG_PRINTF(...)    	  		debug_print_f(__VA_ARGS__)
#define D_LAMP_DBG_PRINT_TXT(...)    		debug_print_mod_f(D_LAMP_DBG_ID_STR_C, __VA_ARGS__)
#define D_LAMP_DBG_PRINT_ERR(...)			debug_print_err(D_LAMP_DBG_ID_STR_C, __VA_ARGS__)
#define D_LAMP_DBG_PRINT_WRN(...)			debug_print_warn(D_LAMP_DBG_ID_STR_C, __VA_ARGS__)
#define D_LAMP_DBG_PRINT_OK(...)			debug_print_ok(D_LAMP_DBG_ID_STR_C, __VA_ARGS__)

#define D_LAMP_ENABLE_PIN_C 				14 									/* LAMP_ENABLE */
#define D_LAMP_STATUS_PIN_C 				12 									/* LAMP_STATUS */
#define D_LAMP_PWM_PIN_C 					13 									/* LAMP_PWM */

#define D_LAMP_STEPCOUNT_DIMMING_C  		100


/* Global variables  ---------------------------------------------------------*/
/* Private variables  --------------------------------------------------------*/

static uint16_t				drv_lamp_last_pwm_level;

static bool					b_drv_lamp_is_enabled;
static bool					drv_lamp_is_events_ctr_paused;
volatile uint32_t			drv_lamp_status_events_ctr;


/* Callback prototypes -------------------------------------------------------*/

void drv_lamp_status_gpio_callback(uint gpio, uint32_t events);


/* Private function prototypes -----------------------------------------------*/

/* Exported functions --------------------------------------------------------*/

/**
 * @brief 	Lamp control initialization procedure
 * 
 * @return 	void 
 */

void drv_lamp_init(void)
{
	uint slice_num;
	pwm_config pwm_cfg;

	b_drv_lamp_is_enabled = false;
	drv_lamp_is_events_ctr_paused = false;
	drv_lamp_status_events_ctr = 0;

	gpio_init(D_LAMP_ENABLE_PIN_C);
	gpio_set_dir(D_LAMP_ENABLE_PIN_C, GPIO_OUT);
	gpio_put(D_LAMP_ENABLE_PIN_C, false);

	gpio_init(D_LAMP_STATUS_PIN_C);
	gpio_set_dir(D_LAMP_STATUS_PIN_C, GPIO_IN);
	gpio_set_pulls(D_LAMP_STATUS_PIN_C, true, false);

	gpio_set_irq_callback(drv_lamp_status_gpio_callback);
    gpio_set_irq_enabled(D_LAMP_STATUS_PIN_C, GPIO_IRQ_EDGE_RISE, true);
    irq_set_enabled(IO_IRQ_BANK0, true);

	gpio_set_function(D_LAMP_PWM_PIN_C, GPIO_FUNC_PWM);
	slice_num = pwm_gpio_to_slice_num(D_LAMP_PWM_PIN_C);

	pwm_cfg = pwm_get_default_config();

	// Set divider, reduces counter clock to sysclock/this value
	pwm_config_set_clkdiv(&pwm_cfg, 8);

	pwm_config_set_wrap(&pwm_cfg, D_LAMP_STEPCOUNT_DIMMING_C - 1); // 244kHz

	// Load the configuration into our PWM slice, and set it running.
	pwm_init(slice_num, &pwm_cfg, false);

	drv_lamp_last_pwm_level = 0xFFFF;
	drv_lamp_set_pwm_level(0);

	pwm_set_enabled(slice_num, true);
}

/**
 * @brief Sets LAMP_ENABLE pin to 1
 * 
 */
void drv_lamp_enable(void)
{
	b_drv_lamp_is_enabled = true;

	gpio_put(D_LAMP_ENABLE_PIN_C, 1);
}

/**
 * @brief Sets LAMP_ENABLE pin to 0
 * 
 */
void drv_lamp_disable(void)
{
	b_drv_lamp_is_enabled = false;

	gpio_put(D_LAMP_ENABLE_PIN_C, 0);
}

/**
 * @brief Returns if lamp should be enabled  by the last enable/disable received
 * 
 */
bool drv_lamp_is_enabled(void)
{
	return b_drv_lamp_is_enabled;
}

/**
 * @brief Sets PWM level
 * 
 * @param level 
 */
void drv_lamp_set_pwm_level(uint16_t level)
{
	if (level != drv_lamp_last_pwm_level)
	{
		pwm_set_gpio_level(D_LAMP_PWM_PIN_C, level);

		drv_lamp_last_pwm_level = level;
	}
}

/**
 * @brief Returns LAMP_STATUS pin state
 * 
 * @return true 
 * @return false 
 */
bool drv_lamp_is_status_on(void)
{
	return gpio_get(D_LAMP_STATUS_PIN_C);
}

/**
 * @brief Returns current status pin raise events counter and resets the counter
 * 
 * @return uint32_t 
 */
uint32_t drv_lamp_get_status_evts_count(void)
{
	uint32_t count;

	drv_lamp_is_events_ctr_paused = true;

	count = drv_lamp_status_events_ctr;
	drv_lamp_status_events_ctr = 0;

	drv_lamp_is_events_ctr_paused = false;

	return count;
}


/* Callback functions --------------------------------------------------------*/

/**
 * @brief Callback for lamp status ISR
 * 
 * Counts the status events
 * 
 * @param a_gpio 
 * @param a_events 
 * e
 * @return 	void
 */
void drv_lamp_status_gpio_callback(uint gpio, uint32_t events)
{
	if (gpio == D_LAMP_STATUS_PIN_C)
	{
		if (!drv_lamp_is_events_ctr_paused)
		{
			drv_lamp_status_events_ctr++;
		}
	}
}


/* Private functions ---------------------------------------------------------*/


/*** END OF FILE ***/