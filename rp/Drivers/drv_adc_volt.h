/**
 * @file      drv_adc_volt.h
 * @author    The OSLUV Project
 * @brief     Functions prototypes for ADC voltages sensing driver
 *  
 */

#ifndef _D_ADC_VOLT_H_
#define _D_ADC_VOLT_H_


/* Exported variables --------------------------------------------------------*/

extern float g_adc_v_vbus, g_adc_v_12v, g_adc_v_24v;


/* Exported functions prototypes ---------------------------------------------*/

void drv_adc_volt_init();
void drv_adc_volt_update();


#endif /* _D_ADC_VOLT_H_ */

/*** END OF FILE ***/