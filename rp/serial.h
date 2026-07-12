/**
 * @file      serial.h
 * @author    The OSLUV Project
 * @brief     Per-lamp unique serial number (from RP2040 hardware unique ID)
 *
 */

#ifndef _SERIAL_H_
#define _SERIAL_H_


/* Exported functions prototypes ---------------------------------------------*/

void        serial_init(void);              /* read hardware unique ID once at boot */
const char* serial_get_string(void);        /* canonical 13-char Base32, no dashes  */
const char* serial_get_display_string(void);/* grouped 4-4-5 with dashes, for the UI */


#endif /* _SERIAL_H_ */

/*** END OF FILE ***/
