#include <lvgl.h>
#include <stdio.h>
#include "display.h"

static lv_obj_t* screen;

void ui_main_init()
{
	screen = lv_obj_create(NULL);
	
	{
		lv_obj_t* button = lv_button_create(screen);
		lv_obj_set_size(button, 80, 80);
	    lv_obj_set_pos(button, 10, 10);
	    // lv_obj_add_event_cb(button, avg_event, LV_EVENT_CLICKED, NULL);

	    lv_obj_t* label = lv_label_create(button);
	    lv_label_set_text(label, "ABC");
	    lv_group_add_obj(the_group, button);
	}

	{
		lv_obj_t* button = lv_button_create(screen);
		lv_obj_set_size(button, 80, 80);
	    lv_obj_set_pos(button, 10, 100);
	    // lv_obj_add_event_cb(button, avg_event, LV_EVENT_CLICKED, NULL);

	    lv_obj_t* label = lv_label_create(button);
	    lv_label_set_text(label, "DEF");
	    lv_group_add_obj(the_group, button);
	}
}

void ui_main_update()
{
	
}

void ui_main_open()
{
	printf("Open ui_main\n");
	lv_screen_load(screen);
}
