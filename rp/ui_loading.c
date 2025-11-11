#include <lvgl.h>
#include "splash_img.h"  
#include "lamp.h"    
#include "display.h"   

static lv_group_t* the_group;
static lv_obj_t* splash_screen;
static lv_obj_t * catch;
static lv_obj_t* label;
static lv_event_cb_t exit_cb   = NULL;      // caller-supplied

// internal handler – runs on FIRST key / click
static void splash_event_cb(lv_event_t * e)
{
    if(exit_cb) exit_cb(e);                  // hand back control
}


void ui_loading_init()
{
	splash_screen = lv_obj_create(NULL);
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
	lv_screen_load(splash_screen);
}


void splash_image_init()
{
	the_group = lv_group_create();
    /* 1 ─ pick the bitmap ------------------------------------------------ */
    const lv_image_dsc_t *src = &splash_default_img;       /* fallback   */
    switch (lamp_get_type()) {
        case LAMP_TYPE_DIMMABLE_C:    src = &splash_dimmable_img; break;
        case LAMP_TYPE_NON_DIMMABLE_C: src = &splash_basic_img;    break;
        default: /* UNKNOWN */                               break;
    }

    /* 2 ─ build a throw-away LVGL screen -------------------------------- */
    splash_screen = lv_obj_create(NULL);      /* blank screen            */
    lv_obj_clear_flag(splash_screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *img = lv_img_create(splash_screen);       /* place the bitmap        */
    lv_img_set_src(img, src);
    lv_obj_center(img);
	// listen for ANY key / click / encoder turn
    //lv_obj_add_event_cb(splash_screen, splash_event_cb, LV_EVENT_ALL, NULL);
	catch = lv_btn_create(splash_screen);         // transparent button
	lv_obj_remove_style_all(catch);
	lv_obj_set_size(catch, LV_PCT(100), LV_PCT(100));     // cover whole screen
	lv_obj_set_style_bg_opa(catch, LV_OPA_TRANSP, 0);     // invisible
	
	
	lv_obj_add_event_cb(catch, splash_event_cb, LV_EVENT_KEY, NULL);
	lv_group_add_obj(the_group, catch);
	
	
}

void splash_image_open(lv_event_cb_t on_exit) 
{
	exit_cb = on_exit;
    lv_screen_load(splash_screen);                      /* swap to the new screen  */
    backlight_set_brightness(33);    
	
	display_set_indev_group(the_group);
	lv_group_focus_obj(catch); // ensure it has focus
	
	lv_timer_handler();                       /* flush once so it appears*/
}


static lv_obj_t * scr_psu = NULL;
void ui_psu_show(void)            // call when the PSU is bad
{
    if(scr_psu) { lv_scr_load(scr_psu); return; }

    scr_psu = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_psu, lv_color_black(), 0);
    lv_obj_clear_flag(scr_psu, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * txt = lv_label_create(scr_psu);
    lv_label_set_text(txt,
                      "ERROR:\n\nPOWER SUPPLY\n"
                      "INCOMPATIBLE!\n\n"
                      "Needs  12 V | 2.5 A");
    lv_obj_set_style_text_color(txt, lv_color_white(), 0);
    lv_obj_set_style_text_align(txt, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(txt, &lv_font_montserrat_24, 0);
    lv_obj_center(txt);

    lv_scr_load(scr_psu);             // make it active immediately
	lv_timer_handler(); //
}