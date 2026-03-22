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


/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Global variables  ---------------------------------------------------------*/
/* Private variables  --------------------------------------------------------*/

static lv_group_t*   ui_loading_lv_group;
static lv_obj_t*     ui_loading_lv_splash_screen;
static lv_obj_t *    ui_loading_lv_catch;
static lv_obj_t*     ui_loading_lv_label;
static lv_event_cb_t ui_loading_lv_exit_callback = NULL;
static lv_obj_t *    ui_loading_lv_psu_screen = NULL;
static lv_obj_t *    ui_loading_lv_psu_label  = NULL;


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

    /* 1 ─ Pick the bitmap -------------------------------------------------- */
    const lv_image_dsc_t *p_src = &splash_default_img;                          // Fallback
    switch (lamp_get_type())
    {
        case LAMP_TYPE_DIMMABLE_C:
            p_src = &splash_dimmable_img;
        break;

        case LAMP_TYPE_NON_DIMMABLE_C:
            p_src = &splash_basic_img;
        break;

        default: /* UNKNOWN */
        break;
    }

    /* 2 ─ Build a throw-away LVGL screen ----------------------------------- */
    ui_loading_lv_splash_screen = lv_obj_create(NULL);                          // Blank screen

    lv_obj_clear_flag(ui_loading_lv_splash_screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *p_img = lv_img_create(ui_loading_lv_splash_screen);               // place the bitmap

    lv_img_set_src(p_img, p_src);
    lv_obj_center(p_img);

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
        lv_label_set_text(ui_loading_lv_psu_label, ui_loading_get_psu_error_msg());
        lv_obj_center(ui_loading_lv_psu_label);
        lv_scr_load(ui_loading_lv_psu_screen);
        return;
    }

    ui_loading_lv_psu_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_loading_lv_psu_screen, lv_color_black(), 0);
    lv_obj_clear_flag(ui_loading_lv_psu_screen, LV_OBJ_FLAG_SCROLLABLE);

    ui_loading_lv_psu_label = lv_label_create(ui_loading_lv_psu_screen);
    lv_label_set_text(ui_loading_lv_psu_label, ui_loading_get_psu_error_msg());
    lv_obj_set_style_text_color(ui_loading_lv_psu_label, lv_color_white(), 0);
    lv_obj_set_style_text_align(ui_loading_lv_psu_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(ui_loading_lv_psu_label, &lv_font_montserrat_24, 0);
    lv_obj_center(ui_loading_lv_psu_label);

    lv_scr_load(ui_loading_lv_psu_screen);                                      // Make it active immediately
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
    if (!ui_loading_lv_psu_label)
    {
        return;
    }

    lv_label_set_text(ui_loading_lv_psu_label, status);
    lv_obj_center(ui_loading_lv_psu_label);
    lv_timer_handler();
}


/* Private functions ---------------------------------------------------------*/

static const char* ui_loading_get_psu_error_msg(void)
{
    if (board_is_v1_2())
    {
        return "ERROR:\n\nPOWER SUPPLY\n"
               "INCOMPATIBLE!\n\n"
               "Needs 5-24V\n"
               "barrel jack or USB-PD";
    }

    return "ERROR:\n\nPOWER SUPPLY\n"
           "INCOMPATIBLE!\n\n"
           "Needs  12 V | 2.5 A";
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
