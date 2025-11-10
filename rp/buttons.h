/**
 * @file      buttons.h
 * @author    The OSLUV Project
 * @brief     Functions prototypes for system's buttons state monitoring driver
 *  
 */

#ifndef _D_BUTTONS_H_
#define _D_BUTTONS_H_


/* Exported typedef ----------------------------------------------------------*/

typedef enum {
	BUTTON_UP_C     = (1 << 0),
	BUTTON_DOWN_C   = (1 << 1),
	BUTTON_LEFT_C   = (1 << 2),
	BUTTON_RIGHT_C  = (1 << 3),
	BUTTON_CENTER_C = (1 << 4)
} BUTTONS_E;


/* Exported variables --------------------------------------------------------*/

extern BUTTONS_E g_buttons_pressed;
extern BUTTONS_E g_buttons_released;
extern BUTTONS_E g_buttons_down;
extern BUTTONS_E g_buttons_pulsed;


/* Exported functions --------------------------------------------------------*/

void buttons_init();
void buttons_update();
void buttons_print_states();


#endif /* _D_BUTTONS_H_ */

/*** END OF FILE ***/
