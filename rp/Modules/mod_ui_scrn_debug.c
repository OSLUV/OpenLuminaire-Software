/**
 * @file      mod_ui_scrn_debug.c
 * @author    The OSLUV Project
 * @brief     UI debugging tools
 *
 */


/* Includes ------------------------------------------------------------------*/

#include <stdio.h>
#include <lvgl.h>
#include "pico/stdlib.h"
#include <hardware/watchdog.h>
#include "Modules/mod_ui_scrn_debug.h"
#include "Modules/mod_ui_screens.h"
#include "Modules/system.h"
#include "Drivers/drv_adc_volt.h"
#include "Drivers/drv_debug.h"
#include "Drivers/drv_display.h"
#include "Drivers/drv_radar.h"
#include "Drivers/drv_usb_pd.h"


/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/

#define M_UI_SCRN_DBG_DBG_ID_STR_C        	"mod_ui_scrn_dbg     "
#define M_UI_SCRN_DBG_DBG_PRINTF(...)    	debug_print_f(__VA_ARGS__)
#define M_UI_SCRN_DBG_DBG_PRINT_TXT(...)    debug_print_mod_f(M_UI_SCRN_DBG_DBG_ID_STR_C, __VA_ARGS__)
#define M_UI_SCRN_DBG_DBG_PRINT_ERR(...)	debug_print_err(M_UI_SCRN_DBG_DBG_ID_STR_C, __VA_ARGS__)
#define M_UI_SCRN_DBG_DBG_PRINT_WRN(...)	debug_print_warn(M_UI_SCRN_DBG_DBG_ID_STR_C, __VA_ARGS__)
#define M_UI_SCRN_DBG_DBG_PRINT_OK(...)		debug_print_ok(M_UI_SCRN_DBG_DBG_ID_STR_C, __VA_ARGS__)


/* Global variables  ---------------------------------------------------------*/

extern M_UI_SCRN_E  g_mod_ui_new_screen;
extern bool         g_mod_pow_is_usb_connected_b;
extern uint32_t     g_mod_pow_usb_negotiated_ma;
extern uint32_t     g_mod_pow_usb_negotiated_ma;
extern uint32_t     g_mod_pow_usb_negotiated_mv;


/* Private variables  --------------------------------------------------------*/

static lv_obj_t*    mod_ui_debug_screen;
static lv_obj_t*    mod_ui_debug_label;
static lv_obj_t*    mod_ui_debug_back_btn;
static lv_obj_t*    mod_ui_debug_retest_btn;
static lv_group_t*  mod_ui_debug_group;
static char         mod_ui_debug_label_txt[512];
static bool         b_mod_ui_debug_is_retesting;


/* Callback prototypes -------------------------------------------------------*/

static void mod_ui_debug_back_btn_callback(lv_event_t* p_evt);
static void mod_ui_debug_retest_btn_callback(lv_event_t* p_evt);
static void mod_ui_debug_key_callback(lv_event_t* p_evt);


/* Private function prototypes -----------------------------------------------*/
/* Exported functions --------------------------------------------------------*/

/**
 * @brief UI debug initialization procedure
 *
 */
