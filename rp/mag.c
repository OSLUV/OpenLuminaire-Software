/**
 * @file      mag.c
 * @author    The OSLUV Project
 * @brief     Driver for magnet sensor
 * @hwref     U8 (TMAG5273)
 * @schematic lamp_controller.SchDoc
 *  
 */


/* Includes ------------------------------------------------------------------*/

#include <hardware/i2c.h>
#include <pico/stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "pins.h"


/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/

#define MAG_I2C_PORT_C          I2C_INST
#define MAG_IC_ADDR_C           0x35
#define MAG_REG_DEV_CFG_1_C     0x01
#define MAG_REG_DEV_CFG_2_C     0x02
#define MAG_REG_X_MSB_RES_C     0x12
#define MAG_REG_X_LSB_RES_C     0x13
#define MAG_REG_Y_MSB_RES_C     0x14
#define MAG_REG_Y_LSB_RES_C     0x15
#define MAG_REG_Z_MSB_RES_C     0x16
#define MAG_REG_Z_LSB_RES_C     0x17


/* Global variables  ---------------------------------------------------------*/

int16_t g_mag_x, g_mag_y, g_mag_z = 0;


/* Private variables  --------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/

static inline int mag_read(uint8_t dev_addr, int data_len, uint8_t* p_data_rd);
static inline int mag_write(uint8_t dev_addr, uint8_t data_wr);


/* Exported functions --------------------------------------------------------*/

/**
 * @brief Magnet sensor initialization procedure
 * 
 * @return 	void  
 * 
 */
void mag_init(void)
{
    mag_write(MAG_REG_DEV_CFG_1_C, (0x04 << 2) | (0x02 << 0));                  /* CONV_AVG: 16x average, I2C_RD: 1-byte I2C read command for 8 bit sensor MSB data and conversion status */
    mag_write(MAG_REG_DEV_CFG_2_C, (0x03 << 5) | (0x01 << 4));                  /* THR_HYST: ? , LP_LN: Low noise mode */
}

/**
 * @brief   Updates magnet sensor readings
 * 
 * @return 	void  
 */
void mag_update(void)
{
    uint8_t buf[2];

    mag_read(MAG_REG_X_MSB_RES_C, 2, buf);
    *(uint16_t*)&g_mag_x = (buf[0] << 8) | buf[1];

    mag_read(MAG_REG_Y_MSB_RES_C, 2, buf);
    *(uint16_t*)&g_mag_y = (buf[0] << 8) | buf[1];

    mag_read(MAG_REG_Z_MSB_RES_C, 2, buf);
    *(uint16_t*)&g_mag_z = (buf[0] << 8) | buf[1];
}


/* Private functions ---------------------------------------------------------*/

/**
 * @brief Reads multiple data bytes from magnet sensor
 * 
 * @param dev_addr IC device I2C address 
 * @param data_len Data length to read from device
 * @param p_data_rd Pointer to buffer where to deliver read data
 * @return int 1 - Read operation failed
 * @return int 0 - Read operation succeed
 */
static inline int mag_read(uint8_t dev_addr, int data_len, uint8_t* p_data_rd)
{
    if (i2c_write_timeout_us(MAG_I2C_PORT_C, MAG_IC_ADDR_C, &dev_addr, 1, true, 1000) <0)
    {
        printf("mag_read fail: addr\n");
        return 1;
    }

    if (i2c_read_timeout_us(MAG_I2C_PORT_C, MAG_IC_ADDR_C, p_data_rd, data_len, false, 1000) <0)
    {
        printf("mag_read fail: data\n");
        return 1;
    }

    return 0;
}

/**
 * @brief Writes a single data byte to magnet sensor
 * 
 * @param dev_addr IC device I2C address 
 * @param data_wr Data byte to write to device
 * @return int 1 - Write operation failed
 * @return int 0 - Write operation succeed
 */
static inline int mag_write(uint8_t dev_addr, uint8_t data_wr)
{
    uint8_t buf[] = {dev_addr, data_wr};

    if (i2c_write_timeout_us(MAG_I2C_PORT_C, MAG_IC_ADDR_C, buf, 2, false, 1000) <0)
    {
        printf("mag_write fail\n");
        return 1;
    }

    return 0;
}

/*** END OF FILE ***/
