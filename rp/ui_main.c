#include <lvgl.h>
#include <stdio.h>
#include "display.h"
#include "lamp.h"
#include "imu.h"
#include "menu.h"
#include "buttons.h"
#include "ui_debug.h"
#include "ui_loading.h"
#include "safety_logic.h"
#include "persistance.h"
#include <string.h>
#define COLOR_ACCENT  lv_color_hex(0x5600FF)

static lv_obj_t *screen;
static lv_obj_t *sw_power;
static lv_obj_t *sw_radar;
static lv_obj_t *slider_intensity;
static lv_obj_t *lbl_radar;
static lv_obj_t *lbl_slider;
static lv_obj_t *lbl_status;
static lv_obj_t *lbl_percent;
static const uint8_t dim_levels[] = { 20, 40, 70, 100 };
void ui_main_open();

// Callbacks
static void debug_btn_cb(lv_event_t * e)
{
    ui_debug_open();
}
static void back_to_menu_cb(lv_event_t * e)
{
    ui_main_open();    // reopen the main menu screen
}

/* --- persistence write helpers --------------------------------- */
static void sw_power_changed_cb(lv_event_t * e)
{
    bool on = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    persist_set_power(on);
    write_persistance_region();                        /* flash only if value changed */
}

static void sw_radar_changed_cb(lv_event_t * e)
{
    bool on = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    persist_set_radar(on);
    write_persistance_region();
}

static void slider_int_changed_cb(lv_event_t * e)
{
    uint8_t idx = lv_slider_get_value(lv_event_get_target(e)); /* 0–3 */
    persist_set_dim_idx(idx);
    write_persistance_region();
}


uint16_t ROW_HEIGHT;
uint16_t SWITCH_HEIGHT;
uint16_t SWITCH_LENGTH;
//uint16_t DEBUG_POS;
bool SHOW_DIM;
extern const lv_font_t * FONT_MAIN = NULL; 
extern const lv_font_t * FONT_BIG = NULL;
extern const lv_font_t * FONT_SMALL = NULL;
extern const lv_font_t * FONT_MED = NULL;

void ui_theme_init(void)
{
    if (get_lamp_type() == LAMP_TYPE_DIMMABLE) {   
		ROW_HEIGHT     = 23;
		SWITCH_HEIGHT = ROW_HEIGHT-3;
		SWITCH_LENGTH = SWITCH_HEIGHT * 2;
		//DEBUG_POS = 165;
		SHOW_DIM = true;
		FONT_MAIN = &lv_font_montserrat_20;  
		FONT_MED = &lv_font_montserrat_16;
		FONT_SMALL = &lv_font_montserrat_14;
		FONT_BIG = &lv_font_montserrat_44;
        
    } else {
        ROW_HEIGHT     = 32;
		SWITCH_HEIGHT = ROW_HEIGHT-2;
		SWITCH_LENGTH = SWITCH_HEIGHT * 2;
		//DEBUG_POS = 125;
		SHOW_DIM = false;
		FONT_MAIN = &lv_font_montserrat_32;  
		FONT_MED = &lv_font_montserrat_22;
		FONT_SMALL = &lv_font_montserrat_22;
		FONT_BIG = &lv_font_montserrat_48;
	}
}


/* -------- TILT read-out handle (big number) -------- */
static lv_obj_t *lbl_tilt_val;

/* Update helper you can call from sensor task */
void ui_set_tilt(uint16_t deg)
{
    static char buf[8];  // enough for "-123°\0"  
    lv_snprintf(buf, sizeof(buf), "%d°", deg);
    lv_label_set_text(lbl_tilt_val, buf);
}

/* ------- Event helper ----------*/
static void focus_sync_cb(lv_event_t *e)
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
/* ————————————————————————————————— STYLE HELPERS —— */
static lv_style_t style_title, style_status, style_tick, style_big,
                  style_btn, style_slider_main, style_slider_knob,
                  style_switch_on, style_switch_off, style_row,
				  style_focus, style_btn_focus_inv, style_label_inv, style_inactive;

static lv_group_t* the_group;

