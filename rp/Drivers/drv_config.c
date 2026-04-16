/**
 * @file      drv_config.c
 * @author    The OSLUV Project
 * @brief     Driver for system configuration storage
 * @schematic lamp_controller.SchDoc
 *  
 */


/* Includes ------------------------------------------------------------------*/

#include <hardware/flash.h>
#include <pico/flash.h>
#include <string.h>
#include <stdio.h>
#include "Drivers/drv_config.h"
#include "Modules/mod_ui_scrn_main.h"


/* Private typedef -----------------------------------------------------------*/

typedef struct __packed {
    uint32_t magic;          /* guard */
    uint8_t  power_on;       /* 1 = lamp on */
    uint8_t  radar_on;       /* 1 = radar enabled */
    uint8_t  dim_index;      /* 0–3  (20/40/70/100 %) */
	uint8_t  factory_lamp_type;
} D_CFG_DATA_T;


/* Private define ------------------------------------------------------------*/

#define DRV_CFG_MAGIC_VAL_C 	0xb8870200
#define DRV_CFG_FLASH_OFFSET_C 	(PICO_FLASH_SIZE_BYTES - 4096) 					/* Stored in the very last 4 kB sector */

#define DRV_CFG_DEF_POWER_ON_C	1												/* Lamp on   */
#define DRV_CFG_DEF_RADAR_ON_C  0												/* Radar off */
#define DRV_CFG_DEF_DIM_IDX_C	3												/* 0–3  (20/40/70/100 %) */


/* Global variables  ---------------------------------------------------------*/

D_CFG_DATA_T g_drv_cfg = {0};


/* Private variables  --------------------------------------------------------*/

static bool 				b_drv_cfg_is_modified = false;
static const uint8_t*		p_drv_cfg_flash_region = 
							(const uint8_t *)(XIP_BASE + DRV_CFG_FLASH_OFFSET_C);


/* Private function prototypes -----------------------------------------------*/

static void drv_cfg_write_data(void*);


/* Exported functions --------------------------------------------------------*/

/**
 * @brief Driver initialization procedure
 * 
 * @return 	void  
 * 
 */
void drv_cfg_init(void)
{
	b_drv_cfg_is_modified = false;

	drv_cfg_read();
}

/**
 * @brief Gets system configuration data from assigned memory region
 * 
 * @return 	void  
 * 
 */
void drv_cfg_read(void)
{
	memcpy(&g_drv_cfg, p_drv_cfg_flash_region, sizeof(g_drv_cfg));

	if (g_drv_cfg.magic != DRV_CFG_MAGIC_VAL_C)
	{
		memset(&g_drv_cfg, 0, sizeof(g_drv_cfg));

		g_drv_cfg.magic 	 = DRV_CFG_MAGIC_VAL_C;
		g_drv_cfg.power_on   = DRV_CFG_DEF_POWER_ON_C;
        g_drv_cfg.radar_on   = DRV_CFG_DEF_RADAR_ON_C;
        g_drv_cfg.dim_index  = DRV_CFG_DEF_DIM_IDX_C;

		b_drv_cfg_is_modified = true;
	}
}

/**
 * @brief Saves system configuration data at assigned memory region
 * 
 */
void drv_cfg_save(void)
{
	if (b_drv_cfg_is_modified) 
	{
		printf("drv_cfg_save saving configuration");

		flash_safe_execute(drv_cfg_write_data, NULL, 100);

		b_drv_cfg_is_modified = false;
	}
}

/**
 * @brief Sets a new lamp power state
 * 
 * @param b_pwr_on The new state to set
 */
void drv_cfg_set_power_state(bool b_pwr_on)
{
	b_drv_cfg_is_modified |= (g_drv_cfg.power_on != b_pwr_on);

    g_drv_cfg.power_on = b_pwr_on; 
}

/**
 * @brief Gets the current lamp power state
 * 
 * @return true 
 * @return false 
 */
bool drv_cfg_get_power_state(void)
{
	return g_drv_cfg.power_on;
}

/**
 * @brief Sets a new radar state
 * 
 * @param b_radar_on The new state to set
 */
void drv_cfg_set_radar_state(bool b_radar_on)
{
	b_drv_cfg_is_modified |= (g_drv_cfg.radar_on != b_radar_on);

	g_drv_cfg.radar_on = b_radar_on;
}

/**
 * @brief Gets the current radar state
 * 
 * @return true 
 * @return false 
 */
bool drv_cfg_get_radar_state(void)
{
	return g_drv_cfg.radar_on;
}

/**
 * @brief Sets a new dim level by index 
 * @ref UI_MAIN_MAX_DIM_LEVELS_C
 * 
 * @param idx 
 */
void drv_cfg_set_dim_index(uint8_t idx)
{
	if (idx > UI_MAIN_MAX_DIM_INDEX_C) 
	{
		idx = UI_MAIN_MAX_DIM_INDEX_C;
	}

	b_drv_cfg_is_modified |= (g_drv_cfg.dim_index != idx);

	g_drv_cfg.dim_index = idx;
}

/**
 * @brief Gets the current dim level by index 
 * @ref UI_MAIN_MAX_DIM_LEVELS_C
 * 
 * @return uint8_t 
 */
uint8_t drv_cfg_get_dim_index(void)
{
	return g_drv_cfg.dim_index;
}

/**
 * @brief Sets the factory lamp type in persistence
 *
 * @param type @ref D_LAMP_TYPE_E
 */
void drv_cfg_set_factory_lamp_type(uint8_t type)
{
	b_drv_cfg_is_modified |= (g_drv_cfg.factory_lamp_type != type);
	
	g_drv_cfg.factory_lamp_type = type;
}

/**
 * @brief Gets the factory lamp type from persistence
 *
 * @return uint8_t @ref D_LAMP_TYPE_E
 */
uint8_t drv_cfg_get_factory_lamp_type(void)
{
	return g_drv_cfg.factory_lamp_type;
}


/* Private functions ---------------------------------------------------------*/

/**
 * @brief Writes the persistence data at assigned memory region
 * 
 */
static void drv_cfg_write_data(void*)
{
	flash_range_erase(DRV_CFG_FLASH_OFFSET_C, FLASH_SECTOR_SIZE);

	flash_range_program(DRV_CFG_FLASH_OFFSET_C, 
						(const uint8_t*)&g_drv_cfg,
						sizeof(g_drv_cfg));
}

/*** END OF FILE ***/
