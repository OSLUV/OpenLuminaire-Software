#ifndef TEST_PICO_STDLIB_H
#define TEST_PICO_STDLIB_H

#include <stdbool.h>
#include <stdint.h>

#include "pico/stdio.h"
#include "pico/time.h"

typedef unsigned int uint;

#define __isr
#define __packed __attribute__((packed))
#define GPIO_FUNC_UART 2U

void gpio_set_function(uint pin, uint function);

#endif
