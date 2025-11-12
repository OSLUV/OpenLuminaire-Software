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

static void back_btn_cb(lv_event_t * e)
{
    ui_main_open();
}

void ui_debug_init()
{
	screen = lv_obj_create(NULL);

    the_group = lv_group_create();

	label = lv_label_create(screen);
	//lv_label_set_text(label, "DEBUG - PRESS ANY KEY TO RETURN");
	lv_obj_remove_style(screen, NULL, LV_PART_SCROLLBAR);    /* kill scrollbar */
    lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);

    back_btn = lv_btn_create(screen);
    lv_obj_remove_style_all(back_btn);

    lv_obj_set_pos(back_btn, 10, 220);
    //lv_label_set_text(lv_label_create(back_btn), "< BACK");
    lv_obj_add_event_cb(back_btn, back_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_group_add_obj(the_group, back_btn);
}

char debug_label[512];

void ui_debug_update()
{
    char* w = debug_label;

    #define ADD_TEXT(...) w += lv_snprintf(w, sizeof(debug_label) - (w - debug_label) - 1, __VA_ARGS__)
	ADD_TEXT("DEBUG - PRESS CENTER \nBUTTON TO RETURN\n");
    ADD_TEXT("Lamp State: %s %dms\n", lamp_get_lamp_state_str(lamp_get_lamp_state()), lamp_get_state_elapsed_ms());

    LAMP_PWR_LEVEL_E rep;
    lamp_get_reported_power_level(&rep);
    ADD_TEXT("Lamp Req %s / Cmd %s\n", lamp_get_power_level_string(lamp_get_requested_power_level()), lamp_get_power_level_string(lamp_get_commanded_power_level()));
    ADD_TEXT("      Rep %s (%dHz)\n", lamp_get_power_level_string(rep), lamp_get_raw_freq());

    char* type_strs[] = {
        [LAMP_TYPE_UNKNOWN_C]      = "UNKNOWN",
        [LAMP_TYPE_DIMMABLE_C]     = "DIMMABLE",
        [LAMP_TYPE_NON_DIMMABLE_C] = "NONDIMMABLE"
    };
    ADD_TEXT("Lamp Type %s\n", type_strs[lamp_get_type()]);

    ADD_TEXT("IMU: %+.2f/%+.2f/%+.2f\n", g_imu_x, g_imu_y, g_imu_z);
    ADD_TEXT("Mag: %+ 5d/%+ 5d/%+ 5d\n", g_mag_x, g_mag_y, g_mag_z);
    ADD_TEXT("12V Switched %s / 24V Reg %s\n", lamp_get_switched_12v()?"ON ":"off", lamp_get_switched_24v()?"ON ":"off");
    ADD_TEXT("VBUS: %.1f/12V: %.1f/12V: %.1f/24V\n", sense_vbus, sense_12v, sense_24v);
    ADD_TEXT("USB %s %s/%.1fA\n", usbpd_get_is_trying_for_12v()?"Req 12V":"Req 5V", usbpd_get_is_12v()?"Got 12V":"Got 5V", ((float)usbpd_get_negotiated_mA())/1000.);

    RADAR_REPORT_T* r = radar_debug_get_report();
    int r_time = radar_debug_get_report_time();
    int dt = (time_us_64() - r_time)/(1000);
    ADD_TEXT("Radar: Ty%d dT% 8dms %s\n", r->type, dt, (dt>3000 || r_time == 0)?"STALE":"OK");
    ADD_TEXT("Radar: M: %dcm %de\n", r->report.moving_target_distance_cm, r->report.moving_target_energy);
    ADD_TEXT("Radar: S: %dcm %de\n", r->report.stationary_target_distance_cm, r->report.stationary_target_energy);
    ADD_TEXT("Radar: DD: %dcm / RD:%d\n", r->report.detection_distance_cm, radar_get_distance_cm());

    lv_label_set_text(label, debug_label);
}

void ui_debug_open()
{
	lv_screen_load(screen);
    display_set_indev_group(the_group);
    lv_group_focus_obj(back_btn);
}