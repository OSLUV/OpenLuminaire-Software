/**
 * @file      st7789.c
 * @author    The OSLUV Project
 * @brief     Driver display driver control
 *  
 */


/* Includes ------------------------------------------------------------------*/

#include <string.h>

#include "hardware/gpio.h"
#include "hardware/dma.h"
#include <pico/stdlib.h>
#include <hardware/pwm.h>

#include "st7789.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/

#define STEPCOUNT_BL 1000


/* Global variables  ---------------------------------------------------------*/
/* Private variables  --------------------------------------------------------*/

static struct st7789_config st7789_cfg;
static uint16_t             st7789_width;
static uint16_t             st7789_height;
static bool                 st7789_data_mode = false;
static uint                 st7789_dma_chno = 0;
static int                  st7789_curr_bl = 0;


/* Callback prototypes -------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/

static void st7789_cmd(uint8_t cmd, const uint8_t* data, size_t len);
static void st7789_caset(uint16_t xs, uint16_t xe);
static void st7789_raset(uint16_t ys, uint16_t ye);
static void st7789_ramwr(void);


/* Exported functions --------------------------------------------------------*/

/**
 * @brief ST7789 driver initialization procedure
 * 
 * @param config    @ref st7789_config
 * @param width     Display pixels width
 * @param height    Display pixels height
 */
