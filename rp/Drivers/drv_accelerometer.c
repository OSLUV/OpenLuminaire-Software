/**
 * @file      drv_accelerometer.c
 * @author    The OSLUV Project
 * @brief     Driver for accelerometer device
 * @hwref     U7 (LIS3DHTR)
 * @schematic lamp_controller.SchDoc
 *  
 */


/* Includes ------------------------------------------------------------------*/

#include <hardware/i2c.h>
#include <pico/stdlib.h>

#include <stdio.h>
#include <stdint.h>
#include <math.h>

#include "Drivers/drv_i2c.h"


/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/

//#define _D_ACC_ENABLE_ADC_

#define D_ACC_IC_ADDR_C           0x19
#define D_ACC_REG_OUT_ADC1_L_C    0x08
#define D_ACC_REG_OUT_ADC1_H_C    0x09
#define D_ACC_REG_OUT_ADC2_L_C    0x0A
#define D_ACC_REG_OUT_ADC2_H_C    0x0B
#define D_ACC_REG_OUT_ADC3_L_C    0x0C
#define D_ACC_REG_OUT_ADC3_H_C    0x0D
#define D_ACC_REG_CTRL_REG_1_C    0x20
#define D_ACC_REG_CTRL_REG_4_C    0x23
#define D_ACC_REG_TEMP_CFG_REG_C  0x1F
#define D_ACC_REG_OUT_X_L_C       0x28
#define D_ACC_REG_OUT_X_H_C       0x29
#define D_ACC_REG_OUT_Y_L_C       0x2A
#define D_ACC_REG_OUT_Y_H_C       0x2B
#define D_ACC_REG_OUT_Z_L_C       0x2C
#define D_ACC_REG_OUT_Z_H_C       0x2D


/* Global variables  ---------------------------------------------------------*/

float g_drv_acc_x, g_drv_acc_y, g_drv_acc_z;


/* Private variables  --------------------------------------------------------*/

static float drv_acc_fx = 0.0f, drv_acc_fy = 0.0f, drv_acc_fz = 1.0f;           /* Filtered g-vector */


/* Private function prototypes -----------------------------------------------*/

static inline int drv_acc_read(uint8_t dev_addr, int data_len, uint8_t* p_data_rd);
static inline int drv_acc_write(uint8_t dev_addr, uint8_t data_wr);
#if defined(_D_ACC_ENABLE_ADC_)
static inline int16_t drv_acc_adjust_adc(int16_t adc_data);
#endif
static void drv_acc_read_data(uint8_t reg, float *p_data_val, bool b_is_accel);
static void drv_acc_calc_value(uint16_t raw_value, float *p_calc_value, bool b_is_accel);
static void drv_acc_read_raw_data(uint8_t reg, int16_t *p_data);


/* Exported functions --------------------------------------------------------*/

/**
 * @brief Accelerometer initialization procedure
 * 
 * @return 	void
 */
void drv_acc_init(void)
{
    drv_i2c_init();

    drv_acc_write(D_ACC_REG_CTRL_REG_1_C, (9 << 4) | (7 << 0));                 /* ODR: HR / normal (1.344 kHz) Low-power mode (5.376 kHz), Zen: Enabled, Yen: Enabled, Xen: Enabled */
    drv_acc_write(D_ACC_REG_CTRL_REG_4_C, (1 << 7));                            /* BDU: output registers not updated until MSB and LSB reading */
    drv_acc_write(D_ACC_REG_TEMP_CFG_REG_C, (1 << 7) | (0 << 6));               /* ADC_EN: Enabled, TEMP_EN: Disabled, */
}

/**
 * @brief   Updates accelerometer readings
 * 
 * @return 	void  
 */
void drv_acc_update(void)
{
    drv_acc_read_data(D_ACC_REG_OUT_X_L_C, &g_drv_acc_x, true);
    drv_acc_read_data(D_ACC_REG_OUT_Y_L_C, &g_drv_acc_y, true);
    drv_acc_read_data(D_ACC_REG_OUT_Z_L_C, &g_drv_acc_z, true);
	
	const float alpha = 0.05f;

    // one-pole low-pass:  y[n] = y[n-1] + α(x[n] – y[n-1])
    drv_acc_fx += alpha * (g_drv_acc_x - drv_acc_fx);
    drv_acc_fy += alpha * (g_drv_acc_y - drv_acc_fy);
    drv_acc_fz += alpha * (g_drv_acc_z - drv_acc_fz);

#if defined(_D_ACC_ENABLE_ADC_)
    int16_t adc1, adc2, adc3;

    drv_acc_read_raw_data(D_ACC_REG_OUT_ADC1_L_C, &adc1);
    drv_acc_read_raw_data(D_ACC_REG_OUT_ADC2_L_C, &adc2);
    drv_acc_read_raw_data(D_ACC_REG_OUT_ADC3_L_C, &adc3);

    int adc_r1 = drv_acc_adjust_adc(adc1);
    int adc_r2 = drv_acc_adjust_adc(adc2);
    int adc_r3 = drv_acc_adjust_adc(adc3);
#endif
}