static void styles_init(void)
{
    the_group = lv_group_create();

    /* white titles like “RADAR”, “INTENSITY” */
    lv_style_init(&style_title);
    lv_style_set_text_color(&style_title, lv_color_white());
    lv_style_set_text_font(&style_title, FONT_MAIN);

	//status
	lv_style_init(&style_status);
    lv_style_set_text_color(&style_status, lv_color_white());
    lv_style_set_text_font(&style_status, FONT_MED);

    /* small tick labels */
    lv_style_init(&style_tick);
    lv_style_set_text_color(&style_tick, lv_color_white());
    lv_style_set_text_font(&style_tick, FONT_SMALL);

    /* big 180° */
    lv_style_init(&style_big);
    lv_style_set_text_color(&style_big, lv_color_white());
    lv_style_set_text_font(&style_big, FONT_BIG);

    /* bottom nav buttons as plain text */
    lv_style_init(&style_btn);
    lv_style_set_bg_opa(&style_btn, LV_OPA_TRANSP);
    lv_style_set_border_opa(&style_btn, LV_OPA_TRANSP);
    lv_style_set_text_color(&style_btn, lv_color_white());
    lv_style_set_text_font(&style_btn, FONT_SMALL);

	// label highlighting
	lv_style_init(&style_label_inv);
	lv_style_set_bg_color (&style_label_inv, lv_color_white());
	lv_style_set_bg_opa   (&style_label_inv, LV_OPA_COVER);
	lv_style_set_text_color(&style_label_inv, lv_color_black());
	// put a little breathing-room around the text
    lv_style_set_pad_left (&style_label_inv, 2);
    lv_style_set_pad_right(&style_label_inv, 2);
    lv_style_set_pad_top  (&style_label_inv, 1);
    lv_style_set_pad_bottom(&style_label_inv,1);
    //  softly rounded corners ; LV_RADIUS_CIRCLE for a pill
    lv_style_set_radius(&style_label_inv, 4);

    lv_style_init(&style_btn_focus_inv);
    lv_style_set_bg_color (&style_btn_focus_inv, lv_color_white());
    lv_style_set_bg_opa   (&style_btn_focus_inv, LV_OPA_COVER);
    lv_style_set_text_color(&style_btn_focus_inv, lv_color_black());
    lv_style_set_outline_width(&style_btn_focus_inv, 1);       /* no ring */
    // put a little breathing-room around the text
    lv_style_set_pad_left (&style_btn_focus_inv, 2);
    lv_style_set_pad_right(&style_btn_focus_inv, 2);
    lv_style_set_pad_top  (&style_btn_focus_inv, 1);
    lv_style_set_pad_bottom(&style_btn_focus_inv,1);
    //  softly rounded corners ; LV_RADIUS_CIRCLE for a pill
    lv_style_set_radius(&style_btn_focus_inv, 4);

    /* slider track */
    lv_style_init(&style_slider_main);
    lv_style_set_bg_color(&style_slider_main, COLOR_ACCENT);
    lv_style_set_bg_opa(&style_slider_main, LV_OPA_COVER);

    /* slider knob */
    lv_style_init(&style_slider_knob);
    lv_style_set_bg_color(&style_slider_knob, lv_color_white());
    lv_style_set_radius(&style_slider_knob, LV_RADIUS_CIRCLE);

    /* switch ON */
    lv_style_init(&style_switch_on);
    lv_style_set_bg_color(&style_switch_on, COLOR_ACCENT);
	lv_style_set_border_width(&style_switch_on, 2);
	lv_style_set_border_color(&style_switch_on, COLOR_ACCENT);
    lv_style_set_border_opa(&style_switch_on, LV_OPA_100);

    /* switch OFF (outline white) */
    lv_style_init(&style_switch_off);
    lv_style_set_bg_color(&style_switch_off, lv_color_black());
	lv_style_set_border_width(&style_switch_off, 2);
    lv_style_set_border_opa(&style_switch_off, LV_OPA_100);
    lv_style_set_border_color(&style_switch_off, lv_color_white());
    //lv_style_set_border_width(&style_switch_off, 2);

    /* Row container: fully transparent, no border, no padding */
    lv_style_init(&style_row);
    lv_style_set_bg_opa(&style_row, LV_OPA_TRANSP);
    lv_style_set_border_opa(&style_row, LV_OPA_0);
    lv_style_set_border_width(&style_row, 0);
    lv_style_set_pad_all(&style_row, 0);

    lv_style_init(&style_focus);
    lv_style_set_outline_width (&style_focus, 3);    //thickness
    lv_style_set_outline_pad   (&style_focus, 0);    //snug
    lv_style_set_outline_color (&style_focus, lv_color_white());  
    lv_style_set_outline_opa   (&style_focus, LV_OPA_COVER);    
    lv_style_set_radius        (&style_focus, LV_RADIUS_CIRCLE); 
	
	// inactive/disabled switches/sliders
	lv_style_init(&style_inactive);
	lv_style_set_opa            (&style_inactive, LV_OPA_40); // fade everything a bit
	lv_style_set_bg_color       (&style_inactive, lv_color_hex(0x808080));
	lv_style_set_outline_color  (&style_inactive, lv_color_hex(0x808080));  
	lv_style_set_border_color       (&style_inactive, lv_color_hex(0x808080));
	// grey text for labels that live inside the widget (slider value, etc.)
	lv_style_set_text_color     (&style_inactive, lv_color_hex(0xc0c0c0));
}


