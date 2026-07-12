/**
 * @file      serial.h
 * @author    The OSLUV Project
 * @brief     Per-lamp unique serial number (from RP2040 hardware unique ID)
 *
 */

#ifndef _SERIAL_H_
#define _SERIAL_H_


/* Exported functions prototypes ---------------------------------------------*/

void        serial_init(void);          /* read hardware unique ID once at boot */
const char* serial_get_string(void);    /* 16-char hex string, null-terminated */


#endif /* _SERIAL_H_ */

/*** END OF FILE ***/
