/**
 * @file      mod_ui_scrn_main.h
 * @author    The OSLUV Project
 * @brief     Functions prototypes for UI main screen handling module
 *  
 */

#ifndef _M_UI_MAIN_H_
#define _M_UI_MAIN_H_

/* Exported defines ----------------------------------------------------------*/

#define UI_MAIN_MAX_DIM_LEVELS_C    4
#define UI_MAIN_MAX_DIM_INDEX_C     (UI_MAIN_MAX_DIM_LEVELS_C - 1)


/* Exported functions prototypes ---------------------------------------------*/

void mod_ui_main_init(void);
void mod_ui_main_handler(void);
void mod_ui_main_open(void);


#endif /* _M_UI_MAIN_H_ */

/*** END OF FILE ***/
