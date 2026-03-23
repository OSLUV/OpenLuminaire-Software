/**
 * @file      ui_main.h
 * @author    The OSLUV Project
 * @brief     UI main screen handling module
 *  
 */


/* Includes ------------------------------------------------------------------*/

#include <lvgl.h>
#include <stdio.h>
#include "display.h"
#include "lamp.h"
#include "imu.h"
#include "buttons.h"
#include "ui_debug.h"
#include "ui_loading.h"
#include "ui_main.h"
#include "safety_logic.h"
#include "persistance.h"
#include <string.h>


/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/

#define UI_COLOR_ACCENT_C   lv_color_hex(0x5600FF)
#define UI_MAIN_LAMP_PWR_C  LAMP_PWR_100PCT_C


/* Global variables  ---------------------------------------------------------*/

extern const lv_font_t * FONT_MAIN  = NULL; 
extern const lv_font_t * FONT_BIG   = NULL;
extern const lv_font_t * FONT_SMALL = NULL;
extern const lv_font_t * FONT_MED   = NULL;


/* Private variables  --------------------------------------------------------*/

static const uint8_t ui_dim_levels[UI_MAIN_MAX_DIM_LEVELS_C] = { 20, 40, 70, 100 };

static lv_obj_t *ui_screen;
static lv_obj_t *ui_sw_power;
static lv_obj_t *ui_sw_radar;
static lv_obj_t *ui_slider_intensity;
static lv_obj_t *ui_lbl_radar;
static lv_obj_t *ui_lbl_slider;
static lv_obj_t *ui_lbl_status;
static lv_obj_t *ui_lbl_percent;

static lv_obj_t *ui_lbl_tilt_val;                                               /* Tilt read-out handle (big number) */

// Style helpers
static lv_style_t ui_style_title;
static lv_style_t ui_style_status;
static lv_style_t ui_style_tick;
static lv_style_t ui_style_big;
static lv_style_t ui_style_btn;
static lv_style_t ui_style_slider_main;
static lv_style_t ui_style_slider_knob;
static lv_style_t ui_style_switch_on;
static lv_style_t ui_style_switch_off;
static lv_style_t ui_style_row;
static lv_style_t ui_style_focus;
static lv_style_t ui_style_btn_focus_inv;
static lv_style_t ui_style_label_inv;
static lv_style_t ui_style_inactive;

static lv_group_t* ui_lv_group;

static uint16_t ui_row_height;
static uint16_t ui_sw_height;
static uint16_t ui_sw_length;
//static uint16_t ui_dbg_pos;
static bool ui_show_dim_b;
static bool ui_lamp_known_b;


/* Callback prototypes -------------------------------------------------------*/

static void ui_main_debug_btn_callback(lv_event_t * e);
static void ui_main_back_to_menu_callback(lv_event_t * e);
static void ui_main_sw_power_changed_callback(lv_event_t * e);
static void ui_main_sw_radar_changed_callback(lv_event_t * e);
static void ui_main_slider_int_changed_callback(lv_event_t * e);
static void ui_main_focus_sync_callback(lv_event_t *e);


/* Private function prototypes -----------------------------------------------*/

static inline void ui_main_set_screen(void);
static inline void ui_main_set_lamp_ctrl_row(void);
static inline void ui_main_set_lamp_power_row(void);
static inline void ui_main_set_radar_row(void);
static inline void ui_main_set_lamp_dim_slider(void);
static inline void ui_main_set_tilt_row(void);
static inline void ui_main_set_debug_tools(void);
static void ui_main_theme_init(void);
static void ui_main_set_tilt(uint16_t deg);
static void ui_main_styles_init(void);


/* Exported functions --------------------------------------------------------*/

void ui_main_init(void)
{
    lv_obj_t *row;

	ui_main_theme_init();
    ui_main_styles_init();

    ui_main_set_screen();

    ui_lamp_known_b = (lamp_get_type() != LAMP_TYPE_UNKNOWN_C);

    if (!ui_lamp_known_b)
    {
        lv_obj_t *lbl = lv_label_create(ui_screen);
        lv_label_set_text(lbl,
                          "LAMP TYPE COULD\n"
                          "NOT BE DETERMINED\n\n"
                          "Check lamp\n"
                          "connection");
        lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, 0);
        lv_obj_set_width(lbl, LV_PCT(100));
        lv_obj_set_style_pad_top(lbl, 20, 0);
    }
    else
    {
        ui_main_set_lamp_ctrl_row();
        ui_main_set_lamp_power_row();
        ui_main_set_radar_row();

        if (ui_show_dim_b)
        {
            ui_main_set_lamp_dim_slider();
        }

        ui_main_set_tilt_row();
    }

    ui_main_set_debug_tools();
}

