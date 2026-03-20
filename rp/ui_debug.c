/**
 * @file      ui_debug.c
 * @author    The OSLUV Project
 * @brief     UI debugging tools
 *
 */


/* Includes ------------------------------------------------------------------*/

#include <stdio.h>
#include <lvgl.h>
#include "pico/stdlib.h"
#include <hardware/watchdog.h>
#include "ui_debug.h"
#include "display.h"
#include "lamp.h"
#include "usbpd.h"
#include "sense.h"
#include "mag.h"
#include "imu.h"
#include "radar.h"
#include "ui_main.h"


/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Global variables  ---------------------------------------------------------*/
/* Private variables  --------------------------------------------------------*/

static lv_obj_t*    ui_debug_screen;
static lv_obj_t*    ui_debug_label;
static lv_obj_t*    ui_debug_back_btn;
static lv_obj_t*    ui_debug_retest_btn;
static lv_group_t*  ui_debug_group;
static char         ui_debug_label_txt[512];


/* Callback prototypes -------------------------------------------------------*/

static void ui_debug_back_btn_callback(lv_event_t* p_evt);
static void ui_debug_retest_btn_callback(lv_event_t* p_evt);
static void ui_debug_key_callback(lv_event_t* p_evt);


/* Private function prototypes -----------------------------------------------*/
/* Exported functions --------------------------------------------------------*/

/**
 * @brief UI debug initialization procedure
 *
 */
void ui_debug_init(void)
{
	ui_debug_screen = lv_obj_create(NULL);
	lv_obj_set_style_bg_color(ui_debug_screen, lv_color_black(), 0);
	lv_obj_remove_style(ui_debug_screen, NULL, LV_PART_SCROLLBAR);
	lv_obj_set_scrollbar_mode(ui_debug_screen, LV_SCROLLBAR_MODE_OFF);
	lv_obj_clear_flag(ui_debug_screen, LV_OBJ_FLAG_SCROLLABLE);

    ui_debug_group = lv_group_create();

	ui_debug_label = lv_label_create(ui_debug_screen);
	lv_obj_set_style_text_color(ui_debug_label, lv_color_white(), 0);

    // BACK button
    ui_debug_back_btn = lv_btn_create(ui_debug_screen);
    lv_obj_remove_style_all(ui_debug_back_btn);
    lv_obj_set_pos(ui_debug_back_btn, 10, 220);
    lv_label_set_text(lv_label_create(ui_debug_back_btn), "BACK");
    lv_obj_set_style_text_color(ui_debug_back_btn, lv_color_white(), 0);
    lv_obj_set_style_bg_color(ui_debug_back_btn, lv_color_white(), LV_STATE_FOCUSED);
    lv_obj_set_style_bg_opa(ui_debug_back_btn, LV_OPA_COVER, LV_STATE_FOCUSED);
    lv_obj_set_style_text_color(ui_debug_back_btn, lv_color_black(), LV_STATE_FOCUSED);
    lv_obj_set_style_pad_all(ui_debug_back_btn, 2, 0);
    lv_obj_set_style_radius(ui_debug_back_btn, 4, 0);
    lv_obj_add_event_cb(ui_debug_back_btn,
                        ui_debug_back_btn_callback,
                        LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_debug_back_btn,
                        ui_debug_key_callback,
                        LV_EVENT_KEY, NULL);
    lv_group_add_obj(ui_debug_group, ui_debug_back_btn);

    // RETEST button
    ui_debug_retest_btn = lv_btn_create(ui_debug_screen);
    lv_obj_remove_style_all(ui_debug_retest_btn);
    lv_obj_set_pos(ui_debug_retest_btn, 150, 220);
    lv_label_set_text(lv_label_create(ui_debug_retest_btn), "RETEST");
    lv_obj_set_style_text_color(ui_debug_retest_btn, lv_color_white(), 0);
    lv_obj_set_style_bg_color(ui_debug_retest_btn, lv_color_white(), LV_STATE_FOCUSED);
    lv_obj_set_style_bg_opa(ui_debug_retest_btn, LV_OPA_COVER, LV_STATE_FOCUSED);
    lv_obj_set_style_text_color(ui_debug_retest_btn, lv_color_black(), LV_STATE_FOCUSED);
    lv_obj_set_style_pad_all(ui_debug_retest_btn, 2, 0);
    lv_obj_set_style_radius(ui_debug_retest_btn, 4, 0);
    lv_obj_add_event_cb(ui_debug_retest_btn,
                        ui_debug_retest_btn_callback,
                        LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_debug_retest_btn,
                        ui_debug_key_callback,
                        LV_EVENT_KEY, NULL);
    lv_group_add_obj(ui_debug_group, ui_debug_retest_btn);
}

/**
 * @brief Updates UI debug information
 *
 */
