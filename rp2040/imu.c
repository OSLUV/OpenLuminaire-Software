#include <hardware/i2c.h>
#include <pico/stdlib.h>

#include <stdio.h>
#include <stdint.h>

#include "pins.h"

#define IMU_ADDR 0x19
#define IMU_I2C I2C_INST
#define IMU_REG_CTRL_REG_1 0x20
#define IMU_REG_CTRL_REG_4 0x23
#define IMU_REG_TEMP_CFG_REG 0x1f

static inline int imu_read(uint8_t addr_l, int len, uint8_t* out)
{
    if (i2c_write_timeout_us(IMU_I2C, IMU_ADDR, &addr_l, 1, true, 1000) <0)
    {
        printf("imu_read fail: addr\n");
        return 1;
    }

    if (i2c_read_timeout_us(IMU_I2C, IMU_ADDR, out, len, false, 1000) <0)
    {
        printf("imu_read fail: data\n");
        return 1;
    }

    return 0;
}

static inline int imu_write(uint8_t addr_l, uint8_t value)
{
    uint8_t buf[] = {addr_l, value};

    if (i2c_write_timeout_us(IMU_I2C, IMU_ADDR, buf, 2, false, 1000) <0)
    {
        printf("imu_write fail\n");
        return 1;
    }

    return 0;
}


void lis3dh_calc_value(uint16_t raw_value, float *final_value, bool isAccel) {
    // Convert with respect to the value being temperature or acceleration reading 
    float scaling;
    float senstivity = 0.004f; // g per unit

    if (isAccel == true) {
        scaling = 64 / senstivity;
    } else {
        scaling = 64;
    }

    // raw_value is signed
    *final_value = (float) ((int16_t) raw_value) / scaling;
}

void lis3dh_read_data(uint8_t reg, float *final_value, bool IsAccel) {
    // Read two bytes of data and store in a 16 bit data structure
    uint8_t lsb;
    uint8_t msb;
    uint16_t raw_accel;
    imu_read(reg, 1, &lsb);

    reg |= 0x01;
    imu_read(reg, 1, &msb);

    raw_accel = (msb << 8) | lsb;

    lis3dh_calc_value(raw_accel, final_value, IsAccel);
}

void lis3dh_read_raw_data(uint8_t reg, int16_t *final_value) {
    // Read two bytes of data and store in a 16 bit data structure
    uint8_t lsb;
    uint8_t msb;
    imu_read(reg, 1, &lsb);

    reg |= 0x01;
    imu_read(reg, 1, &msb);

    *final_value = (msb << 8) | lsb;
}


void init_imu()
{
    i2c_init(I2C_INST, 100*1000);
    gpio_set_function(PIN_I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_I2C_SDA);
    gpio_pull_up(PIN_I2C_SCL);

    imu_write(IMU_REG_CTRL_REG_1, 0b10010111);
    imu_write(IMU_REG_CTRL_REG_4, 0b10000000);
    imu_write(IMU_REG_TEMP_CFG_REG, 0b10000000); // 0b10000000 enable ADC; 0b11000000 enable ADC + wire ADC3 to temp
}

float imu_x, imu_y, imu_z;

void update_imu()
{
    // float imu_x, imu_y, imu_z;
    lis3dh_read_data(0x28, &imu_x, true);
    lis3dh_read_data(0x2A, &imu_y, true);
    lis3dh_read_data(0x2C, &imu_z, true);

    // int16_t adc1, adc2, adc3;
    // lis3dh_read_raw_data(0x08, &adc1);
    // lis3dh_read_raw_data(0x0a, &adc2);
    // lis3dh_read_raw_data(0x0c, &adc3);

    // #define ADJUST_ADC(x) -(x>>6)

    // int adc_r1 = ADJUST_ADC(adc1);
    // int adc_r2 = ADJUST_ADC(adc2);
    // int adc_r3 = ADJUST_ADC(adc3);

}