/**
 * @brief Handles main screen widgets updates
 * 
 */
void ui_main_update(void)
{
	if (!ui_lamp_known_b) return;

	static char buf[48];
    bool power_on = lv_obj_has_state(ui_sw_power, LV_STATE_CHECKED);
    bool radar_on = lv_obj_has_state(ui_sw_radar, LV_STATE_CHECKED);
	bool inactive = !power_on;
	LAMP_PWR_LEVEL_E intensity_setting = UI_MAIN_LAMP_PWR_C;

	/* Get current User set-point lamp power level  */
	if (ui_show_dim_b)
    {
		int intensity_setting_int = lv_slider_get_value(ui_slider_intensity);
        intensity_setting = LAMP_PWR_20PCT_C + intensity_setting_int;
	}
	
	/* Update lamp status                           */
	LAMP_STATE_E s = lamp_get_lamp_state();
    const char * txt = (s == LAMP_STATE_OFF_C)                 ? "Lamp off"      : 
					   (s == LAMP_STATE_STARTING_C)            ? "Lamp starting..."   :
					   (s == LAMP_STATE_RESTRIKE_COOLDOWN_1_C) ? "Restrike cooldown 1":
					   (s == LAMP_STATE_RESTRIKE_ATTEMPT_1_C)  ? "Restrike attempt 1":
					   (s == LAMP_STATE_RESTRIKE_COOLDOWN_2_C) ? "Restrike cooldown 2":
					   (s == LAMP_STATE_RESTRIKE_ATTEMPT_2_C)  ? "Restrike attempt 2":
					   (s == LAMP_STATE_RESTRIKE_COOLDOWN_3_C) ? "Restrike cooldown 3":
					   (s == LAMP_STATE_RESTRIKE_ATTEMPT_3_C)  ? "Restrike attempt 3":
					   (s == LAMP_STATE_RUNNING_C)             ? "Lamp running"   :
					   (s == LAMP_STATE_FAILED_OFF_C)          ? "Lamp off - ERROR" :
					   (s == LAMP_STATE_FULLPOWER_TEST_C)      ? "Calibrating..." : "STATUS UNKNOWN";
    
	int pct_req = (intensity_setting == LAMP_PWR_20PCT_C) ?  20 :
				  (intensity_setting == LAMP_PWR_40PCT_C) ?  40 :
				  (intensity_setting == LAMP_PWR_70PCT_C) ?  70 :
				  (intensity_setting == LAMP_PWR_100PCT_C)? 100 : 0;
	LAMP_PWR_LEVEL_E  cmd = lamp_get_commanded_power_level();                   // What has been sent to pwm
	int pct_cmd;
	bool warming = lamp_is_warming();

	if (warming)
    {
		txt = "Lamp starting...";
		pct_cmd = 100;
	}
    else 
    {
	    pct_cmd = (cmd == LAMP_PWR_20PCT_C) ?  20 :
				  (cmd == LAMP_PWR_40PCT_C) ?  40 :
				  (cmd == LAMP_PWR_70PCT_C) ?  70 :
				  (cmd == LAMP_PWR_100PCT_C)? 100 : 0;                          // LAMP_PWR_OFF_C or unknown
	}
    LAMP_PWR_LEVEL_E rep;                                                       // Reported level
	lamp_get_reported_power_level(&rep);   
    int pct_rep = (rep == LAMP_PWR_20PCT_C) ?  20 :
				  (rep == LAMP_PWR_40PCT_C) ?  40 :
				  (rep == LAMP_PWR_70PCT_C) ?  70 :
				  (rep == LAMP_PWR_100PCT_C)? 100 : 0;
				
	bool radar_active = radar_on && pct_cmd < pct_req && power_on;
	if (radar_active)
    {
		txt = "Radar triggered";
    }

	if (ui_show_dim_b)
    {
		lv_snprintf(buf, sizeof(buf), "%s (%d%%)", txt, pct_cmd);
		
	}
    else
    {
		lv_snprintf(buf, sizeof(buf), "%s\n%d%%", txt, pct_cmd);
	}
	lv_label_set_text(ui_lbl_status, buf);

    if (!power_on)
    {
        safety_logic_set_radar_enabled_state(false);
        lamp_request_power_level(LAMP_PWR_OFF_C);
		persistance_set_power_state(power_on);
    }
    else
    {
        if (radar_on)
        {
            safety_logic_set_radar_enabled_state(true);
            safety_logic_set_cap_power(intensity_setting);
        }
        else
        {
            safety_logic_set_radar_enabled_state(false);
            lamp_request_power_level(intensity_setting);
        }
    }

	if (ui_show_dim_b)
    {
		if (inactive || warming)
        {
			lv_obj_add_state(ui_slider_intensity, LV_STATE_USER_2);             // Grey it
			lv_obj_add_state(ui_lbl_slider, LV_STATE_USER_2);  
		}
        else
        {
			lv_obj_clear_state(ui_slider_intensity, LV_STATE_USER_2);           // Full color
			lv_obj_clear_state(ui_lbl_slider, LV_STATE_USER_2);
		}
    }
	if (inactive)
    {
        lv_obj_add_state(ui_sw_radar, LV_STATE_USER_2);
		lv_obj_add_state(ui_lbl_radar, LV_STATE_USER_2);
    }
    else
    {
        lv_obj_clear_state(ui_sw_radar, LV_STATE_USER_2);
		lv_obj_clear_state(ui_lbl_radar,  LV_STATE_USER_2);
	}
	
	/* Update tilt data */
	int16_t a = imu_get_pointing_down_angle(); 
	ui_main_set_tilt(a);
}

