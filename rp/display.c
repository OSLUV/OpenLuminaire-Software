/**
 * @file      display.c
 * @author    The OSLUV Project
 * @brief     Driver for magnet sensor
 * @hwref     J12 (ST7796)
 * @schematic lamp_controller.SchDoc
 *  
 */


/* Includes ------------------------------------------------------------------*/

#include <stdio.h>
#include <pico/stdlib.h>
#include <hardware/irq.h>
#include <hardware/spi.h>
#include <hardware/pwm.h>
#include <drivers/display/st7796/lv_st7796.h>
#include <lvgl.h>
#include "pins.h"
#include "buttons.h"


/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/

#define DISPLAY_LCD_WIDTH_C 				240
#define DISPLAY_LCD_HEIGHT_C 				240
#define DISPLAY_LCD_SPI_PORT_C 				spi0
#define DISPLAY_BACKLIGHT_WRAP_C           	1000      							/* Counter counts 0…1000  (≈1 kHz) */
#define DISPLAY_BACKLIGHT_MAX_BRIGHTNESS_C 	100       							/* User-facing range 0-100 */
#define DISPLAY_STARTUP_BRIGHTNESS_C		0									/* In percentage (%) */
#define DISPLAY_TURN_ON_BRIGHTNESS_C		33
#define DISPLAY_BUF_SIZE_C					DISPLAY_LCD_WIDTH_C  * \
											DISPLAY_LCD_HEIGHT_C * \
											sizeof(uint16_t) / 2


/* Global variables  ---------------------------------------------------------*/
/* Private variables  --------------------------------------------------------*/

static uint8_t display_draw_buffer[DISPLAY_BUF_SIZE_C];

lv_indev_t * indev_keypad;
lv_group_t * the_group;


/* Callback prototypes -------------------------------------------------------*/

uint32_t display_lvgl_tick_callback(void);
void display_read_keypad_callback(lv_indev_t * p_indev, lv_indev_data_t * p_data);


/* Private function prototypes -----------------------------------------------*/

static void display_set_init_config(void);
static void display_driver_config_spi(int data_bits);
static void display_driver_send_cmd(bool is_color,
								 const uint8_t* p_cmd, size_t cmd_size,
								 const uint8_t* p_param, size_t param_size);
static void display_send_cmd(lv_display_t* p_disp, 
							const uint8_t* p_cmd, size_t cmd_size,
							const uint8_t* p_param, size_t param_size);
static void display_set_color(lv_display_t* p_disp,
							  const uint8_t* p_cmd, size_t cmd_size,
							  uint8_t* p_param, size_t param_size);


/* Exported functions --------------------------------------------------------*/

/**
 * @brief Display module initialization procedure
 * 
 */
void display_init(void)
{
	static const uint8_t invon = 0x21;

	/* Chip select pin */
	gpio_init(PIN_LCD_CS);
	gpio_set_dir(PIN_LCD_CS, GPIO_OUT);
	gpio_put(PIN_LCD_CS, 1);

	/* Reset pin */
	gpio_init(PIN_LCD_RST);
	gpio_set_dir(PIN_LCD_RST, GPIO_OUT);
	gpio_put(PIN_LCD_RST, 1);

	/* Data/command pin */
	gpio_init(PIN_LCD_DC);
	gpio_set_dir(PIN_LCD_DC, GPIO_OUT);
	gpio_put(PIN_LCD_DC, 1);

	spi_init(DISPLAY_LCD_SPI_PORT_C, 64 * 1000 * 1000);
	gpio_set_function(PIN_LCD_MOSI, GPIO_FUNC_SPI);
	gpio_set_function(PIN_LCD_SCK, GPIO_FUNC_SPI);
	display_driver_config_spi(8);

	printf("Reset device...\n");
	sleep_ms(10);
	gpio_put(PIN_LCD_RST, 0);
	sleep_ms(15);
	gpio_put(PIN_LCD_RST, 1);
	sleep_ms(15);
	
	/* Back-light Init PWM & Default Setting */
	display_set_init_config();

	printf("Config LVGL...\n");
	lv_tick_set_cb(display_lvgl_tick_callback);

	printf("Create ST7796...\n");
	lv_display_t * disp = lv_st7796_create(DISPLAY_LCD_WIDTH_C,  
										   DISPLAY_LCD_HEIGHT_C, 
										   0, //LV_LCD_FLAG_BGR, 
										   display_send_cmd, 
										   display_set_color);
	
	display_send_cmd(disp, &invon, 1, NULL, 0);
	lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_0);
	lv_display_set_buffers(disp, 
						   display_draw_buffer,
						   NULL, 
						   sizeof(display_draw_buffer), 
						   LV_DISPLAY_RENDER_MODE_PARTIAL);
	
	indev_keypad = lv_indev_create();
    lv_indev_set_type(indev_keypad, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(indev_keypad, display_read_keypad_callback);
}

/**
 * @brief Sets the display's backlight brightness level
 * 
 * @param brightness	Brightness iun percentage 
 */
void display_set_backlight_brightness(uint8_t brightness)
{
	uint 	 slice;
	uint 	 channel;
	uint16_t pwm_level;

	if (brightness > DISPLAY_BACKLIGHT_MAX_BRIGHTNESS_C)
	{
		brightness = DISPLAY_BACKLIGHT_MAX_BRIGHTNESS_C;
	}

	slice   = pwm_gpio_to_slice_num(PIN_LCD_BACKLIGHT);
	channel = pwm_gpio_to_channel(PIN_LCD_BACKLIGHT);

	pwm_level = brightness * 
			  	DISPLAY_BACKLIGHT_WRAP_C / DISPLAY_BACKLIGHT_MAX_BRIGHTNESS_C;

	pwm_set_chan_level(slice, channel, pwm_level);
}

