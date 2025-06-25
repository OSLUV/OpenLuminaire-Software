#include <lvgl.h>
#include <stdio.h>
#include "display.h"
#define COLOR_ACCENT  lv_color_hex(0x1edaa8)

static lv_obj_t* screen;
static const uint8_t dim_levels[] = { 20, 40, 70, 100 };

/* ————————————————————————————————— STYLE HELPERS —— */
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
static lv_style_t style_tag, style_title, style_tick, style_big,
                  style_btn, style_slider_main, style_slider_knob,
                  style_switch_on, style_switch_off, style_row,
				  style_focus, style_btn_focus_inv, style_label_inv;

static void styles_init(void)
{
    /* tag e.g. “POWER” */
    lv_style_init(&style_tag);
    lv_style_set_bg_color(&style_tag, lv_color_white());
    lv_style_set_radius(&style_tag, LV_RADIUS_CIRCLE);
    lv_style_set_pad_all(&style_tag, 1);
    lv_style_set_text_color(&style_tag, lv_color_black());
    lv_style_set_text_font(&style_tag, &lv_font_montserrat_14);

    /* white titles like “RADAR”, “INTENSITY” */
    lv_style_init(&style_title);
    lv_style_set_text_color(&style_title, lv_color_white());
    lv_style_set_text_font(&style_title, &lv_font_montserrat_20);

    /* small tick labels */
    lv_style_init(&style_tick);
    lv_style_set_text_color(&style_tick, lv_color_white());
    lv_style_set_text_font(&style_tick, &lv_font_montserrat_14);

    /* big 180° */
    lv_style_init(&style_big);
    lv_style_set_text_color(&style_big, lv_color_white());
    lv_style_set_text_font(&style_big, &lv_font_montserrat_46);

    /* bottom nav buttons as plain text */
    lv_style_init(&style_btn);
    lv_style_set_bg_opa(&style_btn, LV_OPA_TRANSP);
    lv_style_set_border_opa(&style_btn, LV_OPA_TRANSP);
    lv_style_set_text_color(&style_btn, lv_color_white());
    lv_style_set_text_font(&style_btn, &lv_font_montserrat_14);

	// label highlighting

	lv_style_init(&style_label_inv);
	lv_style_set_bg_color (&style_label_inv, lv_color_white());
	lv_style_set_bg_opa   (&style_label_inv, LV_OPA_COVER);
	lv_style_set_text_color(&style_label_inv, lv_color_black());

    lv_style_init(&style_btn_focus_inv);
    lv_style_set_bg_color (&style_btn_focus_inv, lv_color_white());
    lv_style_set_bg_opa   (&style_btn_focus_inv, LV_OPA_COVER);
    lv_style_set_text_color(&style_btn_focus_inv, lv_color_black());
    lv_style_set_outline_width(&style_btn_focus_inv, 1);       /* no ring */


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
    lv_style_set_border_opa(&style_switch_on, LV_OPA_TRANSP);

    /* switch OFF (outline white) */
    lv_style_init(&style_switch_off);
    lv_style_set_bg_color(&style_switch_off, lv_color_black());

    lv_style_set_border_opa(&style_switch_off, LV_OPA_TRANSP);
    //lv_style_set_border_color(&style_switch_off, lv_color_white());
    //lv_style_set_border_width(&style_switch_off, 2);

    /* Row container: fully transparent, no border, no padding */
    lv_style_init(&style_row);
    lv_style_set_bg_opa(&style_row, LV_OPA_TRANSP);
    lv_style_set_border_opa(&style_row, LV_OPA_0);
    lv_style_set_border_width(&style_row, 0);
    lv_style_set_pad_all(&style_row, 1);

    lv_style_init(&style_focus);
    lv_style_set_outline_width (&style_focus, 3);                /* how thick  */
    lv_style_set_outline_pad   (&style_focus, 0);                /* snug       */
    lv_style_set_outline_color (&style_focus, lv_color_white());     /* mint ring  */
    lv_style_set_outline_opa   (&style_focus, LV_OPA_COVER);     /* solid      */
    lv_style_set_radius        (&style_focus, LV_RADIUS_CIRCLE); /* rounded on switches */

}
void ui_main_init()
{
	
    styles_init();

    screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_pad_all(screen, 1, 0);
    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(screen, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,  LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_style(screen, NULL, LV_PART_SCROLLBAR);    /* kill scrollbar */
    lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);




	// — POWER ——————————————————————————
    {
        lv_obj_t *row = lv_obj_create(screen);
        lv_obj_add_style(row, &style_row, 0);
        lv_obj_set_size(row, 230, 22);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
        lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);


        lv_obj_t *lbl = lv_label_create(row);
        lv_label_set_text(lbl, "POWER");
        lv_obj_add_style(lbl, &style_title, 0);
        lv_obj_add_style(lbl, &style_label_inv, LV_PART_MAIN | LV_STATE_USER_1);

        lv_obj_t *sw = lv_switch_create(row);
        lv_obj_set_size(sw, 42,21);

        lv_obj_add_state(sw, LV_STATE_CHECKED);
        lv_obj_add_style(sw, &style_switch_off, LV_PART_INDICATOR);
        lv_obj_add_style(sw, &style_switch_on, LV_PART_INDICATOR | LV_STATE_CHECKED);
		lv_obj_add_style(sw, &style_focus, LV_PART_MAIN | LV_STATE_FOCUSED);
        //lv_obj_add_event_cb(sw, cb_power, LV_EVENT_VALUE_CHANGED, NULL);
        lv_group_add_obj(the_group, sw);
		lv_obj_add_event_cb(sw, focus_sync_cb, LV_EVENT_FOCUSED,   lbl);
		lv_obj_add_event_cb(sw, focus_sync_cb, LV_EVENT_DEFOCUSED, lbl);
		//lv_obj_add_event_cb(sw, focus_sync_cb, LV_EVENT_FOCUSED | LV_EVENT_DEFOCUSED, lbl);
    }

    // — RADAR ——————————————————————————
    {
        lv_obj_t *row = lv_obj_create(screen);
        lv_obj_add_style(row, &style_row, 0);
        lv_obj_set_size(row, 230, 22);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
        lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);
        //lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);


        lv_obj_t *lbl = lv_label_create(row);
        lv_label_set_text(lbl, "RADAR");
        lv_obj_add_style(lbl, &style_title, 0);
        lv_obj_add_style(lbl, &style_label_inv, LV_PART_MAIN | LV_STATE_USER_1);

        lv_obj_t *sw = lv_switch_create(row);
        lv_obj_set_size(sw, 42,21);
        lv_obj_add_style(sw, &style_switch_off, LV_PART_INDICATOR);
        lv_obj_add_style(sw, &style_switch_on, LV_PART_INDICATOR | LV_STATE_CHECKED);
		lv_obj_add_style(sw, &style_focus, LV_PART_MAIN | LV_STATE_FOCUSED);
     //   lv_obj_add_event_cb(sw, cb_radar, LV_EVENT_VALUE_CHANGED, NULL);
        lv_group_add_obj(the_group, sw);

		//lv_obj_add_event_cb(sw, focus_sync_cb, LV_EVENT_FOCUSED | LV_EVENT_DEFOCUSED, lbl);
		lv_obj_add_event_cb(sw, focus_sync_cb, LV_EVENT_FOCUSED,   lbl);
		lv_obj_add_event_cb(sw, focus_sync_cb, LV_EVENT_DEFOCUSED, lbl);
	}

    /* DIM slider (discrete 20/40/70/100 – default 100) */
    {
        lv_obj_t *row = lv_obj_create(screen);
        lv_obj_add_style(row, &style_row, 0);
        lv_obj_set_size(row, 230, 20);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
        lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);
        lv_obj_t *lbl = lv_label_create(row);
        lv_label_set_text(lbl, "INTENSITY");
        lv_obj_add_style(lbl, &style_title, 0);
        lv_obj_add_style(lbl, &style_label_inv, LV_PART_MAIN | LV_STATE_USER_1);

        lv_obj_t *row2 = lv_obj_create(screen);
        lv_obj_add_style(row2, &style_row, 0);
        lv_obj_set_size(row2, 230, 20);

        lv_obj_set_flex_flow(row2, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row2,
                              LV_FLEX_ALIGN_CENTER,   /* main-axis (left-to-right) */
                              LV_FLEX_ALIGN_CENTER,  /* cross-axis ⇒ vertical-centre  */
                              LV_FLEX_ALIGN_CENTER); /* track_cross for multi-row wrap */
        //lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
        lv_obj_set_scrollbar_mode(row2, LV_SCROLLBAR_MODE_OFF);
        lv_obj_add_flag(row2, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

        lv_obj_t *slider = lv_slider_create(row2);
        lv_obj_set_size(slider, 180, 8);
        lv_obj_add_style(slider, &style_slider_main, LV_PART_INDICATOR);
        lv_obj_add_style(slider, &style_slider_knob, LV_PART_KNOB);
        lv_slider_set_range(slider, 0, 3);          /* 4 ticks          */
        lv_slider_set_value(slider, 3, LV_ANIM_OFF);/* default 100 %    */
     //   lv_obj_add_event_cb(slider, cb_dim, LV_EVENT_VALUE_CHANGED, NULL);
        lv_group_add_obj(the_group, slider);
		lv_obj_add_event_cb(slider, focus_sync_cb, LV_EVENT_FOCUSED,   lbl);
		lv_obj_add_event_cb(slider, focus_sync_cb, LV_EVENT_DEFOCUSED, lbl);
    }

    /* Navigation buttons row */
    {
        lv_obj_t *row = lv_obj_create(screen);
        lv_obj_add_style(row, &style_row, 0);
        lv_obj_set_width(row, LV_PCT(100));               /* full 240 px */
        lv_obj_set_height(row, 27);
        lv_obj_set_pos(row,0,213);
        //lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        //lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY,  LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);

        lv_obj_t *btn = lv_btn_create(row);
        lv_obj_remove_style_all(btn);
        lv_obj_add_style(btn, &style_btn, 0);
		lv_obj_add_style(btn, &style_btn_focus_inv, LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_pos(btn,10,0);
        lv_label_set_text(lv_label_create(btn), "< BACK");

     //   lv_obj_add_event_cb(btn, cb_nav, LV_EVENT_PRESSED, ui_loading_open);
        lv_group_add_obj(the_group, btn);

        btn = lv_btn_create(row);
        lv_obj_remove_style_all(btn);
        lv_obj_add_style(btn, &style_btn, 0);
		lv_obj_add_style(btn, &style_btn_focus_inv, LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_pos(btn,150,0);
        lv_label_set_text(lv_label_create(btn), "DEBUG >");
   //     lv_obj_add_event_cb(btn, cb_nav, LV_EVENT_PRESSED, ui_debug_open);         /* stub for now */
        lv_group_add_obj(the_group, btn);
    }

	/*
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
	}*/
}

void ui_main_update()
{
	
}

void ui_main_open()
{
	printf("Open ui_main\n");
	lv_screen_load(screen);
}
