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

const struct st7789_config lcd_config = {
		.spi      = spi0,
		.gpio_din = PIN_LCD_MOSI,
		.gpio_clk = PIN_LCD_SCK,
		.gpio_cs  = PIN_LCD_CS,
		.gpio_dc  = PIN_LCD_DC,
		.gpio_rst = PIN_LCD_RST,
		.gpio_bl  = PIN_LCD_BACKLIGHT,
};

#define LCD_WIDTH 240
#define LCD_HEIGHT 240
uint16_t screen_buffer[LCD_WIDTH][LCD_HEIGHT];

#include "font.c"

int curr_y = 0;
int curr_x = 0;

void draw_text(char* c, int nc)
{
	for (int ch = 0; ch < nc; ch++)
		{
			if (c[ch] == '\n')
			{
				curr_x = 0;
				curr_y += 8;
				continue;
			}

			for (int x = 0; x < 8; x++)
			{
				for (int y = 0; y < 8; y++)
				{
					int xo = curr_x + x;
					int yo = curr_y + y;
					if (xo >= LCD_WIDTH) continue;
					if (yo >= LCD_HEIGHT) continue;
					bool set = ((font8x8_basic[c[ch]][y] >> x) & 1);
					screen_buffer[yo][xo] = set?0xffff:0x0000;
					
				}
			}

			curr_x += 8;
		}
}

void draw_box(int x, int y, int w, int c)
{
	for (int dx = 0; dx < w; dx++)
	{
		for (int dy = 0; dy < w; dy++)
		{
			int xo = x + dx;
			int yo = y + dy;
			if (xo >= LCD_WIDTH) continue;
			if (yo >= LCD_HEIGHT) continue;
			screen_buffer[yo][xo] = c;
		}
	}
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


void main()
{
	stdio_init_all();

	gpio_init(4);
	gpio_init(5);
	gpio_init(6);
	gpio_set_dir(4, GPIO_IN);
	gpio_set_dir(5, GPIO_IN);
	gpio_set_dir(6, GPIO_IN);
	
	st7789_init(&lcd_config, LCD_WIDTH, LCD_HEIGHT);
	st7789_set_backlight(50);
	st7789_blit_screen(LCD_WIDTH*LCD_HEIGHT, (uint16_t*)gimp_image.pixel_data);

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

	printf("Scripted start...\n");
	set_switched_12v(true);
	set_lamp(true, 0);


	bool splash_screen = true;

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
			memset(screen_buffer, 0x01, sizeof(screen_buffer));
			do_menu();
			st7789_blit_screen(LCD_WIDTH*LCD_HEIGHT, (uint16_t*)screen_buffer);
		}
	}
}
