#include <lvgl.h>
#include "pico/stdlib.h"
#include "display.h"
#include "lamp.h"
#include "usbpd.h"
#include "sense.h"
#include "mag.h"
#include "imu.h"
#include "radar.h"
#include "ui_main.h"

static lv_obj_t* screen;
static lv_obj_t* label;
static lv_obj_t* back_btn;
static lv_group_t* the_group;

// extern const lv_font_t * FONT_BIG = NULL;
// FONT_BIG = &lv_font_montserrat_32;
static lv_style_t style_debug;
// static void back_btn_cb(lv_event_t * e)
// {
    // ui_main_open();
// }

void ui_debug_init()
{
	screen = lv_obj_create(NULL);

    the_group = lv_group_create();

	label = lv_label_create(screen);
	//lv_label_set_text(label, "DEBUG - PRESS ANY KEY TO RETURN");
	lv_obj_remove_style(screen, NULL, LV_PART_SCROLLBAR);    /* kill scrollbar */
    lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);

    // back_btn = lv_btn_create(screen);
    // lv_obj_remove_style_all(back_btn);

    // lv_obj_set_pos(back_btn, 10, 220);
    //lv_label_set_text(lv_label_create(back_btn), "< BACK");
    // lv_obj_add_event_cb(back_btn, back_btn_cb, LV_EVENT_CLICKED, NULL);
    // lv_group_add_obj(the_group, back_btn);
	
	lv_style_init(&style_debug);
	lv_style_set_text_font(&style_debug, &lv_font_montserrat_28);
}

char debug_label[512];

void ui_debug_update()
{
    char* w = debug_label;

    #define ADD_TEXT(...) w += lv_snprintf(w, sizeof(debug_label) - (w - debug_label) - 1, __VA_ARGS__)
	// ADD_TEXT("DEBUG - PRESS CENTER \nBUTTON TO RETURN\n");
    // ADD_TEXT("Lamp State: %s %dms\n", lamp_state_str(get_lamp_state()), get_lamp_state_elapsed_ms());

    // enum pwr_level rep;
    // get_lamp_reported_power(&rep);
    // ADD_TEXT("Lamp Req %s / Cmd %s\n", pwr_level_str(get_lamp_requested_power()), pwr_level_str(get_lamp_commanded_power()));
    // ADD_TEXT("      Rep %s (%dHz)\n", pwr_level_str(rep), get_lamp_raw_freq());

    // char* type_strs[] = {
        // [LAMP_TYPE_UNKNOWN] = "UNKNOWN",
        // [LAMP_TYPE_DIMMABLE] = "DIMMABLE",
        // [LAMP_TYPE_NONDIMMABLE] = "NONDIMMABLE"
    // };
    // ADD_TEXT("Lamp Type %s\n", type_strs[get_lamp_type()]);

    // ADD_TEXT("IMU: %+.2f/%+.2f/%+.2f\n", imu_x, imu_y, imu_z);
    // ADD_TEXT("Mag: %+ 5d/%+ 5d/%+ 5d\n", mag_x, mag_y, mag_z);
    // ADD_TEXT("12V Switched %s / 24V Reg %s\n", get_switched_12v()?"ON ":"off", get_switched_24v()?"ON ":"off");
    // ADD_TEXT("VBUS: %.1f/12V: %.1f/12V: %.1f/24V\n", sense_vbus, sense_12v, sense_24v);
    // ADD_TEXT("USB %s %s/%.1fA\n", usbpd_get_is_trying_for_12v()?"Req 12V":"Req 5V", usbpd_get_is_12v()?"Got 12V":"Got 5V", ((float)usbpd_get_negotiated_mA())/1000.);

    struct radar_report* r = debug_get_radar_report();
    int r_time = debug_get_radar_report_time();
    int dt = (time_us_64() - r_time)/(1000);
    ADD_TEXT("Ty%d dT% 8dms\n%s\n", r->type, dt, (dt>3000 || r_time == 0)?"STALE":"OK");
    ADD_TEXT("M: %dcm %de\n", r->report.moving_target_distance_cm, r->report.moving_target_energy);
    ADD_TEXT("S: %dcm %de\n", r->report.stationary_target_distance_cm, r->report.stationary_target_energy);
    ADD_TEXT("DD: %dcm\nRD: %d\n", r->report.detection_distance_cm, get_radar_distance_cm());

    lv_label_set_text(label, debug_label);
	lv_obj_add_style(label, &style_debug, 0);
}

void ui_debug_open()
{
	lv_screen_load(screen);
    display_set_indev_group(the_group);
    // lv_group_focus_obj(back_btn);
	lv_timer_handler();
}