void mod_ui_debug_init(void)
{
    b_mod_ui_debug_is_retesting = false;

	mod_ui_debug_screen = lv_obj_create(NULL);
	lv_obj_set_style_bg_color(mod_ui_debug_screen, lv_color_black(), 0);
	lv_obj_remove_style(mod_ui_debug_screen, NULL, LV_PART_SCROLLBAR);
	lv_obj_set_scrollbar_mode(mod_ui_debug_screen, LV_SCROLLBAR_MODE_OFF);
	lv_obj_clear_flag(mod_ui_debug_screen, LV_OBJ_FLAG_SCROLLABLE);

    mod_ui_debug_group = lv_group_create();

	mod_ui_debug_label = lv_label_create(mod_ui_debug_screen);
	lv_obj_set_style_text_color(mod_ui_debug_label, lv_color_white(), 0);

    // BACK button
    mod_ui_debug_back_btn = lv_btn_create(mod_ui_debug_screen);
    lv_obj_remove_style_all(mod_ui_debug_back_btn);
    lv_obj_set_pos(mod_ui_debug_back_btn, 10, 220);
    lv_label_set_text(lv_label_create(mod_ui_debug_back_btn), "BACK");
    lv_obj_set_style_text_color(mod_ui_debug_back_btn, lv_color_white(), 0);
    lv_obj_set_style_bg_color(mod_ui_debug_back_btn, lv_color_white(), LV_STATE_FOCUSED);
    lv_obj_set_style_bg_opa(mod_ui_debug_back_btn, LV_OPA_COVER, LV_STATE_FOCUSED);
    lv_obj_set_style_text_color(mod_ui_debug_back_btn, lv_color_black(), LV_STATE_FOCUSED);
    lv_obj_set_style_pad_all(mod_ui_debug_back_btn, 2, 0);
    lv_obj_set_style_radius(mod_ui_debug_back_btn, 4, 0);
    lv_obj_add_event_cb(mod_ui_debug_back_btn,
                        mod_ui_debug_back_btn_callback,
                        LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(mod_ui_debug_back_btn,
                        mod_ui_debug_key_callback,
                        LV_EVENT_KEY, NULL);
    lv_group_add_obj(mod_ui_debug_group, mod_ui_debug_back_btn);

    // RETEST button
    mod_ui_debug_retest_btn = lv_btn_create(mod_ui_debug_screen);
    lv_obj_remove_style_all(mod_ui_debug_retest_btn);
    lv_obj_set_pos(mod_ui_debug_retest_btn, 150, 220);
    lv_label_set_text(lv_label_create(mod_ui_debug_retest_btn), "RETEST");
    lv_obj_set_style_text_color(mod_ui_debug_retest_btn, lv_color_white(), 0);
    lv_obj_set_style_bg_color(mod_ui_debug_retest_btn, lv_color_white(), LV_STATE_FOCUSED);
    lv_obj_set_style_bg_opa(mod_ui_debug_retest_btn, LV_OPA_COVER, LV_STATE_FOCUSED);
    lv_obj_set_style_text_color(mod_ui_debug_retest_btn, lv_color_black(), LV_STATE_FOCUSED);
    lv_obj_set_style_pad_all(mod_ui_debug_retest_btn, 2, 0);
    lv_obj_set_style_radius(mod_ui_debug_retest_btn, 4, 0);
    lv_obj_add_event_cb(mod_ui_debug_retest_btn,
                        mod_ui_debug_retest_btn_callback,
                        LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(mod_ui_debug_retest_btn,
                        mod_ui_debug_key_callback,
                        LV_EVENT_KEY, NULL);
    lv_group_add_obj(mod_ui_debug_group, mod_ui_debug_retest_btn);
}

/**
 * @brief Updates UI debug information
 *
 */
void mod_ui_debug_handler(void)
{
    char* w = mod_ui_debug_label_txt;

    #define ADD_TEXT(...) w += lv_snprintf(w, sizeof(mod_ui_debug_label_txt) - \
                               (w - mod_ui_debug_label_txt) - 1, __VA_ARGS__)

    if (b_mod_ui_debug_is_retesting)
    {
        return;
    }

    ADD_TEXT("Lamp State: %s %dms\n",
             mod_lamp_get_lamp_state_str(g_sys_stt.lamp_state),
             g_sys_stt.lamp_elap_ms_time);

    ADD_TEXT("Lamp Req %s / Cmd %s\n",
             mod_lamp_get_power_level_str(g_sys_ctl.lamp_req_pwr_level),
             mod_lamp_get_power_level_str(g_sys_stt.lamp_cmd_power_level));

    ADD_TEXT("     Rep %s (%dHz)\n",
             mod_lamp_get_power_level_str(g_sys_stt.lamp_power_level),
             g_sys_stt.lamp_latched_freq_hz);

    ADD_TEXT("Lamp Type %s\n", mod_lamp_get_lamp_type_str(g_sys_stt.lamp_type));

    ADD_TEXT("Board %s\n", g_sys_stt.hw_is_1_2?"V1.2":"V1.1");

    ADD_TEXT("Acc: %+.2f/%+.2f/%+.2f\n", g_sys_stt.acc_x, g_sys_stt.acc_y, g_sys_stt.acc_z);

    ADD_TEXT("Mag: %+ 5d/%+ 5d/%+ 5d\n", g_sys_stt.mag_x, g_sys_stt.mag_y, g_sys_stt.mag_z);

    ADD_TEXT("12V %s %s / 24V Reg %s\n",
             g_sys_stt.hw_is_1_2?"Reg":"Switched",
             g_sys_stt.is_12v_rail_on?"ON ":"off",
             g_sys_stt.is_24v_rail_on?"ON ":"off");

    ADD_TEXT("VBUS: %.1f/12V: %.1f/24V: %.1f\n",
             g_sys_stt.v_vbus,
             g_sys_stt.v_12v,
             g_sys_stt.v_24v);

    if (g_mod_pow_is_usb_connected_b)
    {
        ADD_TEXT("USB Req %dV Got %.1fV/%.1fA\n",
                 g_mod_pow_usb_negotiated_mv / 1000,
                 g_sys_stt.v_vbus,
                 ((float)drv_usb_pd_get_negotiated_ma())/1000.);
    }
    else
    {
        ADD_TEXT("USB: none (barrel jack)\n");
    }

    D_RADAR_REPORT_T* r = drv_radar_debug_get_report();

    int r_time = drv_radar_debug_get_report_time();
    int dt     = (time_us_64() - r_time)/(1000);

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
             drv_radar_get_distance_cm());

    lv_label_set_text(mod_ui_debug_label, mod_ui_debug_label_txt);
}

