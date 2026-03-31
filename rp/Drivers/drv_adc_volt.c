/**
 * @file      drv_adc_volt.c
 * @author    The OSLUV Project
 * @brief     Driver for ADC voltages sensing
 * @schematic lamp_controller.SchDoc
 *  
 */


/* Includes ------------------------------------------------------------------*/

#include <hardware/adc.h>
#include "pins.h"


/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/

#define ADC_V_VBUS_PIN_C 	26 													/* VBUS_VSENSE */
#define ADC_V_12V_PIN_C 	27 													/* +12V_VSENSE */
#define ADC_V_24V_PIN_C 	29 													/* +24V_VSENSE */

#define ADC_V_VBUS_ADC_C	0
#define ADC_V_12V_ADC_C		1
#define ADC_V_24V_ADC_C		3


/* Global variables  ---------------------------------------------------------*/

float g_adc_v_vbus, g_adc_v_12v, g_adc_v_24v = 0;


/* Private variables  --------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/

static float adc_volt_convert_sample(uint16_t sample);


/* Exported functions --------------------------------------------------------*/

/**
 * @brief ADC voltages sensing initialization procedure
 * 
 * @return 	void  
 * 
 */
void adc_volt_init(void)
{
	adc_init();

    adc_gpio_init(ADC_V_VBUS_PIN_C);
    adc_gpio_init(ADC_V_12V_PIN_C);
    adc_gpio_init(ADC_V_24V_PIN_C);
}

/**
 * @brief   Updates voltages readings
 * 
 * @return 	void  
 */
void adc_volt_update(void)
{
	adc_select_input(ADC_V_VBUS_ADC_C);
	g_adc_v_vbus = adc_volt_convert_sample(adc_read());

	adc_select_input(ADC_V_12V_ADC_C);
	g_adc_v_12v = adc_volt_convert_sample(adc_read());

	adc_select_input(ADC_V_24V_ADC_C);
	g_adc_v_24v = adc_volt_convert_sample(adc_read());
}


/* Private functions ---------------------------------------------------------*/

/**
 * @brief Converts an ADC sample to a human readable voltage
 * 
 * @param adc_sample ADC channel sample
 * @return float 
 */
static float adc_volt_convert_sample(uint16_t adc_sample)
{
	float reading = ((float)adc_sample) * (3.3f / (float)(1 << 12));
	float r1 = 100000;
	float r2 = 10000;
	float voltage = (reading * (r1 + r2)) / r2;

	return voltage;
}

/*** END OF FILE ***/
