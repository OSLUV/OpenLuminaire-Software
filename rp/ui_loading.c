/**
 * @file      ui_loading.c
 * @author    The OSLUV Project
 * @brief     UI loading module using LVGL library
 *  
 */


/* Includes ------------------------------------------------------------------*/

#include <lvgl.h>
#include "ui_loading.h"
#include "splash_img.h"
#include "lamp.h"
#include "display.h"
#include "board.h"
#include "sense.h"
#include "serial.h"                                                            /* bangladesh-study: serial on boot screen */


/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Global variables  ---------------------------------------------------------*/
/* Private variables  --------------------------------------------------------*/

static lv_group_t*   ui_loading_lv_group;
static lv_obj_t*     ui_loading_lv_splash_screen;
static lv_obj_t *    ui_loading_lv_catch;
static lv_obj_t*     ui_loading_lv_label;
static lv_event_cb_t ui_loading_lv_exit_callback = NULL;
static lv_obj_t *    ui_loading_lv_psu_screen  = NULL;
static lv_obj_t *    ui_loading_lv_psu_label   = NULL;
static lv_obj_t *    ui_loading_lv_psu_status  = NULL;


/* Callback prototypes -------------------------------------------------------*/

static void ui_loading_splash_event_callback(lv_event_t* p_evt);


/* Private function prototypes -----------------------------------------------*/

static const char* ui_loading_get_psu_error_msg(void);
/* Exported functions --------------------------------------------------------*/

/**
 * @brief UI initialization procedure
 * 
 */
void ui_loading_init(void)
{
	ui_loading_lv_splash_screen = lv_obj_create(NULL);
	//ui_loading_lv_label = lv_label_create(screen);
	//lv_label_set_text(ui_loading_lv_label, "Starting... 123");
}

/**
 * @brief UI update
 * 
 */
void ui_loading_update(void)
{
	// static int x = 0;
	// lv_label_set_text_fmt(ui_loading_lv_label, "Starting... %d", x++);
}

/**
 * @brief Loads UI splash screen
 * 
 */
void ui_loading_open(void)
{
	lv_screen_load(ui_loading_lv_splash_screen);
}

/**
 * @brief UI splash screen initialization
 * 
 */