void st7789_init(const struct st7789_config* config, uint16_t width, uint16_t height)
{
    memcpy(&st7789_cfg, config, sizeof(st7789_cfg));
    st7789_width  = width;
    st7789_height = height;

    spi_init(st7789_cfg.spi, 125 * 1000 * 1000);
    if (st7789_cfg.gpio_cs > -1)
    {
        spi_set_format(st7789_cfg.spi, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    }
    else
    {
        spi_set_format(st7789_cfg.spi, 8, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST);
    }

    gpio_set_function(st7789_cfg.gpio_din, GPIO_FUNC_SPI);
    gpio_set_function(st7789_cfg.gpio_clk, GPIO_FUNC_SPI);

    if (st7789_cfg.gpio_cs > -1)
    {
        gpio_init(st7789_cfg.gpio_cs);
    }
    gpio_init(st7789_cfg.gpio_dc);
    gpio_init(st7789_cfg.gpio_rst);
    gpio_init(st7789_cfg.gpio_bl);

    if (st7789_cfg.gpio_cs > -1)
    {
        gpio_set_dir(st7789_cfg.gpio_cs, GPIO_OUT);
    }
    gpio_set_dir(st7789_cfg.gpio_dc, GPIO_OUT);
    gpio_set_dir(st7789_cfg.gpio_rst, GPIO_OUT);
    gpio_set_dir(st7789_cfg.gpio_bl, GPIO_OUT);

    if (st7789_cfg.gpio_cs > -1)
    {
        gpio_put(st7789_cfg.gpio_cs, 1);
    }
    gpio_put(st7789_cfg.gpio_dc, 1);
    gpio_put(st7789_cfg.gpio_rst, 1);
    sleep_ms(100);
    
    // SWRESET (01h): Software Reset
    st7789_cmd(0x01, NULL, 0);
    sleep_ms(150);

    // SLPOUT (11h): Sleep Out
    st7789_cmd(0x11, NULL, 0);
    sleep_ms(50);

    // COLMOD (3Ah): Interface Pixel Format
    // - RGB interface color format     = 65K of RGB interface
    // - Control interface color format = 16bit/pixel
    st7789_cmd(0x3a, (uint8_t[]){ 0x55 }, 1);
    sleep_ms(10);

    // MADCTL (36h): Memory Data Access Control
    // - Page Address Order            = Top to Bottom
    // - Column Address Order          = Left to Right
    // - Page/Column Order             = Normal Mode
    // - Line Address Order            = LCD Refresh Top to Bottom
    // - RGB/BGR Order                 = RGB
    // - Display Data Latch Data Order = LCD Refresh Left to Right
    st7789_cmd(0x36, (uint8_t[]){ 0x00 }, 1);
   
    st7789_caset(0, width);
    st7789_raset(0, height);

    // INVON (21h): Display Inversion On
    st7789_cmd(0x21, NULL, 0);
    sleep_ms(10);

    // NORON (13h): Normal Display Mode On
    st7789_cmd(0x13, NULL, 0);
    sleep_ms(10);

    // DISPON (29h): Display On
    st7789_cmd(0x29, NULL, 0);
    sleep_ms(10);

    gpio_set_function(st7789_cfg.gpio_bl, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(st7789_cfg.gpio_bl);

    pwm_config pwmconfig = pwm_get_default_config();
    // Set divider, reduces counter clock to sysclock/this value
    pwm_config_set_clkdiv(&pwmconfig, 125);

    pwm_config_set_wrap(&pwmconfig, STEPCOUNT_BL); // 1kHz

    // Load the configuration into our PWM slice, and set it running.
    pwm_init(slice_num, &pwmconfig, false);

    pwm_set_gpio_level(st7789_cfg.gpio_bl, 0);

    pwm_set_enabled(slice_num, true);

    st7789_dma_chno = dma_claim_unused_channel(true);
}

/**
 * @brief Sends data to display driver
 * 
 * @param data  Data buffer
 * @param len   Data length
 */
void st7789_write(const void* data, size_t len)
{
    if (!st7789_data_mode)
    {
        st7789_ramwr();

        if (st7789_cfg.gpio_cs > -1)
        {
            spi_set_format(st7789_cfg.spi, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
        }
        else 
        {
            spi_set_format(st7789_cfg.spi, 16, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST);
        }

        st7789_data_mode = true;
    }

    spi_write16_blocking(st7789_cfg.spi, data, len / 2);
}

/**
 * @brief Sets a pixel
 * 
 * @param pixel 
 */
void st7789_put(uint16_t pixel)
{
    st7789_write(&pixel, sizeof(pixel));
}

/**
 * @brief Fills the display with the same pixel
 * 
 * @param pixel 
 */
void st7789_fill(uint16_t pixel)
{
    int num_pixels = st7789_width * st7789_height;

    st7789_set_cursor(0, 0);

    for (int i = 0; i < num_pixels; i++)
    {
        st7789_put(pixel);
    }
}

/**
 * @brief Sets the cursor position on display
 * 
 * @param x 
 * @param y 
 */
void st7789_set_cursor(uint16_t x, uint16_t y)
{
    st7789_caset(x, st7789_width);
    st7789_raset(y, st7789_height);
}

/**
 * @brief 
 * 
 * @param row 
 */
void st7789_vertical_scroll(uint16_t row)
{
    uint8_t data[] = {
        (row >> 8) & 0xff,
        row & 0x00ff
    };

    // VSCSAD (37h): Vertical Scroll Start Address of RAM 
    st7789_cmd(0x37, data, sizeof(data));
}

/**
 * @brief 
 * 
 * @param pixels 
 * @param buf 
 */
void st7789_blit_screen(int pixels, uint16_t* buf)
{
    st7789_set_cursor(0, 0);

    st7789_ramwr();
    spi_set_format(st7789_cfg.spi, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    st7789_data_mode = true;

    dma_channel_config cfg = dma_channel_get_default_config(st7789_dma_chno);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
    channel_config_set_read_increment(&cfg, true);
    channel_config_set_write_increment(&cfg, false);

    channel_config_set_dreq(&cfg, spi_get_dreq(st7789_cfg.spi, true));

    dma_channel_configure(st7789_dma_chno, &cfg,
        &spi_get_hw(st7789_cfg.spi)->dr, // dst
        buf,
        pixels,
        true
    );

    while (dma_channel_is_busy(st7789_dma_chno))
    {
        sleep_us(100);
    }
}

/**
 * @brief 
 * 
 * @return true 
 * @return false 
 */
bool st7789_blit_done(void)
{
    return !dma_channel_is_busy(st7789_dma_chno);
}

/**
 * @brief Sets display's backligh bright
 * 
 * @param bl 
 */
void st7789_set_backlight(int bl)
{
    if (bl > 100)
    {
        bl = 100;
    }
    if (bl < 0)
    {
        bl = 0;
    }
    
    pwm_set_gpio_level(st7789_cfg.gpio_bl, bl * STEPCOUNT_BL / 100);
    st7789_curr_bl = bl;
}

/**
 * @brief Returns the current display's backligh bright
 * 
 * @return int 
 */
int st7789_get_backlight(void)
{
    return st7789_curr_bl;
}


/* Callback functions --------------------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/**
 * @brief Sends a command to driver
 * 
 * @param cmd   Command
 * @param data  Data command
 * @param len   Data length
 */
static void st7789_cmd(uint8_t cmd, const uint8_t* data, size_t len)
{
    if (st7789_cfg.gpio_cs > -1)
    {
        spi_set_format(st7789_cfg.spi, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    }
    else
    {
        spi_set_format(st7789_cfg.spi, 8, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST);
    }
    st7789_data_mode = false;

    sleep_us(1);
    if (st7789_cfg.gpio_cs > -1)
    {
        gpio_put(st7789_cfg.gpio_cs, 0);
    }
    gpio_put(st7789_cfg.gpio_dc, 0);
    sleep_us(1);
    
    spi_write_blocking(st7789_cfg.spi, &cmd, sizeof(cmd));
    
    if (len)
    {
        sleep_us(1);
        gpio_put(st7789_cfg.gpio_dc, 1);
        sleep_us(1);
        
        spi_write_blocking(st7789_cfg.spi, data, len);
    }

    sleep_us(1);
    if (st7789_cfg.gpio_cs > -1)
    {
        gpio_put(st7789_cfg.gpio_cs, 1);
    }
    gpio_put(st7789_cfg.gpio_dc, 1);
    sleep_us(1);
}

/**
 * @brief Sets column address
 * 
 * @param xs 
 * @param xe 
 */
static void st7789_caset(uint16_t xs, uint16_t xe)
{
    uint8_t data[] = {
        xs >> 8,
        xs & 0xff,
        xe >> 8,
        xe & 0xff,
    };

    // CASET (2Ah): Column Address Set
    st7789_cmd(0x2a, data, sizeof(data));
}

/**
 * @brief Sets row address
 * 
 * @param ys 
 * @param ye 
 */
static void st7789_raset(uint16_t ys, uint16_t ye)
{
    uint8_t data[] = {
        ys >> 8,
        ys & 0xff,
        ye >> 8,
        ye & 0xff,
    };

    // RASET (2Bh): Row Address Set
    st7789_cmd(0x2b, data, sizeof(data));
}

/**
 * @brief 
 * 
 */
static void st7789_ramwr(void)
{
    sleep_us(1);
    if (st7789_cfg.gpio_cs > -1)
    {
        gpio_put(st7789_cfg.gpio_cs, 0);
    }
    gpio_put(st7789_cfg.gpio_dc, 0);
    sleep_us(1);

    // RAMWR (2Ch): Memory Write
    uint8_t cmd = 0x2c;
    spi_write_blocking(st7789_cfg.spi, &cmd, sizeof(cmd));

    sleep_us(1);
    if (st7789_cfg.gpio_cs > -1)
    {
        gpio_put(st7789_cfg.gpio_cs, 0);
    }
    gpio_put(st7789_cfg.gpio_dc, 1);
    sleep_us(1);
}


/*** END OF FILE ***/