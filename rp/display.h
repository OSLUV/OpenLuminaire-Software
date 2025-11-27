/**
 * @file      display.h
 * @author    The OSLUV Project
 * @brief     Functions prototypes for display driver
 *  
 */

#ifndef _D_DISPLAY_H_
#define _D_DISPLAY_H_


/* Exported includes ---------------------------------------------------------*/

#include <lvgl.h>


/* Exported functions prototypes ---------------------------------------------*/

void display_init(void);
void display_set_backlight_brightness(uint8_t brightness);

void display_screen_off(void);
void display_screen_on(void);
void display_set_indev_group(lv_group_t* p_group);


#endif /* _D_DISPLAY_H_ */

/*** END OF FILE ***/