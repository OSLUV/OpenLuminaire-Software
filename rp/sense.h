/**
 * @file      sense.h
 * @author    The OSLUV Project
 * @brief     Functions prototypes for voltages sensing driver
 *  
 */

#ifndef _D_SENSE_H_
#define _D_SENSE_H_


/* Exported variables --------------------------------------------------------*/

extern float g_sense_vbus, g_sense_12v, g_sense_24v;


/* Exported functions prototypes ---------------------------------------------*/

void sense_init();
void sense_update();


#endif /* _D_SENSE_H_ */

/*** END OF FILE ***/