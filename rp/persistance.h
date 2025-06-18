#include <stdint.h>
#include "lamp.h"

extern struct __packed persistance_region {
	uint32_t magic;

	uint8_t factory_lamp_type;
	uint8_t pad[3];
} persistance_region;

void init_persistance_region();
void write_persistance_region();
