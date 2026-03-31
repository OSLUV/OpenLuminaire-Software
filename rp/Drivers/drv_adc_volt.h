/**
 * @file      sense.h
 * @author    The OSLUV Project
 * @brief     Functions prototypes for voltages sensing driver
 *  
 */

#ifndef _D_SENSE_H_
#define _D_SENSE_H_


/* Exported variables --------------------------------------------------------*/

extern float g_adc_v_vbus, g_adc_v_12v, g_adc_v_24v;


/* Exported functions prototypes ---------------------------------------------*/

void adc_volt_init();
void adc_volt_update();


#endif /* _D_SENSE_H_ */

/*** END OF FILE ***/