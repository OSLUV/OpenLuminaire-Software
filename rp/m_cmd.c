/**
 * @file      m_cmd.c
 * @author    The OSLUV Project
 * @brief     Module for managing external commands
 *  
 */


/* Includes ------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/time.h"
#include "m_cmd.h"
#include "d_uart_cmd.h"
#include "lamp.h"
#include "ui_main.h"


/* Private define ------------------------------------------------------------*/

#define CMD_MAX_INST_LEN_C      16
#define CMD_MAX_PARAM_LEN_C     16
#define CMD_MAX_VAL_LEN_C       16
#define CMD_MAX_LEN_C           64

#if CMD_MAX_LEN_C < (CMD_MAX_INST_LEN_C + CMD_MAX_PARAM_LEN_C + CMD_MAX_VAL_LEN_C + 8)
#warning "CMD_MAX_LEN_C is not enough."
#endif

#define CMD_SEPARATOR_CHAR_C    ':'
#define CMD_CR_CHAR_C           '\r'
#define CMD_LF_CHAR_C           '\n'
#define CMD_INST_SET_S          "S"
#define CMD_INST_GET_S          "G"
#define CMD_PARAM_LAMP_CTL_ID_S "L"
#define CMD_PARAM_LAMP_DIM_ID_S "D"

#define CMD_OK_S                "OK"
#define CMD_ERR_S               "ERR"
#define CMD_TMOUT_S             "TOUT"

#define CMD_TMOUT_MS_C          50                                              /* Timeout in ms to wait for more data to arrive */


/* Private typedef -----------------------------------------------------------*/

typedef struct {

    uint8_t inst[CMD_MAX_INST_LEN_C];
    uint8_t param[CMD_MAX_PARAM_LEN_C];
    int16_t (*p_callback)(uint16_t);

} CMD_CTL_T;


/* Global variables  ---------------------------------------------------------*/
/* Private variables  --------------------------------------------------------*/

/* Only for testing * /
static uint16_t  lamp_stt = 0;
static uint16_t  dim_value = 0;
int16_t lamp_set_stt(uint16_t value);
int16_t lamp_get_stt(uint16_t value);
int16_t lamp_set_dim(uint16_t value);
int16_t lamp_get_dim(uint16_t value);
/ * Only for testing */

static const CMD_CTL_T  cmd_list[] = 
{
    {CMD_INST_SET_S, CMD_PARAM_LAMP_CTL_ID_S, ui_main_lamp_set_stt },
    {CMD_INST_GET_S, CMD_PARAM_LAMP_CTL_ID_S, ui_main_lamp_get_stt },
    {CMD_INST_SET_S, CMD_PARAM_LAMP_DIM_ID_S, ui_main_lamp_set_dim },
    {CMD_INST_GET_S, CMD_PARAM_LAMP_DIM_ID_S, ui_main_lamp_get_dim },
    {0,              0,                       0                 }
};

static uint8_t          cmd_buf[CMD_MAX_LEN_C];
static uint16_t         cmd_idx;

static absolute_time_t  cmd_tmout;


/* Callback prototypes -------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/

static void m_cmd_process(void);


/* Exported functions --------------------------------------------------------*/

/**
 * @brief External commands module initialization procedure
 * 
 */
void m_cmd_init(void)
{
    cmd_idx = 0;

    uart_cmd_init();
}

/**
 * @brief Handles external commands over dedicated serial port
 * 
 */
void m_cmd_handler(void)
{
    uint16_t data_len;
    uint8_t string[CMD_MAX_LEN_C];

    data_len = uart_cmd_get_rcvd_data_len();
    if (data_len) 
    {
        if ((cmd_idx + data_len) >= CMD_MAX_LEN_C)
        {
            sprintf(string, 
                    "\r\n:%s\r\n",
                    CMD_ERR_S);

            uart_cmd_send_data(string, strlen(string));
        }
        else 
        {
            data_len = uart_cmd_get_data(cmd_buf + cmd_idx, data_len);
            if (data_len)
            {
                cmd_idx += data_len;
                if (strchr(cmd_buf, CMD_CR_CHAR_C) || 
                    strchr(cmd_buf, CMD_LF_CHAR_C))                             /* End of command has been received ? */
                {
                    m_cmd_process();

                    memset(cmd_buf, 0, sizeof(cmd_buf));

                    cmd_idx = 0;

                    cmd_tmout = 0;
                }
                else
                {
                    cmd_tmout =  make_timeout_time_ms (CMD_TMOUT_MS_C);
                }
            }
        }
    }
    else if ((cmd_tmout != 0) && (get_absolute_time() > cmd_tmout))             /* Is timeout over? */
    {
        cmd_tmout = 0;

        memset(cmd_buf, 0, sizeof(cmd_buf));

        cmd_idx = 0;

        sprintf(string, 
                "\r\n:%s\r\n",
                CMD_TMOUT_S);

        uart_cmd_send_data(string, strlen(string));
    }
}

/* Callback functions --------------------------------------------------------*/

/* Only for testing */
#if 0
int16_t lamp_set_stt(uint16_t value)
{
    if (value < 2)
    {
        lamp_stt = value;

        return 1;
    }

    return 0;
}

int16_t lamp_get_stt(uint16_t value)
{
    return lamp_stt;
}

