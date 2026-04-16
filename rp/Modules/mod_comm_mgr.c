/**
 * @file      mod_comm_mgr.c
 * @author    The OSLUV Project
 * @brief     Communications Manager module. This module handles all system 
 *            communications.
 */


/* Includes ------------------------------------------------------------------*/

#include "Modules/mod_comm_mgr.h"
#include "Modules/mod_ser_cmd.h"
#include "Modules/system.h"


/* Private define ------------------------------------------------------------*/
/* Private typedef -----------------------------------------------------------*/
/* Global variables  ---------------------------------------------------------*/
/* Private variables  --------------------------------------------------------*/
/* Callback prototypes -------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/

/* Exported functions --------------------------------------------------------*/

/**
 * @brief Communications Manager module initialization procedure
 * 
 */
void mod_comm_init(void)
{
    mod_cmd_init();
}

/**
 * @brief Communications Manager module tasks
 * 
 */
void mod_comm_manager(void)
{
    mod_cmd_handler();
}

/* Callback functions --------------------------------------------------------*/
/* Private functions ---------------------------------------------------------*/


/*** END OF FILE ***/