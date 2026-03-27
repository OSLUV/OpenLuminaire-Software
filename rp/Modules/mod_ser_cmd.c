/**
 * @file      mod_ser_cmd.c
 * @author    The OSLUV Project
 * @brief     Module for handling external serial commands
 *  
 */


/* Includes ------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/time.h"
#include "Modules/mod_ser_cmd.h"
#include "Drivers/drv_uart_cmd.h"
#include "lamp.h"
#include "ui_main.h"


/* Private define ------------------------------------------------------------*/

#define M_CMD_MAX_INST_LEN_C      16
#define M_CMD_MAX_PARAM_LEN_C     16
#define M_CMD_MAX_VAL_LEN_C       16
#define M_CMD_MAX_LEN_C           64

#if M_CMD_MAX_LEN_C < (M_CMD_MAX_INST_LEN_C + M_CMD_MAX_PARAM_LEN_C + M_CMD_MAX_VAL_LEN_C + 8)
#warning "M_CMD_MAX_LEN_C is not enough."
#endif

#define M_CMD_SEPARATOR_CHAR_C    ':'
#define M_CMD_CR_CHAR_C           '\r'
#define M_CMD_LF_CHAR_C           '\n'
#define M_CMD_INST_SET_S          "S"
#define M_CMD_INST_GET_S          "G"
#define M_CMD_PARAM_LAMP_CTL_ID_S "L"
#define M_CMD_PARAM_LAMP_DIM_ID_S "D"

#define M_CMD_OK_S                "OK"
#define M_CMD_ERR_S               "ERR"
#define M_CMD_TMOUT_S             "TOUT"

#define M_CMD_TMOUT_MS_C          50                                            /* Timeout in ms to wait for more data to arrive */


/* Private typedef -----------------------------------------------------------*/

typedef struct {

    uint8_t inst[M_CMD_MAX_INST_LEN_C];
    uint8_t param[M_CMD_MAX_PARAM_LEN_C];
    int16_t (*p_callback)(uint16_t);

} M_CMD_CTL_T;


/* Global variables  ---------------------------------------------------------*/
/* Private variables  --------------------------------------------------------*/

static const M_CMD_CTL_T  mod_cmd_list[] = 
{
    {M_CMD_INST_SET_S, M_CMD_PARAM_LAMP_CTL_ID_S, ui_main_lamp_set_stt },
    {M_CMD_INST_GET_S, M_CMD_PARAM_LAMP_CTL_ID_S, ui_main_lamp_get_stt },
    {M_CMD_INST_SET_S, M_CMD_PARAM_LAMP_DIM_ID_S, ui_main_lamp_set_dim },
    {M_CMD_INST_GET_S, M_CMD_PARAM_LAMP_DIM_ID_S, ui_main_lamp_get_dim },
    {0,              0,                       0                 }
};

static uint8_t          mod_cmd_buf[M_CMD_MAX_LEN_C];
static uint16_t         mod_cmd_idx;

static absolute_time_t  mod_cmd_tmout;


/* Callback prototypes -------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/

static void mod_cmd_process(void);


/* Exported functions --------------------------------------------------------*/

/**
 * @brief External commands module initialization procedure
 * 
 */
void mod_cmd_init(void)
{
    mod_cmd_idx = 0;

    drv_uart_cmd_init();
}

/**
 * @brief Handles external commands over dedicated serial port
 * 
 */
