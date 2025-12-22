/**
 * @file      mag.h
 * @author    The OSLUV Project
 * @brief     Functions prototypes for magnet sensor driver
 *  
 */

#ifndef _PICO_ST7789_H_
#define _PICO_ST7789_H_


/* Exported includes ---------------------------------------------------------*/

#include "hardware/spi.h"


/* Expoerted typedef ---------------------------------------------------------*/

/**
 * @brief st7789_config
 * 
 */
struct st7789_config {
    spi_inst_t* spi;
    uint gpio_din;
    uint gpio_clk;
    int gpio_cs;
    uint gpio_dc;
    uint gpio_rst;
    uint gpio_bl;
};


/* Exported functions prototypes ---------------------------------------------*/

void st7789_init(const struct st7789_config* config, uint16_t width, uint16_t height);
void st7789_write(const void* data, size_t len);
void st7789_put(uint16_t pixel);
void st7789_fill(uint16_t pixel);
void st7789_set_cursor(uint16_t x, uint16_t y);
void st7789_vertical_scroll(uint16_t row);
void st7789_blit_screen(int pixels, uint16_t* buf);
bool st7789_blit_done(void);
void st7789_set_backlight(int bl);
int st7789_get_backlight(void);


#endif /* _PICO_ST7789_H_ */

/*** END OF FILE ***/
