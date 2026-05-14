/**
 * @file      mod_ctrl_mgr.h
 * @author    The OSLUV Project
 * @brief     Functions prototypes for Control Manager module
 *  
 */

#ifndef _M_CTRL_H_
#define _M_CTRL_H_


/* Exported includes ---------------------------------------------------------*/

#include <stdint.h>


/* Exported variables --------------------------------------------------------*/
/* Exported functions prototypes ---------------------------------------------*/

void mod_ctrl_init(void);
void mod_ctrl_manager(void);

int16_t mod_ctrl_set_lamp_stt(uint16_t req_state);
int16_t mod_ctrl_get_lamp_stt(uint16_t state);
int16_t mod_ctrl_set_lamp_dim(uint16_t level);
int16_t mod_ctrl_get_lamp_dim(uint16_t level);


#endif /* _M_CTRL_H_ */

/*** END OF FILE ***/