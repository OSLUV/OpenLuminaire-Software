/**
 * @file      board.c
 * @author    The OSLUV Project
 * @brief     Board version detection and configuration
 *
 * Detection algorithm (from hardware designer):
 *   1. Keep 12V and 24V enables off at startup
 *   2. Enable 24V boost
 *   3. If +24V_SENSE reads ~24V → V1.2 (boost input is VSYS, works without 12V)
 *      Else → V1.1 (boost input is 12V rail, can't produce 24V without 12V)
 *
 * V1.1: External 12V → 24V boost from 12V.  USB-PD negotiates 12V/2.5A.
 * V1.2: VSYS → 24V boost from VSYS → 12V buck from 24V.  USB-PD negotiates 20V/1.5A.
 *
 * Fail-safe: defaults to V1.1. 12V negotiation is safe on both boards.
 * 20V negotiation on V1.1 would put 20V on the 12V rail — dangerous.
 */


/* Includes ------------------------------------------------------------------*/

#include <stdio.h>
#include <pico/stdlib.h>
#include <hardware/gpio.h>

#include "board.h"
#include "pins.h"
#include "sense.h"


/* Private define ------------------------------------------------------------*/

#define BOARD_24V_DETECT_THRESHOLD_C	20.0	/* V — if 24V sense reads above this, boost is working */
#define BOARD_DETECT_SETTLE_MS_C		200		/* ms — time for 24V boost to stabilize */


/* Private variables  --------------------------------------------------------*/

static bool b_board_is_v1_2 = false;	/* Fail-safe default: V1.1 */


/* Exported functions --------------------------------------------------------*/

/**
 * @brief Detects board version by probing the 24V boost.
 *
 * Must be called after lamp_init() (GPIO setup) and sense_init() (ADC setup),
 * but before usbpd_negotiate() and rail enable sequence.
 */
void board_init(void)
{
	printf("Board detection: enabling 24V boost...\n");

	/* Both enables should already be off from lamp_init().
	 * Temporarily pulse 24V enable and read the ADC. */
	gpio_put(PIN_ENABLE_24V, true);
	sleep_ms(BOARD_DETECT_SETTLE_MS_C);
	sense_update();
	gpio_put(PIN_ENABLE_24V, false);

	printf("Board detection: 24V sense = %.2fV (threshold = %.1fV)\n",
		   g_sense_24v, (float)BOARD_24V_DETECT_THRESHOLD_C);

	if (g_sense_24v > BOARD_24V_DETECT_THRESHOLD_C)
	{
		b_board_is_v1_2 = true;
		printf("Board detected: V1.2 (24V boost from VSYS)\n");
	}
	else
	{
		b_board_is_v1_2 = false;
		printf("Board detected: V1.1 (24V boost requires 12V)\n");
	}
}

/**
 * @brief Returns whether the detected board is V1.2
 *
 * @return true   V1.2 controller PCB
 * @return false  V1.1 controller PCB (or detection failed — fail-safe)
 */
bool board_is_v1_2(void)
{
	return b_board_is_v1_2;
}


/* Private functions ---------------------------------------------------------*/

/*** END OF FILE ***/
