/**
 * @file      ui_loading.h
 * @author    The OSLUV Project
 * @brief     Functions prototypes for UI loading module
 *  
 */

#ifndef _UI_LOADING_H_
#define _UI_LOADING_H_

/* Exported functions prototypes ---------------------------------------------*/

void ui_loading_init(void);
void ui_loading_update(void);
void ui_loading_open(void);
void ui_loading_splash_image_init(void);
void ui_loading_splash_image_open(lv_event_cb_t on_exit_cb);
void ui_loading_show_psu(void);


#endif /* _UI_LOADING_H_ */

/*** END OF FILE ***/