/**
 * @brief Displays main screen
 * 
 */
void ui_main_open(void)
{
    lv_scr_load(ui_screen);
    display_set_indev_group(ui_lv_group);
    lv_group_focus_obj(ui_sw_power);
    lv_obj_send_event(ui_sw_power, LV_EVENT_FOCUSED, NULL);
}

/**
 * @brief Sets the Lamp state (On/Off)
 * @note This function can be called via external command
 * 
 * @param req_state State to set (1: On, 0: Off)
 * @return int16_t  (0: failed, 1: suceed)
 */
int16_t ui_main_lamp_set_stt(uint16_t req_state)
{
    LAMP_PWR_LEVEL_E lamp_pwr_lvl;

    if (req_state == 0)
    {
        lamp_get_reported_power_level(&lamp_pwr_lvl);

        display_screen_on();

        //if (lamp_pwr_lvl != LAMP_PWR_OFF_C)                                     // Lamp is ON ?
        {
            persistance_set_power_state(0);
            persistance_write_region();

            lv_obj_set_state(ui_sw_power, LV_STATE_CHECKED, false);             // Update function will update lamp's state
        }

        return 1;
    }
    else if (req_state == 1)
    {
        lamp_get_reported_power_level(&lamp_pwr_lvl);

        if (lamp_pwr_lvl == LAMP_PWR_OFF_C)                                     // Lamp state is OFF ?
        {
            display_screen_on();

            persistance_set_power_state(1);
            persistance_write_region();

            lv_obj_set_state(ui_sw_power, LV_STATE_CHECKED, true);              // Update function will update lamp's state
        }

        return 1;
    }

    return 0; // Error
}

/**
 * @brief Gets the current Lamp Status (On/Off)
 * @note This function can be called via external command
 * 
 * @param state No state is required. The function need to comply with format.
 * @return int16_t Lamp Status (On/Off)
 */
int16_t ui_main_lamp_get_stt(uint16_t state)
{
    return persistance_get_power_state();
}

/**
 * @brief Sets the Lamp Dim Level
 * @note This function can be called via external command
 * @note If level is not valid, returns failed
 * 
 * @param level Dim level to set to lamp
 * @return int16_t (0: failed, 1: suceed)
 */
int16_t ui_main_lamp_set_dim(uint16_t level)
{
    LAMP_PWR_LEVEL_E lamp_pwr_level;

    lamp_pwr_level = LAMP_PWR_UNKNOWN_C;
    switch (level)
    {
        case 0:
            //lamp_pwr_level = LAMP_PWR_OFF_C;
            return 0;
        break;

        case 20:
            lamp_pwr_level = LAMP_PWR_20PCT_C;
        break;
        
        case 40:
            lamp_pwr_level = LAMP_PWR_40PCT_C;
        break;
        
        case 70:
            lamp_pwr_level = LAMP_PWR_70PCT_C;
        break;
        
        case 100:
            lamp_pwr_level = LAMP_PWR_100PCT_C;
        break;

        default:
            return 0;
        break;
    }

    if (ui_show_dim_b && (lamp_pwr_level < LAMP_PWR_MAX_SETTINGS_C))
    {
        display_screen_on();

        lamp_pwr_level -= LAMP_PWR_20PCT_C;

        persistance_set_dim_index(lamp_pwr_level);

        lv_slider_set_value(ui_slider_intensity, lamp_pwr_level, LV_ANIM_OFF);

        return level;
    }

    return 0;
}

/**
 * @brief Gets the Lamp Dim Level
 * @note This function can be called via external command
 * 
 * @param level No level is required. The function need to comply with format.
 * @return int16_t 
 */
