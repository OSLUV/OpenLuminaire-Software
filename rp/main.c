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
#include <pico/stdlib.h>
#ifndef DEBUG_BUILD
#include <pico/stdio_uart.h>
#endif
#include "Modules/mod_comm_mgr.h"
#include "Modules/mod_ctrl_mgr.h"
#include "Modules/mod_pow_mgr.h"
#include "Modules/mod_system.h"
#include "Modules/mod_ui_mgr.h"


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
		
#ifndef DEBUG_BUILD
		mod_comm_manager();
#endif
		
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
#ifdef DEBUG_BUILD
	//stdio_uart_init_full(uart1, 115200, 8, 9);
#endif

	stdio_init_all();

	sleep_ms(3 * 1000); // Needed to avoid "detecting" v1.2 if power cycle to fast

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
#ifndef DEBUG_BUILD
	mod_comm_init();
#endif
	mod_ctrl_init();
}


/*** END OF FILE ***/
