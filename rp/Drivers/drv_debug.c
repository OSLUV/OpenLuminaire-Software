/**
 * @file      drv_debug.c
 * @author    The OSLUV Project
 * @brief     Driver for debugging strings output
 *  
 */


/* Includes ------------------------------------------------------------------*/

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <pico/stdlib.h>
#ifndef DEBUG_BUILD
#include <pico/stdio_uart.h>
#endif
#include "drv_debug.h"


/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/

#define D_DEBUG_STR_MAX_LEN_C     256

#define D_DEBUG_LINE_BREAK_TXT_C  "\r\n"

#define D_DEBUG_RESET_COLOR       printf("\033[0m")
#define D_DEBUG_SET_RED_COLOR     printf("\033[0;31m")
#define D_DEBUG_SET_GRN_COLOR     printf("\033[0;32m")
#define D_DEBUG_SET_YEL_COLOR     printf("\033[0;33m")
#define D_DEBUG_SET_CYAN_COLOR    printf("\033[0;36m")


/* Global variables  ---------------------------------------------------------*/
/* Private variables  --------------------------------------------------------*/

static char   debug_string[D_DEBUG_STR_MAX_LEN_C];


/* Callback prototypes -------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/

static inline void debug_print_format(char *p_id_str, char *p_format, va_list args);


/* Exported functions --------------------------------------------------------*/

/**
 * @brief Debug driver initialization procedure
 * 
 */
void debug_init(void)
{
#ifdef DEBUG_BUILD
	stdio_uart_init_full(uart1, 115200, 8, 9);
#endif
}

/**
 * @brief Writes the string to debug port.
 * 
 * @param p_string  String to send
 */
void debug_print_str(char *p_string)
{
  printf(p_string);
}

/**
 * @brief Writes the string pointed by p_format to debug port.
 * 
 * @param p_format  String to send
 * @param ...       Parameters
 */
void debug_print_f(char *p_format, ...)
{
  va_list args;

  memset(debug_string, 0, sizeof(debug_string));

  va_start(args, p_format);
  vsnprintf(debug_string,
            sizeof(debug_string) - 1,
            p_format,
            args);
  va_end(args);

  debug_print_str( debug_string );
}

/**
 * @brief Writes the string pointed by p_format to output port. 
 * 
 * @note This function will attach a timestamp and module's ID first prior to 
 * the string
 * 
 * @param p_id_str  Module/driver string ID
 * @param p_format  String format
 * @param ... 
 */
void debug_print_mod_f(char *p_id_str, char *p_format, ...)
{
#if 1
  va_list args;

  debug_print_str(D_DEBUG_LINE_BREAK_TXT_C);

  D_DEBUG_RESET_COLOR;

  va_start(args, p_format);
  debug_print_format(p_id_str, p_format, args);
  va_end(args);

  //D_DEBUG_RESET_COLOR;
#else
  va_list  args;
  uint64_t time;

  time = 0;//time_us_64() / 1000;

  D_DEBUG_RESET_COLOR;

  debug_print_str(D_DEBUG_LINE_BREAK_TXT_C);

  if (strlen(p_id_str))
  {
    if (strrchr(p_id_str, '/'))
    {
      p_id_str = strrchr(p_id_str, '/');
    }
    debug_print_f("%llu %s: ",
                  time,
                  p_id_str);
  }

  memset(debug_string, 0, sizeof(debug_string));

  va_start(args, p_format);
  vsnprintf(debug_string,
            sizeof(debug_string) - 1,
            p_format,
            args);
  va_end(args);

  debug_print_str(debug_string);
#endif
}

/**
 * @brief Writes the string pointed by p_format to a debug terminal in red.
 * 
 * @param p_id_str Module/driver string ID
 * @param p_format String format
 * @param ...      Parameters
 */
void debug_print_err(char *p_id_str, char *p_format, ...)
{
  va_list args;

  debug_print_str(D_DEBUG_LINE_BREAK_TXT_C);

  D_DEBUG_SET_RED_COLOR;

  va_start(args, p_format);
  debug_print_format(p_id_str, p_format, args);
  va_end(args);

  //D_DEBUG_RESET_COLOR;
}

/**
 * @brief Writes the string pointed by p_format to a debug terminal in yellow.
 * 
 * @param p_id_str Module/driver string ID
 * @param p_format String format
 * @param ...      Parameters
 */
void debug_print_warn(char *p_id_str, char *p_format, ...)
{
  va_list args;

  debug_print_str(D_DEBUG_LINE_BREAK_TXT_C);

  D_DEBUG_SET_YEL_COLOR;

  va_start(args, p_format);
  debug_print_format(p_id_str, p_format, args);
  va_end(args);

  //D_DEBUG_RESET_COLOR;
}

void debug_print_ok(char *p_id_str, char *p_format, ...)
{
  va_list args;

  debug_print_str(D_DEBUG_LINE_BREAK_TXT_C);

  D_DEBUG_SET_GRN_COLOR;

  va_start(args, p_format);
  debug_print_format(p_id_str, p_format, args);
  va_end(args);

  //D_DEBUG_RESET_COLOR;
}

/**
 * @brief Resets text color on COM terminal
 * 
 */
void debug_reset_color(void)
{
  D_DEBUG_RESET_COLOR;
}


/* Callback functions --------------------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/**
 * @brief Writes the string pointed by p_format to output port. 
 * 
 * @note This function will attach a timestamp and module's ID first prior to 
 * the string
 * 
 * @param p_id_str  Module/driver string ID
 * @param p_format  String format
 * @param args       
 */
static inline void debug_print_format(char *p_id_str, char *p_format, va_list args)
{
  uint64_t time;

  time = time_us_64() / 1000;

  //debug_print_str(D_DEBUG_LINE_BREAK_TXT_C);

  if (strlen(p_id_str))
  {
    if (strrchr(p_id_str, '/'))
    {
      p_id_str = strrchr(p_id_str, '/');
    }
    debug_print_f("%llu %s: ",
                  time,
                  p_id_str);
  }

  memset(debug_string, 0, sizeof(debug_string));

  vsnprintf(debug_string,
            sizeof(debug_string) - 1,
            p_format,
            args);

  debug_print_str(debug_string);
}


/*** END OF FILE ***/