int16_t lamp_set_dim(uint16_t value)
{
    if ((value == 20) || (value == 40) || (value == 70) || (value == 100))
    {
        dim_value = value;

        return 1;
    }

    return 0;
}

int16_t lamp_get_dim(uint16_t value)
{
    return dim_value;
}
#endif
/* Only for testing */

/* Private functions ---------------------------------------------------------*/

/**
 * @brief Processes the received command and executes defined routine according 
 * to commands list @ref cmd_list.
 * 
 * @note Initial commands format is I:P:V where I is for Instruction (i.e. SET 
 * or GET), P is for Parameter (i.e. Lamp state, lamp dim setting) and V is for
 * Value needed to set to the required parameter.
 * 
 * @note If the received command is not found in the commands list @ref cmd_list
 *  error will be notified back to sender. Or if the value is not validated by 
 * the corresponding callback an error will be notified back to sender.
 */
static void m_cmd_process(void)
{
    uint8_t idx;
    uint8_t *p_arg_sta, *p_arg_end;
    uint8_t string[CMD_MAX_LEN_C];
    uint8_t args_found, args_valid;
    uint8_t inst_str[CMD_MAX_INST_LEN_C];
    uint8_t param_str[CMD_MAX_PARAM_LEN_C];
    uint8_t value_str[CMD_MAX_VAL_LEN_C];
    uint16_t value;

    memset(inst_str, 0, sizeof(inst_str));
    memset(param_str, 0, sizeof(param_str));
    memset(value_str, 0, sizeof(value_str));
    args_found = 0;
    
    /* Get all command parameters */
    idx = 0;
    while ((cmd_list[idx].inst[0] != 0) && (args_found == 0))
    {
        memset(string, 0, sizeof(string));
        strcpy(string, cmd_list[idx].inst);
        string[strlen(cmd_list[idx].inst)] = CMD_SEPARATOR_CHAR_C;

        p_arg_sta = strstr(cmd_buf, string);
        if (p_arg_sta != 0)
        {
            strcpy(inst_str, cmd_list[idx].inst);

            p_arg_end = strchr(p_arg_sta, CMD_SEPARATOR_CHAR_C);
            if (p_arg_end != 0)
            {
                p_arg_sta = p_arg_end + 1;
                p_arg_end = strchr(p_arg_sta, CMD_SEPARATOR_CHAR_C);
                if (p_arg_end != 0)
                {
                    strncpy(param_str, p_arg_sta, (p_arg_end - p_arg_sta));

                    strcpy(value_str, p_arg_end + 1);

                    if ((value_str[0] != CMD_CR_CHAR_C) &&
                        (value_str[0] != CMD_LF_CHAR_C))                        /* value argument is not empty? */
                    {
                        p_arg_end = strchr(value_str, CMD_CR_CHAR_C);
                        if (p_arg_end)
                        {
                            *p_arg_end = 0;
                        }
                        else
                        {
                            p_arg_end = strchr(value_str, CMD_LF_CHAR_C);
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
                    p_arg_end = strchr(p_arg_sta, CMD_CR_CHAR_C);
                    if (p_arg_end != 0)                                         /* End of command ? */
                    {
                        strncpy(param_str, p_arg_sta, (p_arg_end - p_arg_sta));

                        args_found = 1;
                    }
                    else 
                    {
                        p_arg_end = strchr(p_arg_sta, CMD_LF_CHAR_C);
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

        while (cmd_list[idx].inst[0] != 0)
        {
            if (strcmp(cmd_list[idx].inst, inst_str) == 0)
            {
                if (strcmp(cmd_list[idx].param, param_str) == 0)
                {
                    if (cmd_list[idx].p_callback != 0)
                    {
                        if (value_str[0] != 0)                                  /* Value argument was received? */
                        {
                            args_valid = 1;

                            if (cmd_list[idx].p_callback(value))
                            {
                                sprintf(string, 
                                        "%s:%s:%d:%s\r\n",
                                        inst_str,
                                        param_str,
                                        value,
                                        CMD_OK_S);

                                uart_cmd_send_data(string, strlen(string));
                            }
                            else
                            {
                                /* Value invalid for the received parameter */

                                sprintf(string, 
                                        "%s:%s:%d:%s\r\n",
                                        inst_str,
                                        param_str,
                                        value,
                                        CMD_ERR_S);

                                uart_cmd_send_data(string, strlen(string));
                            }
                        }
                        else
                        {
                            args_valid = 1;
                            
                            sprintf(string, 
                                    "%s:%s:%d\r\n",
                                    inst_str,
                                    param_str,
                                    cmd_list[idx].p_callback(0));

                            uart_cmd_send_data(string, strlen(string));
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
                                    CMD_ERR_S);
                        }
                        else
                        {
                            sprintf(string, 
                                    "%s:%s:%s\r\n",
                                    inst_str,
                                    param_str,
                                    CMD_ERR_S);
                        }

                        uart_cmd_send_data(string, strlen(string));
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
                    CMD_ERR_S);

            uart_cmd_send_data(string, strlen(string));
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
                    CMD_ERR_S);
        }
        else
        {
            sprintf(string, 
                    "%s:%s:%s\r\n",
                    inst_str,
                    param_str,
                    CMD_ERR_S);
        }

        uart_cmd_send_data(string, strlen(string));
    }
}


/*** END OF FILE ***/