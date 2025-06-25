#include <stdio.h>
#include <string.h>
#include <pico/stdlib.h>

#include "st7789.h"
#include "pins.h"
#include "imu.h"
#include "mag.h"
#include "usbpd.h"
#include "lamp.h"
#include "buttons.h"
#include "sense.h"
#include "radar.h"
#include "menu.h"
#include "fan.h"
#include "radio.h"
#include "safety_logic.h"
#include "persistance.h"
#include "display.h"
#include "ui_main.h"
#include "ui_loading.h"
#include <lvgl.h>

#include "font.c"

int curr_y = 0;
int curr_x = 0;

void draw_text(char* c, int nc)
{
	// for (int ch = 0; ch < nc; ch++)
	// 	{
	// 		if (c[ch] == '\n')
	// 		{
	// 			curr_x = 0;
	// 			curr_y += 8;
	// 			continue;
	// 		}

	// 		for (int x = 0; x < 8; x++)
	// 		{
	// 			for (int y = 0; y < 8; y++)
	// 			{
	// 				int xo = curr_x + x;
	// 				int yo = curr_y + y;
	// 				if (xo >= LCD_WIDTH) continue;
	// 				if (yo >= LCD_HEIGHT) continue;
	// 				bool set = ((font8x8_basic[c[ch]][y] >> x) & 1);
	// 				screen_buffer[yo][xo] = set?0xffff:0x0000;
					
	// 			}
	// 		}

	// 		curr_x += 8;
	// 	}
}

void draw_box(int x, int y, int w, int c)
{
	// for (int dx = 0; dx < w; dx++)
	// {
	// 	for (int dy = 0; dy < w; dy++)
	// 	{
	// 		int xo = x + dx;
	// 		int yo = y + dy;
	// 		if (xo >= LCD_WIDTH) continue;
	// 		if (yo >= LCD_HEIGHT) continue;
	// 		screen_buffer[yo][xo] = c;
	// 	}
	// }
}



void dbgf(const char *fmt, ...) {
	static char work_buf[512];

	va_list args;
	va_start(args, fmt);

	int n = vsnprintf(work_buf, sizeof(work_buf)-2, fmt, args);

	va_end(args);

	draw_text(work_buf, n);
}

#include "image.c"

#define LCD_WIDTH 240
#define LCD_HEIGHT 240
uint16_t screen_buffer[LCD_WIDTH][LCD_HEIGHT];

void main()
{
	backlight_set_brightness(0);
	stdio_init_all();

	gpio_init(4);
	gpio_init(5);
	gpio_init(6);
	gpio_set_dir(4, GPIO_IN);
	gpio_set_dir(5, GPIO_IN);
	gpio_set_dir(6, GPIO_IN);
	
	// st7789_init(&lcd_config, LCD_WIDTH, LCD_HEIGHT);
	// st7789_set_backlight(50);
	// display_splash_screen();
	
	
	lv_init();
	//backlight_set_brightness(0);
	display_init();
	//backlight_set_brightness(0);
	
	
	display_splash_image();
	
	

	lv_timer_handler();

	init_buttons();
	init_imu();
	init_mag();
	init_lamp();
	init_sense();
	init_radar();
	init_fan();
	init_radio();
	usbpd_negotiate(true);
	set_fan(100);

	sleep_ms(250);
	update_sense();

	set_switched_12v(true);

	sleep_ms(1000);

	update_sense();

	printf("Scripted start...\n");
	printf("Setup persistance region...\n");

	init_persistance_region();
	printf("persistance_region.factory_lamp_type = %d\n", persistance_region.factory_lamp_type);

	lamp_perform_type_test();
	display_splash_image();
	request_lamp_power(PWR_100PCT);
	
	//sleep_ms(1000);
	ui_loading_init();
	//ui_loading_update();
	//ui_loading_open();

	printf("Enter mainloop... xx\n");

	

	// sleep_ms(1000);

	
	ui_main_init();

	bool splash_screen = true;
	uint64_t last_buttons = 0;

	while (1) {
		update_sense();
		update_buttons();
		update_imu();
		update_mag();
		update_radar();
		update_usbpd();
		update_radio();
		update_lamp();

		curr_x = 0;
		curr_y = 0;

		if (splash_screen && buttons_pressed)
		{
			splash_screen = false;
		}
		else if (!splash_screen)
		{
			ui_main_update();
		}

		if (((time_us_64() - last_buttons) > (1000*1000*30)) && !buttons_pressed && !splash_screen)
		{
			last_buttons = time_us_64();
			display_splash_image();
			splash_screen = true;
		}

		if (buttons_pressed)
		{
			last_buttons = time_us_64();
		}

//		ui_main_update();
		ui_loading_update();
		lv_timer_handler();

		static int cycle= 0;
		printf("Mainloop... %d\n", cycle++);

		update_safety_logic();
	}
}