void mod_cmd_handler(void)
{
    uint16_t data_len;
    uint8_t string[M_CMD_MAX_LEN_C];

    data_len = drv_uart_cmd_get_rcvd_data_len();
    if (data_len) 
    {
        if ((mod_cmd_idx + data_len) >= M_CMD_MAX_LEN_C)
        {
            sprintf(string, 
                    "\r\n:%s\r\n",
                    M_CMD_ERR_S);

            drv_uart_cmd_send_data(string, strlen(string));
        }
        else 
        {
            data_len = drv_uart_cmd_get_data(mod_cmd_buf + mod_cmd_idx, data_len);
            if (data_len)
            {
                mod_cmd_idx += data_len;
                if (strchr(mod_cmd_buf, M_CMD_CR_CHAR_C) || 
                    strchr(mod_cmd_buf, M_CMD_LF_CHAR_C))                       /* End of command has been received ? */
                {
                    mod_cmd_process();

                    memset(mod_cmd_buf, 0, sizeof(mod_cmd_buf));

                    mod_cmd_idx = 0;

                    mod_cmd_tmout = 0;
                }
                else
                {
                    mod_cmd_tmout =  make_timeout_time_ms (M_CMD_TMOUT_MS_C);
                }
            }
        }
    }
    else if ((mod_cmd_tmout != 0) &&                                            /* Is timeout set?  */
             (get_absolute_time() > mod_cmd_tmout))                             /* Is timeout over? */
    {
        mod_cmd_tmout = 0;

        memset(mod_cmd_buf, 0, sizeof(mod_cmd_buf));

        mod_cmd_idx = 0;

        sprintf(string, 
                "\r\n:%s\r\n",
                M_CMD_TMOUT_S);

        drv_uart_cmd_send_data(string, strlen(string));
    }
}


/* Callback functions --------------------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/**
 * @brief Processes the received command and executes defined routine according 
 * to commands list @ref mod_cmd_list.
 * 
 * @note Initial commands format is I:P:V where I is for Instruction (i.e. SET 
 * or GET), P is for Parameter (i.e. Lamp state, lamp dim setting) and V is for
 * Value needed to set to the required parameter.
 * 
 * @note If the received command is not found in the commands list @ref 
 * mod_cmd_list error will be notified back to sender. Or if the value is not 
 * validated by the corresponding callback an error will be notified back to 
 * sender.
 */
