#ifndef TEST_PICO_STDIO_H
#define TEST_PICO_STDIO_H

#include <stdbool.h>
#include <stdint.h>

int getchar_timeout_us(uint32_t timeout_us);
int putchar_raw(int character);
int stdio_put_string(const char *data, int len, bool newline, bool cr_translation);
void stdio_flush(void);

#endif
