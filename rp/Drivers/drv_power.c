/**
 * @file      drv_power.c
 * @author    The OSLUV Project
 * @brief     Driver for lamp control
 * @schematic lamp_controller.SchDoc
 * @schematic power.SchDoc
 *  
 */


/* Includes ------------------------------------------------------------------*/

#include <hardware/gpio.h>
#include <hardware/pwm.h>
#include "Drivers/drv_power.h"


/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/

#define D_PWR_DBG_ID_STR_C        	"drv_power           "
#define D_PWR_DBG_PRINTF(...)    	debug_print_f(__VA_ARGS__)
#define D_PWR_DBG_PRINT_TXT(...)    debug_print_mod_f(D_PWR_DBG_ID_STR_C, __VA_ARGS__)
#define D_PWR_DBG_PRINT_ERR(...)	debug_print_err(D_PWR_DBG_ID_STR_C, __VA_ARGS__)
#define D_PWR_DBG_PRINT_WRN(...)	debug_print_warn(D_PWR_DBG_ID_STR_C, __VA_ARGS__)
#define D_PWR_DBG_PRINT_OK(...)		debug_print_ok(D_PWR_DBG_ID_STR_C, __VA_ARGS__)

#define D_PWR_EN_12V_PIN_C 			7											/* +12V_ENABLE */
#define D_PWR_EN_24V_PIN_C 			15											/* +24V_ENABLE */

#define D_PWR_STEPCOUNT_SOFTSTART_C 64


/* Global variables  ---------------------------------------------------------*/
/* Private variables  --------------------------------------------------------*/
/* Callback prototypes -------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Exported functions --------------------------------------------------------*/

/**
 * @brief 	Lamp control initialization procedure
 * 
 * @return 	void 
 */

void drv_power_init(void)
{
	uint slice_num;
	pwm_config pwm_cfg;

	gpio_init(D_PWR_EN_24V_PIN_C);
	gpio_set_dir(D_PWR_EN_24V_PIN_C, GPIO_OUT);
	gpio_put(D_PWR_EN_24V_PIN_C, false);
	
	gpio_set_function(D_PWR_EN_12V_PIN_C, GPIO_FUNC_PWM);
	slice_num = pwm_gpio_to_slice_num(D_PWR_EN_12V_PIN_C);

	pwm_cfg = pwm_get_default_config();
	// Set divider, reduces counter clock to sysclock/this value
	pwm_config_set_clkdiv(&pwm_cfg, 8);

	pwm_config_set_wrap(&pwm_cfg, D_PWR_STEPCOUNT_SOFTSTART_C); 				// ~244kHz

	// Load the configuration into our PWM slice, and set it running.
	pwm_init(slice_num, &pwm_cfg, false);

	pwm_set_gpio_level(D_PWR_EN_12V_PIN_C, 0);

	pwm_set_enabled(slice_num, true);
}

/**
 * @brief Enables/disables Switched 12V Enable pin
 *
 * Ramps PWM up on enable, ramps down on disable. No precondition checks —
 * callers (board_power_up) are responsible for correct ordering and
 * voltage verification.
 *
 * @param b_on
 *
 * @return 	void
 */
void drv_power_set_switched_12v_level(uint16_t level)
{
	pwm_set_gpio_level(D_PWR_EN_12V_PIN_C, level);
}

/**
 * @brief Enables/disables Switched 24V Enable pin
 *
 * Sets GPIO and flag. No precondition checks — callers (board_power_up)
 * are responsible for correct ordering and voltage verification.
 *
 * @param b_on
 *
 * @return 	void
 */
void drv_power_set_switched_24v(bool b_on)
{
	gpio_put(D_PWR_EN_24V_PIN_C, b_on);
}


/* Callback functions --------------------------------------------------------*/
/* Private functions ---------------------------------------------------------*/


/*** END OF FILE ***/