int16_t ui_main_lamp_get_dim(uint16_t level)
{
    int dim_setting;
    LAMP_PWR_LEVEL_E lamp_pwr_lvl;
    int16_t dim_level;

    dim_level = 100;
	
	if (ui_show_dim_b) 
    {
		dim_setting  = lv_slider_get_value(ui_slider_intensity);
        lamp_pwr_lvl = LAMP_PWR_20PCT_C + dim_setting;
	
        switch (lamp_pwr_lvl)
        {
            case LAMP_PWR_OFF_C:
                dim_level = 0;
            break;

            case LAMP_PWR_20PCT_C:
                dim_level = 20;
            break;
            
            case LAMP_PWR_40PCT_C:
                dim_level = 40;
            break;
            
            case LAMP_PWR_70PCT_C:
                dim_level = 70;
            break;
            
            case LAMP_PWR_100PCT_C:
                dim_level = 100;
            break;

            default:
                dim_level = 100;
            break;
        }
	}

    return dim_level;
}


/* Callback functions --------------------------------------------------------*/

static void ui_main_debug_btn_callback(lv_event_t * e)
{
    ui_debug_open();
}

static void ui_main_back_to_menu_callback(lv_event_t * e)
{
    ui_main_open();    // reopen the main menu ui_screen
}

/* --- persistence write helpers --------------------------------- */
static void ui_main_sw_power_changed_callback(lv_event_t * e)
{
    bool on = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    persistance_set_power_state(on);
    persistance_write_region();                        /* flash only if value changed */
}

static void ui_main_sw_radar_changed_callback(lv_event_t * e)
{
    bool on = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    persistance_set_radar_state(on);
    persistance_write_region();
}

static void ui_main_slider_int_changed_callback(lv_event_t * e)
{
    uint8_t idx = lv_slider_get_value(lv_event_get_target(e)); /* 0–3 */
    persistance_set_dim_index(idx);
    persistance_write_region();
}

/**
 * @brief Event helper
 * 
 * @param e 
 */
static void ui_main_focus_sync_callback(lv_event_t *e)
{
    lv_obj_t *ctrl  = lv_event_get_target(e);       /* slider / switch / btn */
    lv_obj_t *label = lv_event_get_user_data(e);    /* associated label     */

    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_FOCUSED) {
        lv_obj_add_state(label, LV_STATE_USER_1);   /* turn mint on */
    }
    else if(code == LV_EVENT_DEFOCUSED) {
        lv_obj_clear_state(label, LV_STATE_USER_1); /* back to white */
    }
}


/* Private functions ---------------------------------------------------------*/

/**
 * @brief Sets and initializes the main screen object
 * 
 */
static inline void ui_main_set_screen(void)
{
    ui_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_screen, lv_color_black(), 0);
    lv_obj_set_style_pad_all(ui_screen, 0, 0);
    lv_obj_set_flex_flow(ui_screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui_screen, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,  LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_style(ui_screen, NULL, LV_PART_SCROLLBAR);                    /* kill scrollbar */
    lv_obj_set_scrollbar_mode(ui_screen, LV_SCROLLBAR_MODE_OFF);
}

/**
 * @brief Sets and initializes the Lamp Control (On/Off) object on screen
 * @note Screen must be initialized before calling this function
 * 
 */
static inline void ui_main_set_lamp_ctrl_row(void)
{
    lv_obj_t *row;

    row = lv_obj_create(ui_screen);
    lv_obj_add_style(row, &ui_style_row, 0);
    lv_obj_set_width(row, 239);
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    //lv_obj_set_pos(row, 0, 240);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);
    
    ui_lbl_status = lv_label_create(row);
    lv_label_set_text(ui_lbl_status, "Status: Unknown");
    lv_obj_set_style_text_align(ui_lbl_status, LV_TEXT_ALIGN_CENTER, 0);  
    lv_obj_remove_style_all(ui_lbl_status);
    lv_obj_add_style(ui_lbl_status, &ui_style_status, 0);
}

/**
 * @brief Sets and initializes the Lamp Power object on screen
 * @note Screen must be initialized before calling this function
 * 
 */
