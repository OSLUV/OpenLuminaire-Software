/**
 * @file      mag.c
 * @author    The OSLUV Project
 * @brief     Driver for magnet sensor
 * @hwref     U8 (TMAG5273A2QDBVR)
 * @ref       lamp_controller.SchDoc
 *  
 */


/* Includes ------------------------------------------------------------------*/

#include <hardware/i2c.h>
#include <pico/stdlib.h>

#include <stdio.h>
#include <stdint.h>

#include "pins.h"

#define MAG_ADDR 0x35
#define MAG_I2C I2C_INST

static inline int mag_read(uint8_t addr_l, int len, uint8_t* out)
{
    if (i2c_write_timeout_us(MAG_I2C, MAG_ADDR, &addr_l, 1, true, 1000) <0)
    {
        printf("mag_read fail: addr\n");
        return 1;
    }

    if (i2c_read_timeout_us(MAG_I2C, MAG_ADDR, out, len, false, 1000) <0)
    {
        printf("mag_read fail: data\n");
        return 1;
    }

    return 0;
}

static inline int mag_write(uint8_t addr_l, uint8_t value)
{
    uint8_t buf[] = {addr_l, value};

    if (i2c_write_timeout_us(MAG_I2C, MAG_ADDR, buf, 2, false, 1000) <0)
    {
        printf("mag_write fail\n");
        return 1;
    }

    return 0;
}

void init_mag()
{
    mag_write(0x01, 1<<4 | 2);
    mag_write(0x02, 7<<4);
}

int16_t mag_x, mag_y, mag_z = 0;

void update_mag()
{
    uint8_t buf[2];

    mag_read(0x12, 2, buf);
    *(uint16_t*)&mag_x = buf[0] << 8 | buf[1];

    mag_read(0x14, 2, buf);
    *(uint16_t*)&mag_y = buf[0] << 8 | buf[1];

    mag_read(0x16, 2, buf);
    *(uint16_t*)&mag_z = buf[0] << 8 | buf[1];

}