void ui_loading_splash_image_init(void)
{
	ui_loading_lv_group = lv_group_create();

    /* bangladesh-study: the boot screen now shows the lamp type, serial number and a  */
    /* "Booting..." message instead of the aerolamp splash bitmap. Lamp type and the   */
    /* serial are already known here (loaded/derived earlier in main()).               */

    /* 1 ─ Pick the lamp-type text ------------------------------------------ */
    const char *type_str;
    switch (lamp_get_type())
    {
        case LAMP_TYPE_DIMMABLE_C:     type_str = "Dimmable"; break;
        case LAMP_TYPE_NON_DIMMABLE_C: type_str = "Basic";    break;
        default: /* UNKNOWN */         type_str = "Unknown";  break;
    }

    /* bangladesh-study: old aerolamp splash bitmap selection, kept for reference:
    const lv_image_dsc_t *p_src = &splash_default_img;                          // Fallback
    switch (lamp_get_type())
    {
        case LAMP_TYPE_DIMMABLE_C:     p_src = &splash_dimmable_img; break;
        case LAMP_TYPE_NON_DIMMABLE_C: p_src = &splash_basic_img;    break;
        default: break;
    }
    */

    /* 2 ─ Build a throw-away LVGL screen ----------------------------------- */
    ui_loading_lv_splash_screen = lv_obj_create(NULL);                          // Blank screen
    lv_obj_set_style_bg_color(ui_loading_lv_splash_screen, lv_color_black(), 0);
    lv_obj_clear_flag(ui_loading_lv_splash_screen, LV_OBJ_FLAG_SCROLLABLE);

    /* bangladesh-study: old bitmap placement, kept for reference:
    lv_obj_t *p_img = lv_img_create(ui_loading_lv_splash_screen);               // place the bitmap
    lv_img_set_src(p_img, p_src);
    lv_obj_center(p_img);
    */

    /* Lamp type — top */
    lv_obj_t *lbl_type = lv_label_create(ui_loading_lv_splash_screen);
    lv_label_set_text(lbl_type, type_str);
    lv_obj_set_style_text_color(lbl_type, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_type, &lv_font_montserrat_24, 0);
    lv_obj_align(lbl_type, LV_ALIGN_TOP_MID, 0, 20);

    /* "Booting..." — dead centre */
    ui_loading_lv_label = lv_label_create(ui_loading_lv_splash_screen);
    lv_label_set_text(ui_loading_lv_label, "Booting...");
    lv_obj_set_style_text_color(ui_loading_lv_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(ui_loading_lv_label, &lv_font_montserrat_32, 0);
    lv_obj_center(ui_loading_lv_label);

    /* Serial number — bottom */
    lv_obj_t *lbl_sn = lv_label_create(ui_loading_lv_splash_screen);
    lv_label_set_text_fmt(lbl_sn, "SN: %s", serial_get_display_string());
    lv_obj_set_style_text_color(lbl_sn, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_sn, &lv_font_montserrat_16, 0);
    lv_obj_align(lbl_sn, LV_ALIGN_BOTTOM_MID, 0, -20);

	/* 3 - Listen for ANY key / click / encoder turn ------------------------ */
    /*
    lv_obj_add_event_cb(ui_loading_lv_splash_screen, 
                        ui_loading_splash_event_callback, 
                        LV_EVENT_ALL, NULL);
    */
	ui_loading_lv_catch = lv_btn_create(ui_loading_lv_splash_screen);           // Transparent button
	lv_obj_remove_style_all(ui_loading_lv_catch);
	lv_obj_set_size(ui_loading_lv_catch, LV_PCT(100), LV_PCT(100));             // Cover whole screen
	lv_obj_set_style_bg_opa(ui_loading_lv_catch, LV_OPA_TRANSP, 0);             // Invisible
	
	lv_obj_add_event_cb(ui_loading_lv_catch, 
                        ui_loading_splash_event_callback, 
                        LV_EVENT_KEY, NULL);
	lv_group_add_obj(ui_loading_lv_group, ui_loading_lv_catch);
}

/**
 * @brief Opens the UI Splash Image
 * 
 * @param on_exit_cb 
 */
void ui_loading_splash_image_open(lv_event_cb_t on_exit_cb) 
{
	ui_loading_lv_exit_callback = on_exit_cb;

    lv_screen_load(ui_loading_lv_splash_screen);                                // Swap to the new screen
    display_set_backlight_brightness(33);    
	
	display_set_indev_group(ui_loading_lv_group);
	lv_group_focus_obj(ui_loading_lv_catch);                                    // Ensure it has focus
	
	lv_timer_handler();                                                         // Flush once so it appears
}

/**
 * @brief Show PSU
 * 
 * @note Call when the PSU is bad
 */
void ui_loading_show_psu(void)
{
    if (ui_loading_lv_psu_screen)
    {
        lv_label_set_text_fmt(ui_loading_lv_psu_label,
                              "%s\n\n"
                              "VBUS: %.1f V\n"
                              "12V: %.1f  24V: %.1f",
                              ui_loading_get_psu_error_msg(),
                              g_sense_vbus, g_sense_12v, g_sense_24v);
        lv_label_set_text(ui_loading_lv_psu_status, "");
        lv_scr_load(ui_loading_lv_psu_screen);
        return;
    }

    ui_loading_lv_psu_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_loading_lv_psu_screen, lv_color_black(), 0);
    lv_obj_clear_flag(ui_loading_lv_psu_screen, LV_OBJ_FLAG_SCROLLABLE);

    /* Main error content — centered */
    ui_loading_lv_psu_label = lv_label_create(ui_loading_lv_psu_screen);
    lv_label_set_text(ui_loading_lv_psu_label, ui_loading_get_psu_error_msg());
    lv_obj_set_style_text_color(ui_loading_lv_psu_label, lv_color_white(), 0);
    lv_obj_set_style_text_align(ui_loading_lv_psu_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(ui_loading_lv_psu_label, &lv_font_montserrat_24, 0);
    lv_obj_align(ui_loading_lv_psu_label, LV_ALIGN_CENTER, 0, -15);

    /* Status line — bottom of screen */
    ui_loading_lv_psu_status = lv_label_create(ui_loading_lv_psu_screen);
    lv_label_set_text(ui_loading_lv_psu_status, "");
    lv_obj_set_style_text_color(ui_loading_lv_psu_status, lv_color_white(), 0);
    lv_obj_set_style_text_align(ui_loading_lv_psu_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(ui_loading_lv_psu_status, &lv_font_montserrat_24, 0);
    lv_obj_align(ui_loading_lv_psu_status, LV_ALIGN_BOTTOM_MID, 0, -10);

    lv_scr_load(ui_loading_lv_psu_screen);
	lv_timer_handler();
}

/**
 * @brief Update the PSU error screen with a status message
 *
 * @param status  Status string to display (e.g., "Retrying USB-PD...")
 *
 * @note Flushes display immediately so the message is visible before
 *       any blocking calls.
 */
void ui_loading_show_psu_status(const char *status)
{
    if (!ui_loading_lv_psu_status)
    {
        return;
    }

    lv_label_set_text(ui_loading_lv_psu_status, status);
    lv_timer_handler();
}


/* Private functions ---------------------------------------------------------*/

static const char* ui_loading_get_psu_error_msg(void)
{
    if (board_is_v1_2())
    {
        return "ERROR:\nPOWER SUPPLY\n"
               "INCOMPATIBLE!\n"
               "Needs 20W at 5-24V";
    }

    return "ERROR:\nPOWER SUPPLY\n"
           "INCOMPATIBLE!\n"
           "Needs  12V | 1.8A";
}


/* Callback functions --------------------------------------------------------*/

/**
 * @brief Callback for clic events
 * 
 * @param p_evt 
 * 
 * @note Internal handler – runs on FIRST key / click
 */
static void ui_loading_splash_event_callback(lv_event_t* p_evt)
{
    if (ui_loading_lv_exit_callback) 
    {
        ui_loading_lv_exit_callback(p_evt);                                     // Hand back control
    }
}


/* Private functions ---------------------------------------------------------*/

/*** END OF FILE ***/