void ui_main_init()
{
	ui_theme_init();
    styles_init();	

    screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(screen, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,  LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_style(screen, NULL, LV_PART_SCROLLBAR);    /* kill scrollbar */
    lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);

	// if (!SHOW_DIM){ // buffer row
		// lv_obj_t *row = lv_obj_create(screen);
		// lv_obj_add_style(row, &style_row, 0);
        // lv_obj_set_size(row, 230, 20);
        // lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        // lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
        // lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);
	// }
	
	//	lamp status row
	{
		lv_obj_t *row = lv_obj_create(screen);
        lv_obj_add_style(row, &style_row, 0);
        lv_obj_set_width(row, 239);
        lv_obj_set_height(row, LV_SIZE_CONTENT);
        //lv_obj_set_pos(row, 0, 240);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER,  LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);
		
		lbl_status = lv_label_create(row);
        lv_label_set_text(lbl_status, "Status: Unknown");
		lv_obj_set_style_text_align(lbl_status, LV_TEXT_ALIGN_CENTER, 0);  
		lv_obj_remove_style_all(lbl_status);
        lv_obj_add_style(lbl_status, &style_status, 0);
	}

	// — POWER ——————————————————————————
    {
        lv_obj_t *row = lv_obj_create(screen);
        lv_obj_add_style(row, &style_row, 0);
        lv_obj_set_size(row, 230, ROW_HEIGHT);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
        lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);

        lv_obj_t *lbl = lv_label_create(row);
        lv_label_set_text(lbl, "POWER");
        lv_obj_add_style(lbl, &style_title, 0);
        lv_obj_add_style(lbl, &style_label_inv, LV_PART_MAIN | LV_STATE_USER_1);


        sw_power = lv_switch_create(row);
        lv_obj_set_size(sw_power, SWITCH_LENGTH, SWITCH_HEIGHT);
        lv_obj_set_pos(row, 5,20);

        // lv_obj_add_state(sw_power, LV_STATE_CHECKED);
		if (persist_get_power())  lv_obj_add_state(sw_power, LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(sw_power, COLOR_ACCENT, LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_add_style(sw_power, &style_switch_off, LV_PART_MAIN);
        lv_obj_add_style(sw_power, &style_switch_on, LV_PART_MAIN | LV_STATE_CHECKED);
		lv_obj_add_style(sw_power, &style_focus, LV_PART_MAIN | LV_STATE_FOCUSED);
        
		lv_obj_add_event_cb(sw_power, focus_sync_cb, LV_EVENT_FOCUSED,   lbl);
		lv_obj_add_event_cb(sw_power, focus_sync_cb, LV_EVENT_DEFOCUSED, lbl);
		lv_obj_add_event_cb(sw_power, sw_power_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
		lv_group_add_obj(the_group, sw_power);
		//lv_obj_add_event_cb(sw, focus_sync_cb, LV_EVENT_FOCUSED | LV_EVENT_DEFOCUSED, lbl);
    }

    // — RADAR ——————————————————————————
    {
        lv_obj_t *row = lv_obj_create(screen);
        lv_obj_add_style(row, &style_row, 0);
        lv_obj_set_size(row, 230, ROW_HEIGHT);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
        lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);
        //lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);

        lbl_radar = lv_label_create(row);
        lv_label_set_text(lbl_radar, "RADAR");
        lv_obj_add_style(lbl_radar, &style_title, 0);
        lv_obj_add_style(lbl_radar, &style_label_inv, LV_PART_MAIN | LV_STATE_USER_1);
		lv_obj_add_style(lbl_radar, &style_inactive, LV_PART_MAIN | LV_STATE_USER_2);
		lv_obj_add_style(lbl_radar, &style_inactive, LV_PART_INDICATOR| LV_STATE_USER_2);
		
        sw_radar = lv_switch_create(row);
		if (persist_get_radar())  lv_obj_add_state(sw_radar, LV_STATE_CHECKED);
        lv_obj_set_size(sw_radar, SWITCH_LENGTH, SWITCH_HEIGHT);
        lv_obj_set_style_bg_color(sw_radar, COLOR_ACCENT, LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_add_style(sw_radar, &style_switch_off, LV_PART_MAIN);
        lv_obj_add_style(sw_radar, &style_switch_on, LV_PART_MAIN | LV_STATE_CHECKED);
		lv_obj_add_style(sw_radar, &style_focus, LV_PART_MAIN | LV_STATE_FOCUSED);
		lv_obj_add_style(sw_radar, &style_inactive, LV_PART_MAIN      | LV_STATE_USER_2);     // body
		lv_obj_add_style(sw_radar, &style_inactive, LV_PART_INDICATOR | LV_STATE_USER_2);     // track
		lv_obj_add_event_cb(sw_radar, sw_radar_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
		lv_group_add_obj(the_group, sw_radar);

		//lv_obj_add_event_cb(sw, focus_sync_cb, LV_EVENT_FOCUSED | LV_EVENT_DEFOCUSED, lbl);
		lv_obj_add_event_cb(sw_radar, focus_sync_cb, LV_EVENT_FOCUSED,   lbl_radar);
		lv_obj_add_event_cb(sw_radar, focus_sync_cb, LV_EVENT_DEFOCUSED, lbl_radar);
		
	}

    /* DIM slider (discrete 20/40/70/100 – default 100) */
     if (SHOW_DIM) {
        lv_obj_t *row = lv_obj_create(screen);
        lv_obj_add_style(row, &style_row, 0);
        lv_obj_set_size(row, 230, ROW_HEIGHT);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
        lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);
        lbl_slider = lv_label_create(row);
        lv_label_set_text(lbl_slider, "INTENSITY");
        lv_obj_add_style(lbl_slider, &style_title, 0);
        lv_obj_add_style(lbl_slider, &style_label_inv, LV_PART_MAIN | LV_STATE_USER_1);
		lv_obj_add_style(lbl_slider, &style_inactive, LV_PART_MAIN | LV_STATE_USER_2);

        lv_obj_t *row2 = lv_obj_create(screen);
        lv_obj_add_style(row2, &style_row, 0);
        lv_obj_set_size(row2, 230, 18);

        lv_obj_set_flex_flow(row2, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row2,
                              LV_FLEX_ALIGN_CENTER,   /* main-axis (left-to-right) */
                              LV_FLEX_ALIGN_CENTER,  /* cross-axis ⇒ vertical-centre  */
                              LV_FLEX_ALIGN_CENTER); /* track_cross for multi-row wrap */
        //lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
        lv_obj_set_scrollbar_mode(row2, LV_SCROLLBAR_MODE_OFF);
        lv_obj_add_flag(row2, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

        slider_intensity = lv_slider_create(row2);
        lv_obj_set_size(slider_intensity, 180, 8);
        lv_obj_add_style(slider_intensity, &style_slider_main, LV_PART_INDICATOR);
        lv_obj_add_style(slider_intensity, &style_slider_knob, LV_PART_KNOB);
		
		lv_obj_add_style(slider_intensity, &style_inactive, LV_PART_MAIN | LV_STATE_USER_2);  // rail
		lv_obj_add_style(slider_intensity, &style_inactive, LV_PART_KNOB | LV_STATE_USER_2);  // knob
		lv_obj_add_style(slider_intensity, &style_inactive, LV_PART_INDICATOR| LV_STATE_USER_2);  // filled part
		
        lv_slider_set_range(slider_intensity, 0, 3);          /* 4 ticks          */
        //lv_slider_set_value(slider_intensity, 3, LV_ANIM_OFF);/* default 100 %    */
		lv_slider_set_value(slider_intensity, persist_get_dim_idx(), LV_ANIM_OFF);
		lv_obj_add_event_cb(slider_intensity, slider_int_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
        lv_group_add_obj(the_group, slider_intensity);
		lv_obj_add_event_cb(slider_intensity, focus_sync_cb, LV_EVENT_FOCUSED,   lbl_slider);
		lv_obj_add_event_cb(slider_intensity, focus_sync_cb, LV_EVENT_DEFOCUSED, lbl_slider);
    }
    if (SHOW_DIM){
            /* ── tick-labels under the INTENSITY slider ───────────────────── */
        /* 1. parent container — transparent, full width, no padding      */
        lv_obj_t *tick_row = lv_obj_create(screen);
        lv_obj_remove_style_all(tick_row);                 /* no border/bg */
        lv_obj_set_size(tick_row, 240, 12);
        lv_obj_set_x(tick_row, 5);
        lv_obj_set_flex_flow(tick_row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(tick_row,
                              LV_FLEX_ALIGN_SPACE_EVENLY,  /* equal gaps   */
                              LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_all(tick_row, 0, 0);          /* zero padding */
        lv_obj_set_style_pad_gap(tick_row, 0, 0);          /* no extra gap */

        /* 2. re-use your monospace 12-pt text style so numbers line up   */
        static const char *ticks[4] = { "20% ", "40%", " 70%", "100%" };

        for(int i = 0; i < 4; i++) {
            lv_obj_t *tl = lv_label_create(tick_row);
            lv_label_set_text(tl, ticks[i]);
            lv_obj_add_style(tl, &style_tick, 0);          /* white mono 12-pt */
        }
    }
	// TILT
    {
        static const lv_coord_t TILT_Y = 150;          /* vertical position */
        static const lv_coord_t TILT_VAL_W = 150;       /* width box for “180°” */

        lv_obj_t *row = lv_obj_create(screen);
        lv_obj_add_style(row, &style_row, 0);
        lv_obj_set_size(row, 230, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
		lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);
        /* caption “TILT” */
        lv_obj_t *lbl_tilt = lv_label_create(row);
        lv_label_set_text(lbl_tilt, "TILT     ");
        lv_obj_add_style(lbl_tilt, &style_title, 0); 
        lv_obj_set_x(lbl_tilt, 8);

        /* big value */
        lbl_tilt_val = lv_label_create(row);
        //lv_obj_set_size(lbl_tilt_val, TILT_VAL_W, LV_SIZE_CONTENT);
        lv_label_set_text(lbl_tilt_val, "--°");       /* default */
        lv_obj_add_style(lbl_tilt_val, &style_big, 0);
        lv_obj_set_x(lbl_tilt, 150);
        lv_obj_set_style_text_align(lbl_tilt_val, LV_TEXT_ALIGN_RIGHT, 0);

    }	
	
    // Debug / other buttons rows
    {
        lv_obj_t *row = lv_obj_create(screen);
        lv_obj_add_style(row, &style_row, 0);
        lv_obj_set_width(row, LV_PCT(100));
        lv_obj_set_height(row, LV_SIZE_CONTENT);
        //lv_obj_set_pos(row,0, 240);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER,  LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);
		//lv_obj_set_flex_grow(row, 0);                //  << important line		
		//lv_obj_align(row, LV_ALIGN_BOTTOM_MID, 0, 0);   // always 27 px from bottom
        
		// lbl_percent = lv_label_create(row);
		// lv_label_set_text(lbl_percent, "          : --%");
		// lv_obj_remove_style_all(lbl_percent);
        // lv_obj_add_style(lbl_percent, &style_btn, 0);
		// lv_obj_set_pos(lbl_percent,10,0);
        // lv_obj_remove_style_all(btn);
        // lv_obj_add_style(btn, &style_btn, 0);
		// lv_obj_add_style(btn, &style_btn_focus_inv, LV_PART_MAIN | LV_STATE_FOCUSED);
        //lv_obj_set_pos(btn,10,0);
        //lv_label_set_text(lv_label_create(btn), "< BACK");

        //lv_obj_add_event_cb(btn, back_btn_cb, LV_EVENT_CLICKED, NULL);
        //lv_group_add_obj(the_group, btn);

        lv_obj_t *btn = lv_btn_create(row);
        lv_obj_remove_style_all(btn);
        lv_obj_add_style(btn, &style_btn, 0);
		lv_obj_add_style(btn, &style_btn_focus_inv, LV_PART_MAIN | LV_STATE_FOCUSED);
        //lv_obj_set_pos(btn, DEBUG_POS,0);
        lv_label_set_text(lv_label_create(btn), "DEBUG");
		lv_obj_add_event_cb(btn, debug_btn_cb, LV_EVENT_CLICKED, NULL);
        lv_group_add_obj(the_group, btn);
    }
	// initialize focus on POWER switch
    
}

void ui_main_update()
{		
	
	//update power/radar/intensity widgets
    bool power_on = lv_obj_has_state(sw_power, LV_STATE_CHECKED);
    bool radar_on = lv_obj_has_state(sw_radar, LV_STATE_CHECKED);
	bool inactive = !power_on;

    enum pwr_level intensity_setting = PWR_100PCT;
	if (SHOW_DIM) {
		int intensity_setting_int = lv_slider_get_value(slider_intensity);
        intensity_setting = PWR_20PCT + intensity_setting_int;
	}	

    if (!power_on)
    {
        set_safety_logic_enabled(false);
        request_lamp_power(PWR_OFF);
		persist_set_power(power_on);
    }
    else
    {
        if (radar_on)
        {
            set_safety_logic_enabled(true);
            set_safety_logic_cap(intensity_setting);
        }
        else
        {
            set_safety_logic_enabled(false);
            request_lamp_power(intensity_setting);
        }
    }

	if (SHOW_DIM){
		if(inactive){
			lv_obj_add_state(slider_intensity, LV_STATE_USER_2);   // grey it
			lv_obj_add_state(lbl_slider, LV_STATE_USER_2);  
		} else {
			lv_obj_clear_state(slider_intensity, LV_STATE_USER_2); // full color
			lv_obj_clear_state(lbl_slider, LV_STATE_USER_2);
		}
		//lv_obj_set_state(slider_intensity, LV_STATE_DISABLED, get_lamp_type() != LAMP_TYPE_DIMMABLE || !power_on || get_lamp_state() == STATE_FULLPOWER_TEST);
    }
	if (inactive) {
        lv_obj_add_state(sw_radar, LV_STATE_USER_2);
		lv_obj_add_state(lbl_radar, LV_STATE_USER_2);
    } else {
        lv_obj_clear_state(sw_radar, LV_STATE_USER_2);
		lv_obj_clear_state(lbl_radar,  LV_STATE_USER_2);
	}//lv_obj_set_state(sw_radar, LV_STATE_DISABLED, !power_on);
	
	// update tilt
	int16_t a = get_angle_pointing_down(); 
	ui_set_tilt(a);
	
	// update lamp status
	
	
	enum lamp_state s = get_lamp_state();
    const char * txt = (s == STATE_OFF)       ? "Lamp off"      : 
					(s == STATE_STARTING) ? "Lamp starting..."   :
					(s == STATE_RESTRIKE_COOLDOWN_1) ? "Restrike cooldown 1":
					(s == STATE_RESTRIKE_ATTEMPT_1) ? "Restrike attempt 1":
					(s == STATE_RESTRIKE_COOLDOWN_2) ? "Restrike cooldown 2":
					(s == STATE_RESTRIKE_ATTEMPT_2) ? "Restrike attempt 2":
					(s == STATE_RESTRIKE_COOLDOWN_3) ? "Restrike cooldown 3":
					(s == STATE_RESTRIKE_ATTEMPT_3) ? "Restrike attempt 3":
					(s == STATE_RUNNING)   ? "Lamp running"   :
					(s == STATE_FAILED_OFF) ? "Lamp off - ERROR" :
					(s == STATE_FULLPOWER_TEST) ? "Calibrating..." : "STATUS UNKNOWN";
    
	//enum pwr_level  cmd = get_lamp_commanded_power(); // what has been sent to pwm
	enum pwr_level cmd; //reported level
	get_lamp_reported_power(&cmd);  
	enum pwr_level req  = intensity_setting;  // user set-point
	int pct_cmd = (cmd == PWR_20PCT) ? 20 :
				  (cmd == PWR_40PCT) ? 40 :
				  (cmd == PWR_70PCT) ? 70 :
				  (cmd == PWR_100PCT)? 100 : 0;  // PWR_OFF or unknown
    int pct_req = (req == PWR_20PCT) ? 20 :
				  (req == PWR_40PCT) ? 40 :
				  (req == PWR_70PCT) ? 70 :
				  (req == PWR_100PCT)? 100 : 0;
				
	bool radar_active = radar_on && pct_cmd < pct_req && power_on;
	if(radar_active)
		txt = "Radar triggered";

	static char buf[48];
	if (SHOW_DIM) { 
		lv_snprintf(buf, sizeof(buf), "%s (%d%%)", txt, pct_cmd);
	} else {
		lv_snprintf(buf, sizeof(buf), "%s\n%d%%", txt, pct_cmd);
	}
	lv_label_set_text(lbl_status, buf);
	
}

void ui_main_open()
{
    lv_scr_load(screen);
    display_set_indev_group(the_group);
    lv_group_focus_obj(sw_power);
    lv_obj_send_event(sw_power, LV_EVENT_FOCUSED, NULL);
}