#include <hardware/adc.h>
#include "pins.h"

#define PIN_ADC0 26


float sense_vbus, sense_12v, sense_24v = 0;

void init_sense()
{
	adc_init();

    adc_gpio_init(PIN_VSENSE_VBUS);
    adc_gpio_init(PIN_VSENSE_12V);
    adc_gpio_init(PIN_VSENSE_24V);
}

float convert(uint16_t sample)
{
	float reading = ((float)sample) * (3.3f / (float)(1 << 12));
	float r1 = 100000;
	float r2 = 10000;
	float voltage = (reading * (r1 + r2)) / r2;
	return voltage;
}

void update_sense()
{
	adc_select_input(PIN_VSENSE_VBUS - PIN_ADC0);
	sense_vbus = convert(adc_read());

	adc_select_input(PIN_VSENSE_12V - PIN_ADC0);
	sense_12v = convert(adc_read());

	adc_select_input(PIN_VSENSE_24V - PIN_ADC0);
	sense_24v = convert(adc_read());
}