static void mod_cmd_process(void)
{
    uint8_t idx;
    uint8_t *p_arg_sta, *p_arg_end;
    uint8_t string[M_CMD_MAX_LEN_C];
    uint8_t args_found, args_valid;
    uint8_t inst_str[M_CMD_MAX_INST_LEN_C];
    uint8_t param_str[M_CMD_MAX_PARAM_LEN_C];
    uint8_t value_str[M_CMD_MAX_VAL_LEN_C];
    uint16_t value;

    memset(inst_str, 0, sizeof(inst_str));
    memset(param_str, 0, sizeof(param_str));
    memset(value_str, 0, sizeof(value_str));
    args_found = 0;
    
    /* Get all command parameters */
    idx = 0;
    while ((mod_cmd_list[idx].inst[0] != 0) && (args_found == 0))
    {
        memset(string, 0, sizeof(string));
        strcpy(string, mod_cmd_list[idx].inst);
        string[strlen(mod_cmd_list[idx].inst)] = M_CMD_SEPARATOR_CHAR_C;

        p_arg_sta = strstr(mod_cmd_buf, string);
        if (p_arg_sta != 0)
        {
            strcpy(inst_str, mod_cmd_list[idx].inst);

            p_arg_end = strchr(p_arg_sta, M_CMD_SEPARATOR_CHAR_C);
            if (p_arg_end != 0)
            {
                p_arg_sta = p_arg_end + 1;
                p_arg_end = strchr(p_arg_sta, M_CMD_SEPARATOR_CHAR_C);
                if (p_arg_end != 0)
                {
                    strncpy(param_str, p_arg_sta, (p_arg_end - p_arg_sta));

                    strcpy(value_str, p_arg_end + 1);

                    if ((value_str[0] != M_CMD_CR_CHAR_C) && 
                        (value_str[0] != M_CMD_LF_CHAR_C))                      /* value argument is not empty? */
                    {
                        p_arg_end = strchr(value_str, M_CMD_CR_CHAR_C);
                        if (p_arg_end)
                        {
                            *p_arg_end = 0;
                        }
                        else
                        {
                            p_arg_end = strchr(value_str, M_CMD_LF_CHAR_C);
                            if (p_arg_end)
                            {
                                *p_arg_end = 0;
                            }
                        }

                        value = atoi(value_str);

                        args_found = 1;
                    }
                    else
                    {
                        value_str[0] = 0;                                       /* Since argument is empty, make sure is null */
                    }
                }
                else 
                {
                    p_arg_end = strchr(p_arg_sta, M_CMD_CR_CHAR_C);
                    if (p_arg_end != 0)                                         /* End of command ? */
                    {
                        strncpy(param_str, p_arg_sta, (p_arg_end - p_arg_sta));

                        args_found = 1;
                    }
                    else 
                    {
                        p_arg_end = strchr(p_arg_sta, M_CMD_LF_CHAR_C);
                        if (p_arg_end != 0)                                     /* End of command ? */
                        {
                            strncpy(param_str, p_arg_sta, (p_arg_end - p_arg_sta));

                            args_found = 1;
                        }
                    }
                }
            }
        }

        if (!args_found)
        {
            idx++;
        }
    }

    if (args_found)
    {
        args_valid = 0;

        while (mod_cmd_list[idx].inst[0] != 0)
        {
            if (strcmp(mod_cmd_list[idx].inst, inst_str) == 0)
            {
                if (strcmp(mod_cmd_list[idx].param, param_str) == 0)
                {
                    if (mod_cmd_list[idx].p_callback != 0)
                    {
                        if (value_str[0] != 0)                                  /* Value argument was received? */
                        {
                            args_valid = 1;

                            if (mod_cmd_list[idx].p_callback(value))
                            {
                                sprintf(string, 
                                        "%s:%s:%d:%s\r\n",
                                        inst_str,
                                        param_str,
                                        value,
                                        M_CMD_OK_S);

                                drv_uart_cmd_send_data(string, strlen(string));
                            }
                            else
                            {
                                /* Value invalid for the received parameter */

                                sprintf(string, 
                                        "%s:%s:%d:%s\r\n",
                                        inst_str,
                                        param_str,
                                        value,
                                        M_CMD_ERR_S);

                                drv_uart_cmd_send_data(string, strlen(string));
                            }
                        }
                        else
                        {
                            args_valid = 1;
                            
                            sprintf(string, 
                                    "%s:%s:%d\r\n",
                                    inst_str,
                                    param_str,
                                    mod_cmd_list[idx].p_callback(0));

                            drv_uart_cmd_send_data(string, strlen(string));
                        }
                    }
                    else
                    {
                        if (strlen(value_str))
                        {
                            sprintf(string, 
                                    "%s:%s:%s:%s\r\n",
                                    inst_str,
                                    param_str,
                                    value_str,
                                    M_CMD_ERR_S);
                        }
                        else
                        {
                            sprintf(string, 
                                    "%s:%s:%s\r\n",
                                    inst_str,
                                    param_str,
                                    M_CMD_ERR_S);
                        }

                        drv_uart_cmd_send_data(string, strlen(string));
                    }
                }
            }

            idx++;
        }

        if (!args_valid)
        {
            sprintf(string, 
                    "%s:%s:%s:%s\r\n",
                    inst_str,
                    param_str,
                    value_str,
                    M_CMD_ERR_S);

            drv_uart_cmd_send_data(string, strlen(string));
        }
    }
    else
    {
        if (strlen(value_str))
        {
            sprintf(string, 
                    "%s:%s:%s:%s\r\n",
                    inst_str,
                    param_str,
                    value_str,
                    M_CMD_ERR_S);
        }
        else
        {
            sprintf(string, 
                    "%s:%s:%s\r\n",
                    inst_str,
                    param_str,
                    M_CMD_ERR_S);
        }

        drv_uart_cmd_send_data(string, strlen(string));
    }
}


/*** END OF FILE ***/