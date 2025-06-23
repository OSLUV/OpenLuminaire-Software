#include <lvgl.h>

static lv_obj_t* screen;
static lv_obj_t* label;

void ui_loading_init()
{
	screen = lv_obj_create(NULL);
	label = lv_label_create(screen);
	lv_label_set_text(label, "Starting... 123");
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
