#include <lvgl.h>
#include "splash_img.h"  
#include "lamp.h"    
#include "display.h"   

static lv_obj_t* screen;
static lv_obj_t* label;

void ui_loading_init()
{
	screen = lv_obj_create(NULL);
	//label = lv_label_create(screen);
	//lv_label_set_text(label, "Starting... 123");
}

void ui_loading_update()
{
	// static int x = 0;
	// lv_label_set_text_fmt(label, "Starting... %d", x++);
	
}

void ui_loading_open()
{
	lv_screen_load(screen);
}

void display_splash_image(void)
{
    /* 1 ─ pick the bitmap ------------------------------------------------ */
    const lv_image_dsc_t *src = &splash_default_img;       /* fallback   */
    switch (get_lamp_type()) {
        case LAMP_TYPE_DIMMABLE:    src = &splash_dimmable_img; break;
        case LAMP_TYPE_NONDIMMABLE: src = &splash_basic_img;    break;
        default: /* UNKNOWN */                               break;
    }

    /* 2 ─ build a throw-away LVGL screen -------------------------------- */
    lv_obj_t *scr = lv_obj_create(NULL);      /* blank screen            */
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *img = lv_img_create(scr);       /* place the bitmap        */
    lv_img_set_src(img, src);
    lv_obj_center(img);

    /* 3 ─ make it visible ------------------------------------------------ */
    lv_screen_load(scr);                      /* swap to the new screen  */
    backlight_set_brightness(33);
    lv_timer_handler();                       /* flush once so it appears*/
}