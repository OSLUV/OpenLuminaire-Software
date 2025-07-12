#include <stdint.h>
#include "lamp.h"

// extern struct __packed persistance_region {
	// uint32_t magic;

	// uint8_t factory_lamp_type;
	// uint8_t pad[3];
// } persistance_region;



/* persistent data stored in the very last 4 kB sector */
extern struct __packed persistance_region {
    uint32_t magic;          /* guard */
    uint8_t  power_on;       /* 1 = lamp on */
    uint8_t  radar_on;       /* 1 = radar enabled */
    uint8_t  dim_index;      /* 0–3  (20/40/70/100 %) */
	uint8_t factory_lamp_type;
    // uint8_t  _pad;           /* keep struct 4-byte aligned */
} persistance_region;

void init_persistance_region();
void write_persistance_region();

// void  persist_init(void);                    /* call once at boot  */
void  persist_set_power(bool on);
void  persist_set_radar(bool on);
void  persist_set_dim_idx(uint8_t idx);     /* 0–3 */
bool  persist_get_power	();
bool  persist_get_radar();
uint8_t persist_get_dim_idx();
