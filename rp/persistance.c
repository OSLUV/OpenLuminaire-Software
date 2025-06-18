#include <hardware/flash.h>
#include <pico/flash.h>
#include <string.h>

#include "persistance.h"

#define MAGIC_VAL 0xb8870200
#define FLASH_TARGET_OFFSET (512 * 1024)

struct persistance_region persistance_region = {0};

const uint8_t *persistance_region_flash_ptr = (const uint8_t *) (XIP_BASE + FLASH_TARGET_OFFSET);

void init_persistance_region()
{
	memcpy(&persistance_region, persistance_region_flash_ptr, sizeof(persistance_region));

	if (persistance_region.magic != MAGIC_VAL)
	{
		memset(&persistance_region, 0, sizeof(persistance_region));
		persistance_region.magic = MAGIC_VAL;
	}
}

void write_persistance_region_inner(void*)
{
	flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
	flash_range_program(FLASH_TARGET_OFFSET, (const uint8_t*)&persistance_region, sizeof(persistance_region));
}

void write_persistance_region()
{
	flash_safe_execute(write_persistance_region_inner, NULL, 100);
}
