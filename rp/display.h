#include <lvgl.h>

void display_init();
void backlight_set_brightness(uint8_t brightness_percent);

void display_screen_off();
void display_screen_on();
void display_set_indev_group(lv_group_t* group);