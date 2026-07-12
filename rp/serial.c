/**
 * @file      serial.c
 * @author    The OSLUV Project
 * @brief     Per-lamp unique serial number
 *
 * @note The serial is the RP2040's 64-bit hardware unique ID, read from the
 *       QSPI flash chip. It is globally unique per physical board and needs no
 *       provisioning or persistence. Rendered as a 16-char uppercase hex string.
 */


/* Includes ------------------------------------------------------------------*/

#include "pico/unique_id.h"
#include "serial.h"


/* Private variables  --------------------------------------------------------*/

static char g_serial_str[2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1];  /* 17 bytes */


/* Exported functions --------------------------------------------------------*/

/**
 * @brief Reads the hardware unique ID once into the cached string
 *
 */
void serial_init(void)
{
    pico_get_unique_board_id_string(g_serial_str, sizeof(g_serial_str));
}

/**
 * @brief Gets the cached serial number string
 *
 * @return const char* 16-char uppercase hex, null-terminated
 */
const char* serial_get_string(void)
{
    return g_serial_str;
}

/*** END OF FILE ***/
