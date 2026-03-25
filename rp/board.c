/**
 * @file      board.c
 * @author    The OSLUV Project
 * @brief     Board version detection and configuration
 *
 * Detection algorithm:
 *   With both rail enables off, read the 24V sense pin.
 *   V1.2 boost input is VSYS — VSYS leaks through to the 24V sense (~4.6V on 5V USB).
 *   V1.1 boost input is the 12V rail — reads ~0V with the rail off.
 *   Threshold: 24V sense > 3.0V → V1.2.  Passive, no inrush, safe on any supply.
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

#include "board.h"
#include "sense.h"


/* Private define ------------------------------------------------------------*/

#define BOARD_24V_PASSIVE_THRESHOLD_C	3.0		/* V — VSYS leaks through on V1.2, reads ~0V on V1.1 */


/* Private variables  --------------------------------------------------------*/

static bool b_board_is_v1_2 = false;	/* Fail-safe default: V1.1 */


/* Exported functions --------------------------------------------------------*/

/**
 * @brief Detects board version by passive ADC read.
 *
 * With both rail enables off, VSYS leaks through to the 24V sense on V1.2
 * (~4.6V on 5V USB) but reads ~0V on V1.1. No boost pulse needed.
 *
 * Must be called after sense_init(), but before usbpd_negotiate().
 */
void board_init(void)
{
	sense_update();

	printf("Board detection: 24V sense = %.2fV (threshold = %.1fV)\n",
		   g_sense_24v, (float)BOARD_24V_PASSIVE_THRESHOLD_C);

	if (g_sense_24v > BOARD_24V_PASSIVE_THRESHOLD_C)
	{
		b_board_is_v1_2 = true;
		printf("Board detected: V1.2 (VSYS on 24V sense)\n");
	}
	else
	{
		b_board_is_v1_2 = false;
		printf("Board detected: V1.1 (no voltage on 24V sense)\n");
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