/**
 * @brief Display debug screen
 *
 */
void mod_ui_debug_open(void)
{
	lv_screen_load(mod_ui_debug_screen);
    drv_display_set_indev_group(mod_ui_debug_group);
    lv_group_focus_obj(mod_ui_debug_back_btn);
}

/* Callback functions --------------------------------------------------------*/

/**
 * @brief Callback function for back button click event
 *
 * @param p_evt
 */
static void mod_ui_debug_back_btn_callback(lv_event_t* p_evt)
{
    g_mod_ui_new_screen = M_UI_SCRN_MAIN_C;
}

/**
 * @brief Callback for RETEST button — clears lamp type, re-runs detection, reboots.
 *
 * @param p_evt
 */
static void mod_ui_debug_retest_btn_callback(lv_event_t* p_evt)
{
    M_UI_SCRN_DBG_DBG_PRINT_TXT("Retest requested from debug screen");

    lv_obj_add_flag(mod_ui_debug_back_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(mod_ui_debug_retest_btn, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(mod_ui_debug_label, "RETESTING...\n\nLamp will turn off,\nthen board will\nreboot when done.");
    lv_obj_invalidate(mod_ui_debug_screen);
    //lv_refr_now(NULL);

    b_mod_ui_debug_is_retesting = true;

    g_sys_ctl.task.lamp_test_n_reboot = 1;
}

/**
 * @brief Key handler for debug buttons — LEFT/RIGHT navigate between buttons
 */
static void mod_ui_debug_key_callback(lv_event_t* p_evt)
{
    uint32_t key = lv_event_get_key(p_evt);
    if (key == LV_KEY_LEFT)
    {
        lv_group_focus_prev(mod_ui_debug_group);
    }
    else if (key == LV_KEY_RIGHT)
    {
        lv_group_focus_next(mod_ui_debug_group);
    }
}


/* Private functions ---------------------------------------------------------*/

/*** END OF FILE ***/
