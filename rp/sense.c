/**
 * @file      sense.c
 * @author    The OSLUV Project
 * @brief     Driver for voltages sensing
 * @schematic lamp_controller.SchDoc
 *  
 */


/* Includes ------------------------------------------------------------------*/

#include <hardware/adc.h>
#include "pins.h"


/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/

#define SENSE_PIN_ADC0_C	26


/* Global variables  ---------------------------------------------------------*/

float g_sense_vbus, g_sense_12v, g_sense_24v = 0;


/* Private variables  --------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/

static float sense_convert_adc_sample(uint16_t sample);


/* Exported functions --------------------------------------------------------*/

/**
 * @brief Voltages sensing initialization procedure
 * 
 * @return 	void  
 * 
 */
void sense_init(void)
{
	adc_init();

    adc_gpio_init(PIN_VSENSE_VBUS);
    adc_gpio_init(PIN_VSENSE_12V);
    adc_gpio_init(PIN_VSENSE_24V);
}

/**
 * @brief   Updates voltages readings
 * 
 * @return 	void  
 */
void sense_update(void)
{
	adc_select_input(PIN_VSENSE_VBUS - SENSE_PIN_ADC0_C);
	g_sense_vbus = sense_convert_adc_sample(adc_read());

	adc_select_input(PIN_VSENSE_12V - SENSE_PIN_ADC0_C);
	g_sense_12v = sense_convert_adc_sample(adc_read());

	adc_select_input(PIN_VSENSE_24V - SENSE_PIN_ADC0_C);
	g_sense_24v = sense_convert_adc_sample(adc_read());
}


/* Private functions ---------------------------------------------------------*/

/**
 * @brief Converts an ADC sample to a human readable voltage
 * 
 * @param adc_sample ADC channel sample
 * @return float 
 */
static float sense_convert_adc_sample(uint16_t adc_sample)
{
	float reading = ((float)adc_sample) * (3.3f / (float)(1 << 12));
	float r1 = 100000;
	float r2 = 10000;
	float voltage = (reading * (r1 + r2)) / r2;

	return voltage;
}

/*** END OF FILE ***/