/**
 * @brief Turns off the display by setting its backlight brightness to 0%
 * 
 */
void display_screen_off(void)
{
    display_set_backlight_brightness(0);
}

/**
 * @brief Turns on the display by setting its backlight brightness to 
 * @ref DISPLAY_TURN_ON_BRIGHTNESS_C
 * 
 */
void display_screen_on(void)
{
    display_set_backlight_brightness(DISPLAY_TURN_ON_BRIGHTNESS_C);
    lv_timer_handler();               											/* One flush so image shows */
}

/**
 * @brief 
 * 
 * @param p_group 
 */
void display_set_indev_group(lv_group_t* p_group)
{
	lv_indev_set_group(indev_keypad, p_group);
}


/* Callback functions --------------------------------------------------------*/

uint32_t display_lvgl_tick_callback(void)
{
	return time_us_64() / 1000;
}


void display_read_keypad_callback(lv_indev_t * p_indev_drv, lv_indev_data_t * p_data)
{
	static int last_key = 0;

	if (g_buttons_down)
	{
		int bit = 0;
		while (bit < 5)
		{
			if (g_buttons_down & (1 << bit))
			{
				break;
			}
			bit++;
		}

		p_data->state = LV_INDEV_STATE_PRESSED;

		static const uint8_t key_map[5] = {
			LV_KEY_PREV,      /* bit 2  (BUTTON_UP)     */
			LV_KEY_NEXT,      /* bit 3  (BUTTON_DOWN)   */
			LV_KEY_LEFT,      /* bit 0  (BUTTON_LEFT)   */
			LV_KEY_RIGHT,     /* bit 1  (BUTTON_RIGHT)  */
			LV_KEY_ENTER      /* bit 4  (BUTTON_CENTER) */
		};

		last_key = key_map[bit];

		// printf("Produce PRESSED %d\n", last_key);
	}
	else
	{
		p_data->state = LV_INDEV_STATE_RELEASED;
		// printf("Produce RELEASED %d\n", last_key);
	}

	p_data->key = last_key;
}


/* Private functions ---------------------------------------------------------*/

/**
 * @brief Sets configuration for display control
 * 
 */
static void display_set_init_config(void)
{
    uint 		slice;
    pwm_config 	pwm_cfg;

    gpio_set_function(PIN_LCD_BACKLIGHT, GPIO_FUNC_PWM);

    slice   = pwm_gpio_to_slice_num(PIN_LCD_BACKLIGHT);

    pwm_cfg = pwm_get_default_config();

    /* 125 MHz / 125 = 1 MHz → 1 MHz / (DISPLAY_BACKLIGHT_WRAP_C+1) ≈ 999 Hz */
    pwm_config_set_clkdiv(&pwm_cfg, 125.0f);

    pwm_init(slice, &pwm_cfg, false);
    pwm_set_wrap(slice, DISPLAY_BACKLIGHT_WRAP_C);

    display_set_backlight_brightness(DISPLAY_STARTUP_BRIGHTNESS_C);

    pwm_set_enabled(slice, true);
}

/**
 * @brief Configures ST7796 driver's SPI mode
 * 
 * @param data_bits 
 */
static void display_driver_config_spi(int data_bits)
{
	spi_set_format(DISPLAY_LCD_SPI_PORT_C, data_bits, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
}

/**
 * @brief Sends data command to display driver
 * 
 * @param is_color 		Color control
 * @param p_cmd 		Command to send
 * @param cmd_size 		Command data size
 * @param p_param 		Parameters to send
 * @param param_size 	Parameters data size
 */
static void display_driver_send_cmd(bool is_color,
								 const uint8_t* p_cmd, size_t cmd_size,
								 const uint8_t* p_param, size_t param_size)
{
	// printf("st7796_send_%s(cmd %db, param %db)\n", is_color?"color":"cmd", cmd_size, param_size);

	gpio_put(PIN_LCD_DC, 0);
	gpio_put(PIN_LCD_CS, 0);

	display_driver_config_spi(8);
	spi_write_blocking(DISPLAY_LCD_SPI_PORT_C, p_cmd, cmd_size);
	
	gpio_put(PIN_LCD_DC, 1);
	
	if (!is_color)
	{
		spi_write_blocking(DISPLAY_LCD_SPI_PORT_C, p_param, param_size);
	}
	else
	{
		display_driver_config_spi(16);
		spi_write16_blocking(DISPLAY_LCD_SPI_PORT_C, 
							 (uint16_t*)p_param, param_size / 2);
	}

	gpio_put(PIN_LCD_CS, 1);
}

/**
 * @brief Sends a command to a display
 * 
 * @param p_disp 		Selected display
 * @param p_cmd 		Command to send
 * @param cmd_size 		Command data size
 * @param p_param 		Parameters to send
 * @param param_size 	Parameters data size
 */
static void display_send_cmd(lv_display_t* p_disp, 
							 const uint8_t* p_cmd, size_t cmd_size,
							 const uint8_t* p_param, size_t param_size)
{
	display_driver_send_cmd(false, p_cmd, cmd_size, p_param, param_size);
}

/**
 * @brief Sends a color setting command to display
 * 
 * @param p_disp  		Selected display
 * @param p_cmd 		Command to send
 * @param cmd_size 		Command data size
 * @param p_param 		Parameters to send
 * @param param_size 	Parameters data size
 */
static void display_set_color(lv_display_t* p_disp,
							  const uint8_t* p_cmd, size_t cmd_size,
							  uint8_t* p_param, size_t param_size)
{
	// TODO: Do this asynchronously

	display_driver_send_cmd(true, p_cmd, cmd_size, p_param, param_size);
	lv_display_flush_ready(p_disp);
}


/*** END OF FILE ***/