/**
 * @file      drv_i2c.c
 * @author    The OSLUV Project
 * @brief     Driver for shared I2C peripheral
 * @schematic lamp_controller.SchDoc
 *  
 */


/* Includes ------------------------------------------------------------------*/

#include <pico/stdlib.h>
#include "Drivers/drv_i2c.h"


/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/


#define I2C_INST 

#define D_I2C_PIN_SDA_C     2
#define D_I2C_PIN_SCL_C     3
#define D_I2C_SPEED_HZ_C    (100 * 1000)



/* Global variables  ---------------------------------------------------------*/
/* Private variables  --------------------------------------------------------*/

uint8_t drv_i2c_is_initialized_b = false;

/* Callback prototypes -------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Exported functions --------------------------------------------------------*/

void drv_i2c_init(void)
{
    if (!drv_i2c_is_initialized_b)
    {
        i2c_init(D_I2C_PORT_C, D_I2C_SPEED_HZ_C);
        gpio_set_function(D_I2C_PIN_SDA_C, GPIO_FUNC_I2C);
        gpio_set_function(D_I2C_PIN_SCL_C, GPIO_FUNC_I2C);
        gpio_pull_up(D_I2C_PIN_SDA_C);
        gpio_pull_up(D_I2C_PIN_SCL_C);

        drv_i2c_is_initialized_b = true;
    }
}


/* Callback functions --------------------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/*** END OF FILE ***/