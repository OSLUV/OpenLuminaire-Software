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

int16_t mod_ui_main_lamp_set_stt(uint16_t req_state);
int16_t mod_ui_main_lamp_get_stt(uint16_t state);
int16_t mod_ui_main_lamp_set_dim(uint16_t level);
int16_t mod_ui_main_lamp_get_dim(uint16_t level);


#endif /* _M_UI_MAIN_H_ */

/*** END OF FILE ***/