static inline void ui_main_set_lamp_power_row(void)
{
    lv_obj_t *row;

    row = lv_obj_create(ui_screen);
    lv_obj_add_style(row, &ui_style_row, 0);
    lv_obj_set_size(row, 230, ui_row_height);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *lbl = lv_label_create(row);
    lv_label_set_text(lbl, "POWER");
    lv_obj_add_style(lbl, &ui_style_title, 0);
    lv_obj_add_style(lbl, &ui_style_label_inv, LV_PART_MAIN | LV_STATE_USER_1);

    ui_sw_power = lv_switch_create(row);
    lv_obj_set_size(ui_sw_power, ui_sw_length, ui_sw_height);
    lv_obj_set_pos(row, 5,20);

    // lv_obj_add_state(ui_sw_power, LV_STATE_CHECKED);
    if (persistance_get_power_state())
    {
        lv_obj_add_state(ui_sw_power, LV_STATE_CHECKED);
    }
    lv_obj_set_style_bg_color(ui_sw_power, UI_COLOR_ACCENT_C, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_style(ui_sw_power, &ui_style_switch_off, LV_PART_MAIN);
    lv_obj_add_style(ui_sw_power, &ui_style_switch_on, LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_add_style(ui_sw_power, &ui_style_focus, LV_PART_MAIN | LV_STATE_FOCUSED);
    
    lv_obj_add_event_cb(ui_sw_power, ui_main_focus_sync_callback, LV_EVENT_FOCUSED,   lbl);
    lv_obj_add_event_cb(ui_sw_power, ui_main_focus_sync_callback, LV_EVENT_DEFOCUSED, lbl);
    lv_obj_add_event_cb(ui_sw_power, ui_main_sw_power_changed_callback, LV_EVENT_VALUE_CHANGED, NULL);
    lv_group_add_obj(ui_lv_group, ui_sw_power);
    //lv_obj_add_event_cb(sw, ui_main_focus_sync_callback, LV_EVENT_FOCUSED | LV_EVENT_DEFOCUSED, lbl);
}

/**
 * @brief Sets and initializes the Radar object on screen
 * @note Screen must be initialized before calling this function
 * 
 */
static inline void ui_main_set_radar_row(void)
{
    lv_obj_t *row;

    row = lv_obj_create(ui_screen);
    lv_obj_add_style(row, &ui_style_row, 0);
    lv_obj_set_size(row, 230, ui_row_height);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);
    //lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);

    ui_lbl_radar = lv_label_create(row);
    lv_label_set_text(ui_lbl_radar, "RADAR");
    lv_obj_add_style(ui_lbl_radar, &ui_style_title, 0);
    lv_obj_add_style(ui_lbl_radar, &ui_style_label_inv, LV_PART_MAIN | LV_STATE_USER_1);
    lv_obj_add_style(ui_lbl_radar, &ui_style_inactive, LV_PART_MAIN | LV_STATE_USER_2);
    lv_obj_add_style(ui_lbl_radar, &ui_style_inactive, LV_PART_INDICATOR| LV_STATE_USER_2);
    
    ui_sw_radar = lv_switch_create(row);
    if (persistance_get_radar_state())
    {
        lv_obj_add_state(ui_sw_radar, LV_STATE_CHECKED);
    }
    lv_obj_set_size(ui_sw_radar, ui_sw_length, ui_sw_height);
    lv_obj_set_style_bg_color(ui_sw_radar, UI_COLOR_ACCENT_C, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_style(ui_sw_radar, &ui_style_switch_off, LV_PART_MAIN);
    lv_obj_add_style(ui_sw_radar, &ui_style_switch_on, LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_add_style(ui_sw_radar, &ui_style_focus, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_add_style(ui_sw_radar, &ui_style_inactive, LV_PART_MAIN      | LV_STATE_USER_2);     // body
    lv_obj_add_style(ui_sw_radar, &ui_style_inactive, LV_PART_INDICATOR | LV_STATE_USER_2);     // track
    lv_obj_add_event_cb(ui_sw_radar, ui_main_sw_radar_changed_callback, LV_EVENT_VALUE_CHANGED, NULL);
    lv_group_add_obj(ui_lv_group, ui_sw_radar);

    //lv_obj_add_event_cb(sw, ui_main_focus_sync_callback, LV_EVENT_FOCUSED | LV_EVENT_DEFOCUSED, lbl);
    lv_obj_add_event_cb(ui_sw_radar, ui_main_focus_sync_callback, LV_EVENT_FOCUSED,   ui_lbl_radar);
    lv_obj_add_event_cb(ui_sw_radar, ui_main_focus_sync_callback, LV_EVENT_DEFOCUSED, ui_lbl_radar);
}

/**
 * @brief Sets and initializes the Lamp Dimmer object on screen
 * @note Screen must be initialized before calling this function
 * 
 */
static inline void ui_main_set_lamp_dim_slider(void)
{
    lv_obj_t *row;
    static const char *ticks[4] = { "20% ", "40%", " 70%", "100%" };
    
    row = lv_obj_create(ui_screen);
    lv_obj_add_style(row, &ui_style_row, 0);
    lv_obj_set_size(row, 230, ui_row_height);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);
    ui_lbl_slider = lv_label_create(row);
    lv_label_set_text(ui_lbl_slider, "INTENSITY");
    lv_obj_add_style(ui_lbl_slider, &ui_style_title, 0);
    lv_obj_add_style(ui_lbl_slider, &ui_style_label_inv, LV_PART_MAIN | LV_STATE_USER_1);
    lv_obj_add_style(ui_lbl_slider, &ui_style_inactive, LV_PART_MAIN | LV_STATE_USER_2);

    row = lv_obj_create(ui_screen);
    lv_obj_add_style(row, &ui_style_row, 0);
    lv_obj_set_size(row, 230, 18);

    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row,
                          LV_FLEX_ALIGN_CENTER,  /* main-axis (left-to-right) */
                          LV_FLEX_ALIGN_CENTER,  /* cross-axis ⇒ vertical-centre  */
                          LV_FLEX_ALIGN_CENTER); /* track_cross for multi-row wrap */
    //lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(row, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    ui_slider_intensity = lv_slider_create(row);
    lv_obj_set_size(ui_slider_intensity, 180, 8);
    lv_obj_add_style(ui_slider_intensity, &ui_style_slider_main, LV_PART_INDICATOR);
    lv_obj_add_style(ui_slider_intensity, &ui_style_slider_knob, LV_PART_KNOB);
    
    lv_obj_add_style(ui_slider_intensity, &ui_style_inactive, LV_PART_MAIN | LV_STATE_USER_2);  // rail
    lv_obj_add_style(ui_slider_intensity, &ui_style_inactive, LV_PART_KNOB | LV_STATE_USER_2);  // knob
    lv_obj_add_style(ui_slider_intensity, &ui_style_inactive, LV_PART_INDICATOR| LV_STATE_USER_2);  // filled part
    
    lv_slider_set_range(ui_slider_intensity, 0, 3);                             /* 4 ticks */
    //lv_slider_set_value(ui_slider_intensity, 3, LV_ANIM_OFF);                 /* Default level 3 = 100% */
    lv_slider_set_value(ui_slider_intensity, persistance_get_dim_index(), LV_ANIM_OFF);
    lv_obj_add_event_cb(ui_slider_intensity, ui_main_slider_int_changed_callback, LV_EVENT_VALUE_CHANGED, NULL);
    lv_group_add_obj(ui_lv_group, ui_slider_intensity);
    lv_obj_add_event_cb(ui_slider_intensity, ui_main_focus_sync_callback, LV_EVENT_FOCUSED,   ui_lbl_slider);
    lv_obj_add_event_cb(ui_slider_intensity, ui_main_focus_sync_callback, LV_EVENT_DEFOCUSED, ui_lbl_slider);


    /* Tick-labels under the dim slider */
    lv_obj_t *tick_row = lv_obj_create(ui_screen);
    lv_obj_remove_style_all(tick_row);                                          /* no border/bg */
    lv_obj_set_size(tick_row, 240, 12);
    lv_obj_set_x(tick_row, 5);
    lv_obj_set_flex_flow(tick_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(tick_row,
                            LV_FLEX_ALIGN_SPACE_EVENLY,                         /* equal gaps   */
                            LV_FLEX_ALIGN_CENTER,
                            LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(tick_row, 0, 0);                                   /* zero padding */
    lv_obj_set_style_pad_gap(tick_row, 0, 0);                                   /* no extra gap */

    for (int i = 0; i < 4; i++) 
    {
        lv_obj_t *tl = lv_label_create(tick_row);
        lv_label_set_text(tl, ticks[i]);
        lv_obj_add_style(tl, &ui_style_tick, 0);                                /* white mono 12-pt */
    }
}

/**
 * @brief Sets and initializes the Tilt text object on screen
 * @note Screen must be initialized before calling this function
 * 
 */
static inline void ui_main_set_tilt_row(void)
{
    lv_obj_t *row;

    row = lv_obj_create(ui_screen);
    lv_obj_add_style(row, &ui_style_row, 0);
    lv_obj_set_size(row, 230, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);
    
    /* Caption “TILT” */
    lv_obj_t *lbl_tilt = lv_label_create(row);
    lv_label_set_text(lbl_tilt, "TILT     ");
    lv_obj_add_style(lbl_tilt, &ui_style_title, 0); 
    lv_obj_set_x(lbl_tilt, 8);

    /* Big value */
    ui_lbl_tilt_val = lv_label_create(row);
    lv_label_set_text(ui_lbl_tilt_val, "--°");                                  /* default */
    lv_obj_add_style(ui_lbl_tilt_val, &ui_style_big, 0);
    lv_obj_set_x(lbl_tilt, 150);
    lv_obj_set_style_text_align(ui_lbl_tilt_val, LV_TEXT_ALIGN_RIGHT, 0);
}

/**
 * @brief Set and initialize Debug and Other buttons rows
 * @note Screen must be initialized before calling this function
 * 
 */
static inline void ui_main_set_debug_tools(void)
{
    lv_obj_t *row;

    row = lv_obj_create(ui_screen);
    lv_obj_add_style(row, &ui_style_row, 0);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    //lv_obj_set_pos(row,0, 240);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER,  LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);
    //lv_obj_set_flex_grow(row, 0);                                             // IMPORTANT LINE
    //lv_obj_align(row, LV_ALIGN_BOTTOM_MID, 0, 0);                             // always 27 px from bottom
    
    //ui_lbl_percent = lv_label_create(row);
    //lv_label_set_text(ui_lbl_percent, "          : --%");
    //lv_obj_remove_style_all(ui_lbl_percent);
    //lv_obj_add_style(ui_lbl_percent, &ui_style_btn, 0);
    //lv_obj_set_pos(ui_lbl_percent,10,0);
    //lv_obj_remove_style_all(btn);
    //lv_obj_add_style(btn, &ui_style_btn, 0);
    //lv_obj_add_style(btn, &ui_style_btn_focus_inv, LV_PART_MAIN | LV_STATE_FOCUSED);
    //lv_obj_set_pos(btn,10,0);
    //lv_label_set_text(lv_label_create(btn), "< BACK");

    //lv_obj_add_event_cb(btn, back_btn_cb, LV_EVENT_CLICKED, NULL);
    //lv_group_add_obj(ui_lv_group, btn);

    lv_obj_t *btn = lv_btn_create(row);
    lv_obj_remove_style_all(btn);
    lv_obj_add_style(btn, &ui_style_btn, 0);
    lv_obj_add_style(btn, &ui_style_btn_focus_inv, LV_PART_MAIN | LV_STATE_FOCUSED);
    //lv_obj_set_pos(btn, ui_dbg_pos,0);
    lv_label_set_text(lv_label_create(btn), "DEBUG");
    lv_obj_add_event_cb(btn, ui_main_debug_btn_callback, LV_EVENT_CLICKED, NULL);
    lv_group_add_obj(ui_lv_group, btn);
}

/**
 * @brief Sets screen theme
 * 
 */
static void ui_main_theme_init(void)
{
    if (lamp_get_type() == LAMP_TYPE_DIMMABLE_C)
    {   
		ui_row_height     = 23;
		ui_sw_height = ui_row_height-3;
		ui_sw_length = ui_sw_height * 2;
		//ui_dbg_pos = 165;
		ui_show_dim_b = true;
		FONT_MAIN = &lv_font_montserrat_20;  
		FONT_MED = &lv_font_montserrat_16;
		FONT_SMALL = &lv_font_montserrat_14;
		FONT_BIG = &lv_font_montserrat_44;
    }
    else
    {
        ui_row_height     = 32;
		ui_sw_height = ui_row_height-2;
		ui_sw_length = ui_sw_height * 2;
		//ui_dbg_pos = 125;
		ui_show_dim_b = false;
		FONT_MAIN = &lv_font_montserrat_32;  
		FONT_MED = &lv_font_montserrat_22;
		FONT_SMALL = &lv_font_montserrat_22;
		FONT_BIG = &lv_font_montserrat_48;
	}
}

/**
 * @brief Update helper you can call from sensor task
 * 
 * @param deg 
 */
static void ui_main_set_tilt(uint16_t deg)
{
    static char buf[8];                                                         // Enough for "-123°\0"  
    
    lv_snprintf(buf, sizeof(buf), "%d°", deg);
    lv_label_set_text(ui_lbl_tilt_val, buf);
}

/**
 * @brief Screen styles initialization
 * 
 */
static void ui_main_styles_init(void)
{
    ui_lv_group = lv_group_create();

    /* White titles like “RADAR”, “INTENSITY” */
    lv_style_init(&ui_style_title);
    lv_style_set_text_color(&ui_style_title, lv_color_white());
    lv_style_set_text_font(&ui_style_title, FONT_MAIN);

	/* Status                                 */
	lv_style_init(&ui_style_status);
    lv_style_set_text_color(&ui_style_status, lv_color_white());
    lv_style_set_text_font(&ui_style_status, FONT_MED);

    /* Small tick labels                      */
    lv_style_init(&ui_style_tick);
    lv_style_set_text_color(&ui_style_tick, lv_color_white());
    lv_style_set_text_font(&ui_style_tick, FONT_SMALL);

    /* Big 180°                               */
    lv_style_init(&ui_style_big);
    lv_style_set_text_color(&ui_style_big, lv_color_white());
    lv_style_set_text_font(&ui_style_big, FONT_BIG);

    /* Bottom nav buttons as plain text       */
    lv_style_init(&ui_style_btn);
    lv_style_set_bg_opa(&ui_style_btn, LV_OPA_TRANSP);
    lv_style_set_border_opa(&ui_style_btn, LV_OPA_TRANSP);
    lv_style_set_text_color(&ui_style_btn, lv_color_white());
    lv_style_set_text_font(&ui_style_btn, FONT_SMALL);

	/* Label highlighting                     */
	lv_style_init(&ui_style_label_inv);
	lv_style_set_bg_color(&ui_style_label_inv, lv_color_white());
	lv_style_set_bg_opa(&ui_style_label_inv, LV_OPA_COVER);
	lv_style_set_text_color(&ui_style_label_inv, lv_color_black());
	// Put a little breathing-room around the text
    lv_style_set_pad_left(&ui_style_label_inv, 2);
    lv_style_set_pad_right(&ui_style_label_inv, 2);
    lv_style_set_pad_top(&ui_style_label_inv, 1);
    lv_style_set_pad_bottom(&ui_style_label_inv,1);
    // Softly rounded corners ; LV_RADIUS_CIRCLE for a pill
    lv_style_set_radius(&ui_style_label_inv, 4);

    lv_style_init(&ui_style_btn_focus_inv);
    lv_style_set_bg_color(&ui_style_btn_focus_inv, lv_color_white());
    lv_style_set_bg_opa(&ui_style_btn_focus_inv, LV_OPA_COVER);
    lv_style_set_text_color(&ui_style_btn_focus_inv, lv_color_black());
    lv_style_set_outline_width(&ui_style_btn_focus_inv, 1);                     /* No ring */
    // Put a little breathing-room around the text
    lv_style_set_pad_left(&ui_style_btn_focus_inv, 2);
    lv_style_set_pad_right(&ui_style_btn_focus_inv, 2);
    lv_style_set_pad_top(&ui_style_btn_focus_inv, 1);
    lv_style_set_pad_bottom(&ui_style_btn_focus_inv,1);
    // Softly rounded corners ; LV_RADIUS_CIRCLE for a pill
    lv_style_set_radius(&ui_style_btn_focus_inv, 4);

    /* Slider track                           */
    lv_style_init(&ui_style_slider_main);
    lv_style_set_bg_color(&ui_style_slider_main, UI_COLOR_ACCENT_C);
    lv_style_set_bg_opa(&ui_style_slider_main, LV_OPA_COVER);

    /* Slider knob                            */
    lv_style_init(&ui_style_slider_knob);
    lv_style_set_bg_color(&ui_style_slider_knob, lv_color_white());
    lv_style_set_radius(&ui_style_slider_knob, LV_RADIUS_CIRCLE);

    /* Switch ON                              */
    lv_style_init(&ui_style_switch_on);
    lv_style_set_bg_color(&ui_style_switch_on, UI_COLOR_ACCENT_C);
	lv_style_set_border_width(&ui_style_switch_on, 2);
	lv_style_set_border_color(&ui_style_switch_on, UI_COLOR_ACCENT_C);
    lv_style_set_border_opa(&ui_style_switch_on, LV_OPA_100);

    /* Switch OFF (outline white)             */
    lv_style_init(&ui_style_switch_off);
    lv_style_set_bg_color(&ui_style_switch_off, lv_color_black());
	lv_style_set_border_width(&ui_style_switch_off, 2);
    lv_style_set_border_opa(&ui_style_switch_off, LV_OPA_100);
    lv_style_set_border_color(&ui_style_switch_off, lv_color_white());
    //lv_style_set_border_width(&ui_style_switch_off, 2);

    /* Row container: fully transparent, no border, no padding */
    lv_style_init(&ui_style_row);
    lv_style_set_bg_opa(&ui_style_row, LV_OPA_TRANSP);
    lv_style_set_border_opa(&ui_style_row, LV_OPA_0);
    lv_style_set_border_width(&ui_style_row, 0);
    lv_style_set_pad_all(&ui_style_row, 0);

    lv_style_init(&ui_style_focus);
    lv_style_set_outline_width(&ui_style_focus, 3);                             // Set thickness
    lv_style_set_outline_pad(&ui_style_focus, 0);                               // Snug
    lv_style_set_outline_color(&ui_style_focus, lv_color_white());  
    lv_style_set_outline_opa(&ui_style_focus, LV_OPA_COVER);    
    lv_style_set_radius(&ui_style_focus, LV_RADIUS_CIRCLE); 
	
	/* Inactive/disabled switches/sliders     */
	lv_style_init(&ui_style_inactive);
	lv_style_set_opa(&ui_style_inactive, LV_OPA_40);                            // Fade everything a bit
	lv_style_set_bg_color(&ui_style_inactive, lv_color_hex(0x808080));
	lv_style_set_outline_color(&ui_style_inactive, lv_color_hex(0x808080));  
	lv_style_set_border_color(&ui_style_inactive, lv_color_hex(0x808080));
	//grey text for labels that live inside the widget(slider value, etc.)
	lv_style_set_text_color(&ui_style_inactive, lv_color_hex(0xc0c0c0));
}


/*** END OF FILE ***/
