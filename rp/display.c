#include <stdio.h>
#include <pico/stdlib.h>
#include <hardware/irq.h>
#include <hardware/spi.h>
#include <hardware/pwm.h>
#include <drivers/display/st7796/lv_st7796.h>
#include <lvgl.h>

#include "pins.h"

#define LCD_W 240
#define LCD_H 240
#define LCD_SPI spi0
#define BACKLIGHT_WRAP           1000      // counter counts 0…1000  (≈1 kHz)
#define BACKLIGHT_MAX_BRIGHTNESS 100       // user-facing range 0-100

static void backlight_pwm_init(uint8_t brightness_percent)
{
    if (brightness_percent > BACKLIGHT_MAX_BRIGHTNESS)
        brightness_percent = BACKLIGHT_MAX_BRIGHTNESS;

    gpio_set_function(PIN_LCD_BACKLIGHT, GPIO_FUNC_PWM);

    uint slice   = pwm_gpio_to_slice_num(PIN_LCD_BACKLIGHT);
    uint channel = pwm_gpio_to_channel(PIN_LCD_BACKLIGHT);

    pwm_config cfg = pwm_get_default_config();

    /* 125 MHz / 125 = 1 MHz → 1 MHz / (BACKLIGHT_WRAP+1) ≈ 999 Hz */
    pwm_config_set_clkdiv(&cfg, 125.0f);

    pwm_init(slice, &cfg, false);
    pwm_set_wrap(slice, BACKLIGHT_WRAP);

    uint16_t level = brightness_percent * BACKLIGHT_WRAP / BACKLIGHT_MAX_BRIGHTNESS;
    pwm_set_chan_level(slice, channel, level);

    pwm_set_enabled(slice, true);
}


void backlight_set_brightness(uint8_t brightness_percent)
{
    if (brightness_percent > BACKLIGHT_MAX_BRIGHTNESS)
        brightness_percent = BACKLIGHT_MAX_BRIGHTNESS;

    uint slice   = pwm_gpio_to_slice_num(PIN_LCD_BACKLIGHT);
    uint channel = pwm_gpio_to_channel(PIN_LCD_BACKLIGHT);

    uint16_t level = brightness_percent * BACKLIGHT_WRAP / BACKLIGHT_MAX_BRIGHTNESS;
    pwm_set_chan_level(slice, channel, level);
}


