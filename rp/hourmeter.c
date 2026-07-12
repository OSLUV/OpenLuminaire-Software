/**
 * @file      hourmeter.c
 * @author    The OSLUV Project
 * @brief     Lamp-on-time hour meter
 *
 * @note Accumulates UV-emitting (lamp-on) time into the persistence region,
 *       split by dim level, using monotonic time_us_64() deltas. Flash writes
 *       are throttled (there is no wear-leveling on the persistence sector):
 *       at most once per @ref HOURMETER_FLUSH_INTERVAL_S_C of on-time, plus once
 *       on each lamp-off transition to capture the tail before power-down.
 */


/* Includes ------------------------------------------------------------------*/

#include <pico/stdlib.h>
#include "hourmeter.h"
#include "lamp.h"
#include "persistance.h"


/* Private define ------------------------------------------------------------*/

#define HOURMETER_US_PER_SEC_C        1000000ULL
#define HOURMETER_SECS_PER_HOUR_C     3600U
#define HOURMETER_FLUSH_INTERVAL_S_C  600U      /* Flush at most once per 10 min of on-time */


/* Private variables  --------------------------------------------------------*/

static uint64_t hourmeter_last_us;              /* time_us_64() at previous update */
static uint64_t hourmeter_accum_us;             /* sub-second carry of on-time */
static uint32_t hourmeter_unflushed_secs;       /* on-seconds accumulated since last flash write */
static bool     hourmeter_was_on;               /* lamp emitting on the previous update */


/* Private function prototypes -----------------------------------------------*/

static uint8_t hourmeter_bucket_from_cmd(LAMP_PWR_LEVEL_E cmd);


/* Exported functions --------------------------------------------------------*/

/**
 * @brief Hour-meter initialization procedure
 *
 */
void hourmeter_init(void)
{
    hourmeter_last_us        = time_us_64();
    hourmeter_accum_us       = 0;
    hourmeter_unflushed_secs = 0;
    hourmeter_was_on         = false;
}

/**
 * @brief Accumulates lamp-on time; call once per main-loop iteration
 *
 */
void hourmeter_update(void)
{
    uint64_t         now   = time_us_64();
    uint64_t         delta = now - hourmeter_last_us;
    LAMP_PWR_LEVEL_E cmd   = lamp_get_commanded_power_level();
    bool             on    = (cmd != LAMP_PWR_OFF_C);

    hourmeter_last_us = now;

    if (on)
    {
        hourmeter_accum_us += delta;

        if (hourmeter_accum_us >= HOURMETER_US_PER_SEC_C)
        {
            uint32_t whole_secs = (uint32_t)(hourmeter_accum_us / HOURMETER_US_PER_SEC_C);
            hourmeter_accum_us  -= (uint64_t)whole_secs * HOURMETER_US_PER_SEC_C;

            persistance_add_lamp_on_seconds(whole_secs, hourmeter_bucket_from_cmd(cmd));
            hourmeter_unflushed_secs += whole_secs;

            if (hourmeter_unflushed_secs >= HOURMETER_FLUSH_INTERVAL_S_C)
            {
                persistance_write_region();     /* no-op if not dirty */
                hourmeter_unflushed_secs = 0;
            }
        }
    }
    else if (hourmeter_was_on)
    {
        /* Lamp just turned off — drop the sub-second remainder and flush the
         * tail so accumulated on-time is not lost on power-down.             */
        hourmeter_accum_us = 0;

        if (hourmeter_unflushed_secs > 0)
        {
            persistance_write_region();
            hourmeter_unflushed_secs = 0;
        }
    }

    hourmeter_was_on = on;
}

/**
 * @brief Gets the grand-total lamp-on time in seconds
 *
 * @return uint32_t
 */
uint32_t hourmeter_get_on_seconds(void)
{
    return persistance_get_lamp_on_seconds();
}

/**
 * @brief Gets the grand-total lamp-on time in whole hours
 *
 * @return uint32_t
 */
uint32_t hourmeter_get_on_hours(void)
{
    return persistance_get_lamp_on_seconds() / HOURMETER_SECS_PER_HOUR_C;
}

/**
 * @brief Gets the per-dim-level lamp-on time in seconds
 *
 * @param idx Dim level index (0–3 -> 20/40/70/100 %)
 * @return uint32_t
 */
uint32_t hourmeter_get_dim_on_seconds(uint8_t idx)
{
    return persistance_get_dim_on_seconds(idx);
}

/**
 * @brief Gets the per-dim-level lamp-on time in whole hours
 *
 * @param idx Dim level index (0–3 -> 20/40/70/100 %)
 * @return uint32_t
 */
uint32_t hourmeter_get_dim_on_hours(uint8_t idx)
{
    return persistance_get_dim_on_seconds(idx) / HOURMETER_SECS_PER_HOUR_C;
}


/* Private functions ---------------------------------------------------------*/

/**
 * @brief Maps a commanded power level to its dim-bucket index
 *
 * @param cmd Commanded power level
 * @return uint8_t Bucket index 0–3 (20/40/70/100 %). Any emitting level that is
 *         not a recognized dim step (e.g. transient warmup) is attributed to
 *         the 100 % bucket so total == sum(buckets) always holds.
 */
static uint8_t hourmeter_bucket_from_cmd(LAMP_PWR_LEVEL_E cmd)
{
    switch (cmd)
    {
        case LAMP_PWR_20PCT_C:  return 0;
        case LAMP_PWR_40PCT_C:  return 1;
        case LAMP_PWR_70PCT_C:  return 2;
        case LAMP_PWR_100PCT_C: return 3;
        default:                return 3;
    }
}

/*** END OF FILE ***/
