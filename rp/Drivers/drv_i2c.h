/**
 * @file      drv_i2c.h
 * @author    The OSLUV Project
 * @brief     Functions prototypes for shared I2C peripheral
 *  
 */

#ifndef _D_I2C_H_
#define _D_I2C_H_


/* Exported includes ---------------------------------------------------------*/

#include <stdint.h>
#include <hardware/i2c.h>

/* Exported defines ----------------------------------------------------------*/

#define D_I2C_PORT_C        i2c1


/* Exported functions prototypes ---------------------------------------------*/

void drv_i2c_init(void);

static inline int drv_i2c_rd_tmout_us(uint8_t addr, uint8_t *dst, size_t len, bool nostop, uint timeout_us)
{
    return i2c_read_timeout_us(D_I2C_PORT_C, addr, dst, len, nostop, timeout_us);
}

static inline int drv_i2c_wr_tmout_us(uint8_t addr, const uint8_t *src, size_t len, bool nostop, uint timeout_us)
{
    return i2c_write_timeout_us(D_I2C_PORT_C, addr, src, len, nostop, timeout_us);
}



#endif /* _D_I2C_H_ */

/*** END OF FILE ***/