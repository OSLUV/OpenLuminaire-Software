/**
 * @file      main.c
 * @author    The OSLUV Project
 * @brief     Main application loop
 * @schematic lamp_controller.SchDoc
 * @schematic power.SchDoc
 * 
 * @mainpage Software for OSLUV OpenLuminaire
 * OpenLuminaire is OSHWA certified Open Source 222nm luminiare for the 
 * RP2040-based hardware.
 *  
 */


/* Includes ------------------------------------------------------------------*/

#include <stdio.h>
#include <string.h>
#include <pico/stdlib.h>
#include <lvgl.h>
#include "Modules/mod_comm_mgr.h"
#include "Modules/mod_ctrl_mgr.h"
#include "Modules/mod_pow_mgr.h"
#include "Modules/mod_system.h"
#include "Modules/mod_ui_mgr.h"

#include "font.c"

/* Private function prototypes -----------------------------------------------*/

static void main_sys_init(void);


/* Application main function -------------------------------------------------*/

void main(void)
{
	main_sys_init();
		
	while (1)
	{
		mod_sys_services();

		mod_pow_manager();
		
		mod_comm_manager();
		
		mod_ctrl_manager();

		mod_ui_manager();
	}
}

/**
 * @brief Full system initialization procedure
 * 
 */
static void main_sys_init(void)
{
	stdio_init_all();

	gpio_init(4); /* RADIO_RX */
	gpio_init(5); /* RADIO_TX */
	gpio_init(6); /* RADIO_ENABLE */
	gpio_set_dir(4, GPIO_IN);
	gpio_set_dir(5, GPIO_IN);
	gpio_set_dir(6, GPIO_IN);

	mod_sys_init();
	mod_ui_init();
	mod_pow_init();
	mod_sys_startup_wdt();
	mod_comm_init();
	mod_ctrl_init();
}


/*** END OF FILE ***/
