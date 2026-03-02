/**
 * @file      persistance.c
 * @author    The OSLUV Project
 * @brief     Driver for 
 * @schematic lamp_controller.SchDoc
 *  
 */


/* Includes ------------------------------------------------------------------*/

#include <hardware/flash.h>
#include <pico/flash.h>
#include <string.h>
#include <stdio.h>
#include "persistance.h"
#include "ui_main.h"


/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/

#define PERSISTANCE_MAGIC_VAL_C 	0xb8870200
#define PERSISTANCE_FLASH_OFFSET_C 	(PICO_FLASH_SIZE_BYTES - 4096) 				/* Stored in the very last 4 kB sector */

#define PERSISTANCE_DEF_POWER_ON_C	1											/* Lamp on   */
#define PERSISTANCE_DEF_RADAR_ON_C  0											/* Radar off */
#define PERSISTANCE_DEF_DIM_IDX_C	3											/* 0–3  (20/40/70/100 %) */


/* Global variables  ---------------------------------------------------------*/

PERSISTANCE_REGION_T g_persistance_region = {0};


/* Private variables  --------------------------------------------------------*/

static bool 				b_persistance_is_dirty = false;
static const uint8_t*		p_persistance_flash_region = 
							(const uint8_t *)(XIP_BASE + PERSISTANCE_FLASH_OFFSET_C);


/* Private function prototypes -----------------------------------------------*/

static void write_persistance_region_inner(void*);


/* Exported functions --------------------------------------------------------*/

/**
 * @brief Gets persistence data from assigned memory region
 * 
 * @return 	void  
 * 
 */
void persistance_read_region(void)
{
	memcpy(&g_persistance_region, p_persistance_flash_region, sizeof(g_persistance_region));

	if (g_persistance_region.magic != PERSISTANCE_MAGIC_VAL_C)
	{
		memset(&g_persistance_region, 0, sizeof(g_persistance_region));

		g_persistance_region.magic 	  = PERSISTANCE_MAGIC_VAL_C;
		g_persistance_region.power_on   = PERSISTANCE_DEF_POWER_ON_C;
        g_persistance_region.radar_on   = PERSISTANCE_DEF_RADAR_ON_C;
        g_persistance_region.dim_index  = PERSISTANCE_DEF_DIM_IDX_C;

		b_persistance_is_dirty = true;
	}
}

/**
 * @brief Stores the persistence data at assigned memory region
 * 
 */
void persistance_write_region(void)
{
	if (!b_persistance_is_dirty) 
	{
		return;
	}

	flash_safe_execute(write_persistance_region_inner, NULL, 100);

	b_persistance_is_dirty = false;

	printf("writing to persistance region");
}

/**
 * @brief Sets a new persistence lamp power state
 * 
 * @param b_pwr_on The new state to set
 */
void persistance_set_power_state(bool b_pwr_on)
{
	b_persistance_is_dirty |= (g_persistance_region.power_on != b_pwr_on);

    g_persistance_region.power_on = b_pwr_on; 
}

/**
 * @brief Gets the current persistence lamp power state
 * 
 * @return true 
 * @return false 
 */
bool persistance_get_power_state(void)
{
	return g_persistance_region.power_on;
}

/**
 * @brief Sets a new persistence radar state
 * 
 * @param b_radar_on The new state to set
 */
void persistance_set_radar_state(bool b_radar_on)
{
	b_persistance_is_dirty |= (g_persistance_region.radar_on != b_radar_on);

	g_persistance_region.radar_on = b_radar_on;
}

/**
 * @brief Gets the current persistence radar state
 * 
 * @return true 
 * @return false 
 */
bool persistance_get_radar_state(void)
{
	return g_persistance_region.radar_on;
}

/**
 * @brief Sets a new persistence dim level by index 
 * @ref UI_MAIN_MAX_DIM_LEVELS_C
 * 
 * @param idx 
 */
void persistance_set_dim_index(uint8_t idx)
{
	if (idx > UI_MAIN_MAX_DIM_INDEX_C) 
	{
		idx = UI_MAIN_MAX_DIM_INDEX_C;
	}

	b_persistance_is_dirty |= (g_persistance_region.dim_index != idx);

	g_persistance_region.dim_index = idx;
}

/**
 * @brief Gets the current persistence dim level by index 
 * @ref UI_MAIN_MAX_DIM_LEVELS_C
 * 
 * @return uint8_t 
 */
uint8_t persistance_get_dim_index(void)
{
	return g_persistance_region.dim_index;
}


/* Private functions ---------------------------------------------------------*/

/**
 * @brief Writes the persistence data at assigned memory region
 * 
 */
static void write_persistance_region_inner(void*)
{
	flash_range_erase(PERSISTANCE_FLASH_OFFSET_C, FLASH_SECTOR_SIZE);

	flash_range_program(PERSISTANCE_FLASH_OFFSET_C, 
						(const uint8_t*)&g_persistance_region,
						sizeof(g_persistance_region));
}

/*** END OF FILE ***/
