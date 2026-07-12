/**
 * @file      serial.c
 * @author    The OSLUV Project
 * @brief     Per-lamp unique serial number
 *
 * @note The serial is the RP2040's 64-bit hardware unique ID, read from the
 *       QSPI flash chip. It is globally unique per physical board and needs no
 *       provisioning or persistence.
 *
 *       It is rendered in Crockford Base32 (alphabet excludes I, L, O, U to
 *       avoid visual ambiguity with 1/0). 64 bits encode to 13 chars, which is
 *       bijective with the hardware ID -> zero collision risk. Two cached forms:
 *       a canonical dash-free string for UART/logging, and a grouped 4-4-5 form
 *       for the debug screen.
 */


/* Includes ------------------------------------------------------------------*/

#include <stdint.h>
#include "pico/unique_id.h"
#include "serial.h"


/* Private define ------------------------------------------------------------*/

#define SERIAL_N_CHARS_C    13                                                  /* ceil(64 / 5) */


/* Private variables  --------------------------------------------------------*/

/* Crockford Base32 alphabet (no I, L, O, U). */
static const char CROCKFORD_C[32] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

static char g_serial_str[SERIAL_N_CHARS_C + 1];         /* "TRG58ZQ4P1WKC"      */
static char g_serial_display[SERIAL_N_CHARS_C + 2 + 1]; /* "TRG5-8ZQ4-P1WKC"    */


/* Exported functions --------------------------------------------------------*/

/**
 * @brief Reads the hardware unique ID once and builds the cached serial strings
 *
 */
void serial_init(void)
{
    pico_unique_board_id_t board_id;
    int                    i, w;
    uint64_t               id;

    pico_get_unique_board_id(&board_id);

    /* Assemble the 8 ID bytes MSB-first into a 64-bit value. */
    id = 0;
    for (i = 0; i < PICO_UNIQUE_BOARD_ID_SIZE_BYTES; i++)
    {
        id = (id << 8) | board_id.id[i];
    }

    /* Right-aligned Base32: emit 13 groups of 5 bits, most-significant first.
     * (13*5 = 65 bits; the top char carries only the 4 real high bits.) */
    for (i = SERIAL_N_CHARS_C - 1; i >= 0; i--)
    {
        g_serial_str[i] = CROCKFORD_C[id & 0x1F];
        id >>= 5;
    }
    g_serial_str[SERIAL_N_CHARS_C] = '\0';

    /* Grouped display form: XXXX-XXXX-XXXXX (dashes after chars 4 and 8). */
    w = 0;
    for (i = 0; i < SERIAL_N_CHARS_C; i++)
    {
        if (i == 4 || i == 8)
        {
            g_serial_display[w++] = '-';
        }
        g_serial_display[w++] = g_serial_str[i];
    }
    g_serial_display[w] = '\0';
}

/**
 * @brief Gets the canonical serial string (13 Base32 chars, no dashes)
 *
 * @return const char* null-terminated
 */
const char* serial_get_string(void)
{
    return g_serial_str;
}

/**
 * @brief Gets the grouped serial string for display (4-4-5 with dashes)
 *
 * @return const char* null-terminated
 */
const char* serial_get_display_string(void)
{
    return g_serial_display;
}

/*** END OF FILE ***/
