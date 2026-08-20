#ifndef TEST_PICO_TIME_H
#define TEST_PICO_TIME_H

#include <stdbool.h>
#include <stdint.h>

typedef uint64_t absolute_time_t;

absolute_time_t get_absolute_time(void);
uint64_t time_us_64(void);

static inline absolute_time_t make_timeout_time_ms(uint32_t ms)
{
    return get_absolute_time() + ((uint64_t)ms * 1000U);
}

static inline bool time_reached(absolute_time_t deadline)
{
    return get_absolute_time() >= deadline;
}

#endif
