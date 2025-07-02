#include <hardware/flash.h>
#include <pico/flash.h>
#include <string.h>

#include "persistance.h"

#define MAGIC_VAL 0xb8870200
#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - 4096)

#define DEFAULT_POWER_ON      1      /* lamp on  */
#define DEFAULT_RADAR_ON      1      /* radar on */
#define DEFAULT_DIM_INDEX     3      /* 0-3 → 100 % */

struct persistance_region persistance_region = {0};
bool dirty = false;
const uint8_t *persistance_region_flash_ptr = 
		(const uint8_t *) (XIP_BASE + FLASH_TARGET_OFFSET);

void init_persistance_region()
{
	memcpy(&persistance_region, persistance_region_flash_ptr, sizeof(persistance_region));

	if (persistance_region.magic != MAGIC_VAL)
	{
		memset(&persistance_region, 0, sizeof(persistance_region));
		persistance_region.magic = MAGIC_VAL;
		persistance_region.power_on   = DEFAULT_POWER_ON;
        persistance_region.radar_on   = DEFAULT_RADAR_ON;
        persistance_region.dim_index  = DEFAULT_DIM_INDEX;
		dirty = true;
	}
}

void write_persistance_region_inner(void*)
{
	flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
	flash_range_program(FLASH_TARGET_OFFSET, 
				(const uint8_t*)&persistance_region, sizeof(persistance_region));
}

void write_persistance_region()
{
	if (!dirty) return;
	flash_safe_execute(write_persistance_region_inner, NULL, 100);
	dirty = false;
}

/* -------- setters & getters ------------------------------- */
void persist_set_power(bool on){ dirty |= (persistance_region.power_on != on);
                                           persistance_region.power_on = on; }
void persist_set_radar(bool on) { dirty |= (persistance_region.radar_on != on);
                                           persistance_region.radar_on = on; }
void persist_set_dim_idx(uint8_t idx) { dirty |= (persistance_region.dim_index != idx);
                                           persistance_region.dim_index = idx; }

bool    persist_get_power(void)          { return persistance_region.power_on; }
bool    persist_get_radar(void)          { return persistance_region.radar_on; }
uint8_t persist_get_dim_idx(void)        { return persistance_region.dim_index; }
