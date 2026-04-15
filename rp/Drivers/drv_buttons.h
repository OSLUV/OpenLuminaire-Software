/**
 * @file      drv_buttons.h
 * @author    The OSLUV Project
 * @brief     Functions prototypes for system's buttons state monitoring driver
 *  
 */

#ifndef _D_BUTTONS_H_
#define _D_BUTTONS_H_


/* Exported typedef ----------------------------------------------------------*/

typedef enum {
	D_BUTTON_UP_C     = (1 << 0),
	D_BUTTON_DOWN_C   = (1 << 1),
	D_BUTTON_LEFT_C   = (1 << 2),
	D_BUTTON_RIGHT_C  = (1 << 3),
	D_BUTTON_CENTER_C = (1 << 4)
} D_BUTTONS_E;


/* Exported variables --------------------------------------------------------*/

extern D_BUTTONS_E g_drv_buttons_pressed;
extern D_BUTTONS_E g_drv_buttons_released;
extern D_BUTTONS_E g_drv_buttons_down;
extern D_BUTTONS_E g_drv_buttons_pulsed;


/* Exported functions prototypes ---------------------------------------------*/

void drv_buttons_init();
void drv_buttons_monitor();
void drv_buttons_print_states();


#endif /* _D_BUTTONS_H_ */

/*** END OF FILE ***/