void st7796_config_spi(int data_bits)
{
	spi_set_format(LCD_SPI, data_bits, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
}

void st7796_send_internal(bool is_color,
							const uint8_t * cmd, size_t cmd_size,
							const uint8_t * param, size_t param_size)
{
	// printf("st7796_send_%s(cmd %db, param %db)\n", is_color?"color":"cmd", cmd_size, param_size);

	gpio_put(PIN_LCD_DC, 0);
	gpio_put(PIN_LCD_CS, 0);

	st7796_config_spi(8);
	spi_write_blocking(LCD_SPI, cmd, cmd_size);
	
	gpio_put(PIN_LCD_DC, 1);
	
	if (!is_color)
	{
		spi_write_blocking(LCD_SPI, param, param_size);
	}
	else
	{
		st7796_config_spi(16);
		spi_write16_blocking(LCD_SPI, (uint16_t*)param, param_size/2);
	}

	gpio_put(PIN_LCD_CS, 1);
}

void st7796_send_cmd(lv_display_t * disp, 
						const uint8_t * cmd, size_t cmd_size,
						const uint8_t * param, size_t param_size)
{
	st7796_send_internal(false, cmd, cmd_size, param, param_size);
}

void st7796_send_color(lv_display_t * disp,
						const uint8_t * cmd, size_t cmd_size,
						uint8_t * param, size_t param_size)
{
	// TODO: Do this asynchronously

	// uint16_t* p = (uint16_t*)param;
	// for (int i = 0; i < param_size/2; i++)
	// {
	// 	uint16_t c = *p;
	// 	uint16_t swap = 0;
	// 	swap = c & 0x07E0;				// green
	// 	swap |= (c >> 11) & 0x1F;		// red
	// 	swap |= (c & 0x1F) << 11;       // blue
	// 	*p = swap;
	// 	p++;
	// }

	st7796_send_internal(true, cmd, cmd_size, param, param_size);
	lv_display_flush_ready(disp);

	// write_output(PIN_LCD_BACKLIGHT, 3<<9);
}

uint32_t my_tick(void)
{
	return time_us_64() / 1000;
}

uint8_t draw_buffer[LCD_W * LCD_H * sizeof(uint16_t) / 2];

static void keypad_init(void);
static void keypad_read(lv_indev_t * indev, lv_indev_data_t * data);
static uint32_t keypad_get_key(void);

lv_indev_t * indev_keypad;
lv_group_t * the_group;

void display_init()
{
	gpio_init(PIN_LCD_CS); // Chip Select
	gpio_set_dir(PIN_LCD_CS, GPIO_OUT);
	gpio_put(PIN_LCD_CS, 1);

	gpio_init(PIN_LCD_RST); // Reset
	gpio_set_dir(PIN_LCD_RST, GPIO_OUT);
	gpio_put(PIN_LCD_RST, 1);

	// gpio_init(LCD_PIN_RDX); // Parallel Read Enable (unused)
	// gpio_set_dir(LCD_PIN_RDX, GPIO_OUT);
	// gpio_put(LCD_PIN_RDX, 1);

	gpio_init(PIN_LCD_DC); // Data/command
	gpio_set_dir(PIN_LCD_DC, GPIO_OUT);
	gpio_put(PIN_LCD_DC, 1);

	spi_init(LCD_SPI, 64 * 1000 * 1000);
	gpio_set_function(PIN_LCD_MOSI, GPIO_FUNC_SPI);
	gpio_set_function(PIN_LCD_SCK, GPIO_FUNC_SPI);
	st7796_config_spi(8);

	printf("Reset device...\n");
	sleep_ms(10);
	gpio_put(PIN_LCD_RST, 0);
	sleep_ms(15);
	gpio_put(PIN_LCD_RST, 1);
	sleep_ms(15);
	
	/* Old Code
	// init_output(PIN_LCD_BACKLIGHT);
	// write_output(PIN_LCD_BACKLIGHT, 0); // Leave off until LVGL is ready
	// write_output(PIN_LCD_BACKLIGHT, 3<<9);

	//gpio_init(PIN_LCD_BACKLIGHT);
	//gpio_set_dir(PIN_LCD_BACKLIGHT, GPIO_OUT);
	//gpio_put(PIN_LCD_BACKLIGHT, 1);
	*/

	// --- Back-light Init PWM & Default Setting
	backlight_pwm_init(0); // 50%

	printf("Config LVGL...\n");
	lv_tick_set_cb(my_tick);

	printf("Create ST7796...\n");
	lv_display_t * disp = lv_st7796_create(
					LCD_W,  LCD_H, 0,
					//LV_LCD_FLAG_BGR, 
					st7796_send_cmd, st7796_send_color);
	static const uint8_t invon = 0x21;
	st7796_send_cmd(disp, &invon, 1, NULL, 0);
	lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_0);
	lv_display_set_buffers(disp, draw_buffer, NULL, sizeof(draw_buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
	
	indev_keypad = lv_indev_create();
    lv_indev_set_type(indev_keypad, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(indev_keypad, keypad_read);

    the_group = lv_group_create();
    lv_indev_set_group(indev_keypad, the_group);
}

#include "buttons.h"

static void keypad_read(lv_indev_t * indev_drv, lv_indev_data_t * data)
{
	static int last_key = 0;

	if (buttons_down)
	{
		int bit = 0;
		while (bit < 5)
		{
			if (buttons_down & (1<<bit))
			{
				break;
			}
			bit++;
		}

		data->state = LV_INDEV_STATE_PRESSED;

		/*const int key_map[] = {
			[BUTTON_LEFT] = LV_KEY_LEFT,
			[BUTTON_RIGHT] = LV_KEY_RIGHT,
			[BUTTON_UP] = LV_KEY_PREV,
			[BUTTON_DOWN] = LV_KEY_NEXT,
			[BUTTON_CENTER] = LV_KEY_ENTER
		};*/
		static const uint8_t key_map[5] = {
			LV_KEY_PREV,      /* bit 2  (BUTTON_UP)     */
			LV_KEY_NEXT,      /* bit 3  (BUTTON_DOWN)   */
			LV_KEY_LEFT,      /* bit 0  (BUTTON_LEFT)   */
			LV_KEY_RIGHT,     /* bit 1  (BUTTON_RIGHT)  */
			LV_KEY_ENTER      /* bit 4  (BUTTON_CENTER) */
		};

		last_key = key_map[bit];

		printf("Produce PRESSED %d\n", last_key);
	}
	else
	{
		data->state = LV_INDEV_STATE_RELEASED;
		printf("Produce RELEASED %d\n", last_key);
	}

	data->key = last_key;
	
}