/**
 * @brief Calculates the pointing down angle
 * 1/-1 Z and 1/-1 Y are the pointing-horizontal axes
 * 1 X is pointing down and -1X is pointing up
 * 
 * @return int 
 */
int drv_acc_get_pointing_down_angle(void)
{
    float x = drv_acc_fx;
    float y = drv_acc_fy;
    float z = drv_acc_fz;

    float mag = sqrtf((x * x) + (y * y) + (z * z));

    x /= mag;
    y /= mag;
    z /= mag;

    float angle = acosf((1 * x) + (0 * y) + (0 * z)) * (180.0 / M_PI);

    return angle;
}


/* Private functions ---------------------------------------------------------*/

/**
 * @brief Reads multiple data bytes from accelerometer device
 * 
 * @param dev_addr IC device I2C address 
 * @param data_len Data length to read from device
 * @param p_data_rd Pointer to buffer where to deliver read data
 * @return int 1 - Read operation failed
 * @return int 0 - Read operation succeed
 */
static inline int drv_acc_read(uint8_t dev_addr, int data_len, uint8_t* p_data_rd)
{
    if (drv_i2c_wr_tmout_us(D_ACC_IC_ADDR_C, &dev_addr, 1, true, 1000) < 0)
    {
        printf("drv_acc_read fail: addr\n");
        return 1;
    }

    if (drv_i2c_rd_tmout_us(D_ACC_IC_ADDR_C, p_data_rd, data_len, false, 1000) < 0)
    {
        printf("drv_acc_read fail: data\n");
        return 1;
    }

    return 0;
}

/**
 * @brief Writes a single data byte to accelerometer device
 * 
 * @param dev_addr IC device I2C address 
 * @param data_wr Data byte to write to device
 * @return int 1 - Write operation failed
 * @return int 0 - Write operation succeed
 */
static inline int drv_acc_write(uint8_t dev_addr, uint8_t data_wr)
{
    uint8_t buf[] = {dev_addr, data_wr};

    if (drv_i2c_wr_tmout_us(D_ACC_IC_ADDR_C, buf, 2, false, 1000) < 0)
    {
        printf("drv_acc_write fail\n");
        return 1;
    }

    return 0;
}

#if defined(_D_ACC_ENABLE_ADC_)
/**
 * @brief Adjusts raw ADC read data from accelerometer device
 * 
 * @return int16_t
 */
static inline int16_t drv_acc_adjust_adc(int16_t adc_data)
{
    return -(adc_data >> 6);
}
#endif

/**
 * @brief Reads two bytes of data from accelerometer device
 * 
 * @param reg Register address to read from device
 * @param p_data_val Pointer to calculated data value
 * @param b_is_accel Flag for accelereometer or temperature reading
 */
static void drv_acc_read_data(uint8_t reg, float *p_data_val, bool b_is_accel)
{
    uint8_t lsb;
    uint8_t msb;
    uint16_t raw_accel;
    drv_acc_read(reg, 1, &lsb);

    reg |= 0x01;
    drv_acc_read(reg, 1, &msb);

    raw_accel = (msb << 8) | lsb;

    drv_acc_calc_value(raw_accel, p_data_val, b_is_accel);
}

/**
 * @brief Convert with respect to the value being temperature or acceleration reading 
 * 
 * @param raw_value Raw read value to be converted
 * @param p_calc_value Pointer to put the calculated value
 * @param b_is_accel Flag for accelereometer or temperature reading
 */
static void drv_acc_calc_value(uint16_t raw_value, float *p_calc_value, bool b_is_accel)
{
    float scaling;
    float senstivity = 0.004f; // g per unit

    if (b_is_accel == true)
    {
        scaling = 64 / senstivity;
    } 
    else 
    {
        scaling = 64;
    }

    // raw_value is signed
    *p_calc_value = (float) ((int16_t) raw_value) / scaling;
}

#if defined(_D_ACC_ENABLE_ADC_)
/**
 * @brief Reads raw data from accelerometer device
 * 
 * @param reg Register address to read from
 * @param p_data Data read
 */
static void drv_acc_read_raw_data(uint8_t reg, int16_t *p_data)
{
    uint8_t lsb;
    uint8_t msb;
    drv_acc_read(reg, 1, &lsb);

    reg |= 0x01;
    drv_acc_read(reg, 1, &msb);

    *p_data = (msb << 8) | lsb;
}
#endif

/*** END OF FILE ***/
