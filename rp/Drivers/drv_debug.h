/**
 * @file      drv_debug.h
 * @author    The OSLUV Project
 * @brief     Functions prototypes for debugging strings output driver
 *  
 */

#ifndef _D_DEBUG_H_
#define _D_DEBUG_H_

/* Exported includes ---------------------------------------------------------*/
/* Exported defines ----------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/

void debug_init(void);
void debug_print_str(char *p_str_sc);
void debug_print_f(char *p_aFmt_sc, ...);
void debug_print_mod_f(char *p_aId_str, char *p_aFmt_sc, ...);
void debug_print_err(char *p_id_str, char *p_format, ...);
void debug_print_warn(char *p_id_str, char *p_format, ...);
void debug_print_ok(char *p_id_str, char *p_format, ...);
void debug_reset_color(void);


#endif /*_D_DEBUG_H_ */

/*** END OF FILE ***/
