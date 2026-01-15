

#ifndef _M_UI_MAIN_H_
#define _M_UI_MAIN_H_

#define UI_MAIN_MAX_DIM_LEVELS_C    4
#define UI_MAIN_MAX_DIM_INDEX_C     (UI_MAIN_MAX_DIM_LEVELS_C - 1)

void ui_main_init();
void ui_main_update();
void ui_main_open();

int16_t ui_main_lamp_set_stt(uint16_t req_state);
int16_t ui_main_lamp_get_stt(uint16_t state);
int16_t ui_main_lamp_set_dim(uint16_t level);
int16_t ui_main_lamp_get_dim(uint16_t level);


#endif /* _M_UI_MAIN_H_ */

/*** END OF FILE ***/
