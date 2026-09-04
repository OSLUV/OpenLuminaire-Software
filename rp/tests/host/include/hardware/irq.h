#ifndef TEST_HARDWARE_IRQ_H
#define TEST_HARDWARE_IRQ_H

#include <stdbool.h>

#include "pico/stdlib.h"

#define UART1_IRQ 1U

typedef void (*irq_handler_t)(void);

void irq_set_exclusive_handler(uint irq_num, irq_handler_t handler);
void irq_set_enabled(uint irq_num, bool enabled);

#endif