void ui_debug_update(void)
{
    char* w = ui_debug_label_txt;

    #define ADD_TEXT(...) w += lv_snprintf(w, sizeof(ui_debug_label_txt) - \
                               (w - ui_debug_label_txt) - 1, __VA_ARGS__)

    ADD_TEXT("Lamp State: %s %dms\n",
             lamp_get_lamp_state_str(lamp_get_lamp_state()),
             lamp_get_state_elapsed_ms());

    LAMP_PWR_LEVEL_E rep;

    lamp_get_reported_power_level(&rep);

    ADD_TEXT("Lamp Req %s / Cmd %s\n",
             lamp_get_power_level_string(lamp_get_requested_power_level()),
             lamp_get_power_level_string(lamp_get_commanded_power_level()));

    ADD_TEXT("     Rep %s (%dHz)\n",
             lamp_get_power_level_string(rep),
             lamp_get_raw_freq());

    char* type_strs[] = {
        [LAMP_TYPE_UNKNOWN_C]      = "UNKNOWN",
        [LAMP_TYPE_DIMMABLE_C]     = "DIMMABLE",
        [LAMP_TYPE_NON_DIMMABLE_C] = "NONDIMMABLE"
    };

    ADD_TEXT("Lamp Type %s\n", type_strs[lamp_get_type()]);

    ADD_TEXT("IMU: %+.2f/%+.2f/%+.2f\n", g_imu_x, g_imu_y, g_imu_z);

    ADD_TEXT("Mag: %+ 5d/%+ 5d/%+ 5d\n", g_mag_x, g_mag_y, g_mag_z);

    ADD_TEXT("12V Switched %s / 24V Reg %s\n",
             lamp_get_switched_12v()?"ON ":"off",
             lamp_get_switched_24v()?"ON ":"off");

    ADD_TEXT("VBUS: %.1f/12V: %.1f/12V: %.1f/24V\n",
             g_sense_vbus,
             g_sense_12v,
             g_sense_24v);

    ADD_TEXT("USB %s %s/%.1fA\n",
             usbpd_get_is_trying_for_12v()?"Req 12V":"Req 5V",
             usbpd_get_is_12v()?"Got 12V":"Got 5V",
             ((float)usbpd_get_negotiated_mA())/1000.);

    RADAR_REPORT_T* r      = radar_debug_get_report();
    int             r_time = radar_debug_get_report_time();
    int             dt     = (time_us_64() - r_time)/(1000);

    ADD_TEXT("Radar: Ty%d dT% 8dms %s\n",
             r->type, dt, (dt>3000 || r_time == 0)?"STALE":"OK");

    ADD_TEXT("Radar: M: %dcm %de\n",
             r->report.moving_target_distance_cm,
             r->report.moving_target_energy);

    ADD_TEXT("Radar: S: %dcm %de\n",
             r->report.stationary_target_distance_cm,
             r->report.stationary_target_energy);

    ADD_TEXT("Radar: DD: %dcm / RD:%d\n",
             r->report.detection_distance_cm,
             radar_get_distance_cm());

    lv_label_set_text(ui_debug_label, ui_debug_label_txt);
}

/**
 * @brief Display debug screen
 *
 */
void ui_debug_open(void)
{
	lv_screen_load(ui_debug_screen);
    display_set_indev_group(ui_debug_group);
    lv_group_focus_obj(ui_debug_back_btn);
}

/* Callback functions --------------------------------------------------------*/

/**
 * @brief Callback function for back button click event
 *
 * @param p_evt
 */
static void ui_debug_back_btn_callback(lv_event_t* p_evt)
{
    ui_main_open();
}

/**
 * @brief Callback for RETEST button — clears lamp type, re-runs detection, reboots.
 *
 * @param p_evt
 */
static void ui_debug_retest_btn_callback(lv_event_t* p_evt)
{
    printf("[V1.2] Retest requested from debug menu\n");

    lv_label_set_text(ui_debug_label, "RETESTING...\n\nLamp will turn off,\nthen board will\nreboot when done.");
    lv_obj_invalidate(ui_debug_screen);
    lv_refr_now(NULL);

    lamp_request_power_level(LAMP_PWR_OFF_C);
    lamp_update();
    lamp_update();
    sleep_ms(100);

    // Refresh ADC readings before the type test — stale values from the main
    // loop may be out of the 12V pre-check range (11.5-12.5V) due to load droop
    // while the lamp was running, which causes lamp_set_switched_12v(true) to
    // silently reject and the type test to hang in an infinite loop.
    sense_update();

    lamp_reset_type();
    lamp_perform_type_test();

    printf("[V1.2] Retest complete, type=%d. Rebooting to apply new UI layout...\n",
           lamp_get_type());

    watchdog_reboot(0, 0, 0);
}

/**
 * @brief Key handler for debug buttons — LEFT/RIGHT navigate between buttons
 */
static void ui_debug_key_callback(lv_event_t* p_evt)
{
    uint32_t key = lv_event_get_key(p_evt);
    if (key == LV_KEY_LEFT)
        lv_group_focus_prev(ui_debug_group);
    else if (key == LV_KEY_RIGHT)
        lv_group_focus_next(ui_debug_group);
}


/* Private functions ---------------------------------------------------------*/

/*** END OF FILE ***/
