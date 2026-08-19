/**
 * @file      m_cmd.c
 * @author    The OSLUV Project
 * @brief     Module for managing external commands
 *
 * @note Commands are accepted on two independent channels: the dedicated
 *       command UART (uart1, 9600 8N1) and the USB CDC stdio port. Each
 *       channel assembles commands in its own buffer and responses go back
 *       out on the channel the command arrived on. Firmware log output
 *       (printf) is interleaved with responses on the USB channel.
 */


/* Includes ------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/time.h"
#include "pico/stdio.h"
#include "m_cmd.h"
#include "d_uart_cmd.h"
#include "lamp.h"
#include "ui_main.h"
#include "serial.h"
#include "hourmeter.h"


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
#define CMD_PARAM_SERIAL_ID_S   "N"                                             /* GET-only: unique serial number */
#define CMD_PARAM_HOURS_ID_S    "H"                                             /* GET-only: lamp-on time, whole hours */
#define CMD_PARAM_ONSECS_ID_S   "T"                                             /* GET-only: lamp-on time, seconds */

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

typedef struct {

    uint8_t         buf[CMD_MAX_LEN_C];                                         /* Command assembly buffer */
    uint16_t        idx;                                                        /* Next free position in buf */
    absolute_time_t tmout;                                                      /* Inter-byte timeout deadline */
    uint16_t        (*p_recv)(uint8_t *p_buf, uint16_t max_len);                /* Pulls received bytes from the channel */
    void            (*p_send)(uint8_t *p_data, uint16_t len);                   /* Sends response bytes to the channel */

} CMD_CHANNEL_T;


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

static CMD_CHANNEL_T    cmd_uart_channel;                                       /* Dedicated command UART (uart1)  */
static CMD_CHANNEL_T    cmd_usb_channel;                                        /* USB CDC stdio port              */


/* Callback prototypes -------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/

static void     m_cmd_channel_handler(CMD_CHANNEL_T *p_ch);
static void     m_cmd_process(CMD_CHANNEL_T *p_ch);
static uint16_t m_cmd_uart_recv(uint8_t *p_buf, uint16_t max_len);
static uint16_t m_cmd_usb_recv(uint8_t *p_buf, uint16_t max_len);
static void     m_cmd_usb_send(uint8_t *p_data, uint16_t len);


/* Exported functions --------------------------------------------------------*/

/**
 * @brief External commands module initialization procedure
 * 
 */
void m_cmd_init(void)
{
    memset(&cmd_uart_channel, 0, sizeof(cmd_uart_channel));
    cmd_uart_channel.p_recv = m_cmd_uart_recv;
    cmd_uart_channel.p_send = uart_cmd_send_data;

    memset(&cmd_usb_channel, 0, sizeof(cmd_usb_channel));
    cmd_usb_channel.p_recv = m_cmd_usb_recv;
    cmd_usb_channel.p_send = m_cmd_usb_send;

    uart_cmd_init();
}

/**
 * @brief Handles external commands over the dedicated command UART and the
 * USB CDC (stdio) port
 *
 */
void m_cmd_handler(void)
{
    m_cmd_channel_handler(&cmd_uart_channel);
    m_cmd_channel_handler(&cmd_usb_channel);
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
 * @brief Accumulates received bytes for one command channel and processes the
 * command once its CR/LF terminator arrives
 *
 * @note A command that overruns the buffer without a terminator is dropped
 * and answered with @ref CMD_ERR_S; a partial command with no further bytes
 * for @ref CMD_TMOUT_MS_C is dropped and answered with @ref CMD_TMOUT_S.
 *
 * @param p_ch Command channel context
 */
static void m_cmd_channel_handler(CMD_CHANNEL_T *p_ch)
{
    uint16_t data_len;
    uint8_t string[CMD_MAX_LEN_C];

    /* Keep one byte spare so the buffer stays null-terminated for strchr/strstr */
    data_len = p_ch->p_recv(p_ch->buf + p_ch->idx, (CMD_MAX_LEN_C - 1) - p_ch->idx);
    if (data_len)
    {
        p_ch->idx += data_len;

        if (strchr(p_ch->buf, CMD_CR_CHAR_C) ||
            strchr(p_ch->buf, CMD_LF_CHAR_C))                                   /* End of command has been received ? */
        {
            m_cmd_process(p_ch);

            memset(p_ch->buf, 0, sizeof(p_ch->buf));

            p_ch->idx = 0;

            p_ch->tmout = 0;
        }
        else if (p_ch->idx >= (CMD_MAX_LEN_C - 1))                              /* Buffer full without a terminator ? */
        {
            memset(p_ch->buf, 0, sizeof(p_ch->buf));

            p_ch->idx = 0;

            p_ch->tmout = 0;

            sprintf(string,
                    "\r\n:%s\r\n",
                    CMD_ERR_S);

            p_ch->p_send(string, strlen(string));
        }
        else
        {
            p_ch->tmout = make_timeout_time_ms(CMD_TMOUT_MS_C);
        }
    }
    else if ((p_ch->tmout != 0) && (get_absolute_time() > p_ch->tmout))         /* Is timeout over? */
    {
        p_ch->tmout = 0;

        memset(p_ch->buf, 0, sizeof(p_ch->buf));

        p_ch->idx = 0;

        sprintf(string,
                "\r\n:%s\r\n",
                CMD_TMOUT_S);

        p_ch->p_send(string, strlen(string));
    }
}

/**
 * @brief Pulls received bytes from the dedicated command UART
 *
 * @param p_buf   Destination buffer
 * @param max_len Maximum bytes to pull
 * @return uint16_t Bytes actually pulled
 */
static uint16_t m_cmd_uart_recv(uint8_t *p_buf, uint16_t max_len)
{
    uint16_t data_len;

    data_len = uart_cmd_get_rcvd_data_len();
    if (data_len > max_len)
    {
        data_len = max_len;
    }

    if (data_len == 0)
    {
        return 0;
    }

    return uart_cmd_get_data(p_buf, data_len);
}

/**
 * @brief Pulls received bytes from the USB CDC (stdio) port
 *
 * @param p_buf   Destination buffer
 * @param max_len Maximum bytes to pull
 * @return uint16_t Bytes actually pulled
 */
static uint16_t m_cmd_usb_recv(uint8_t *p_buf, uint16_t max_len)
{
    uint16_t count;
    int      chr;

    count = 0;
    while (count < max_len)
    {
        chr = getchar_timeout_us(0);
        if (chr < 0)                                                            /* PICO_ERROR_TIMEOUT: no more data */
        {
            break;
        }

        p_buf[count++] = (uint8_t)chr;
    }

    return count;
}

/**
 * @brief Sends response bytes to the USB CDC (stdio) port
 *
 * @note Uses putchar_raw so response bytes are not CRLF-translated.
 *
 * @param p_data Response bytes to send
 * @param len    Number of bytes to send
 */
static void m_cmd_usb_send(uint8_t *p_data, uint16_t len)
{
    for (uint16_t idx = 0; idx < len; idx++)
    {
        putchar_raw(p_data[idx]);
    }

    stdio_flush();
}

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
 *
 * @note GET-only parameters N (serial number), H (lamp-on whole hours) and
 * T (lamp-on seconds) are handled outside @ref cmd_list because their values
 * do not fit the int16_t callback return type.
 *
 * @param p_ch Command channel the command arrived on; responses are sent back
 * through its @ref CMD_CHANNEL_T.p_send.
 */
static void m_cmd_process(CMD_CHANNEL_T *p_ch)
{
    uint8_t idx;
    uint8_t *p_arg_sta, *p_arg_end;
    uint8_t string[CMD_MAX_LEN_C];
    uint8_t args_found, args_valid;
    uint8_t inst_str[CMD_MAX_INST_LEN_C];
    uint8_t param_str[CMD_MAX_PARAM_LEN_C];
    uint8_t value_str[CMD_MAX_VAL_LEN_C];
    uint16_t value;

    /* GET-only parameters whose values don't fit the int16_t GET-callback
     * path are answered directly here, before the generic parse. */

    /* OL1.2: serial is a 13-char Base32 string. */
    if (strstr((char*)p_ch->buf,CMD_INST_GET_S ":" CMD_PARAM_SERIAL_ID_S))
    {
        uint8_t resp[CMD_MAX_LEN_C];
        snprintf((char*)resp, sizeof(resp), "%s:%s:%s\r\n",
                 CMD_INST_GET_S, CMD_PARAM_SERIAL_ID_S, serial_get_string());
        p_ch->p_send(resp, strlen((char*)resp));
        return;
    }

    /* bangladesh-study: lamp-on time is a uint32_t counter. G:H answers in
     * whole hours; G:T answers the same counter in seconds so test software
     * can verify the hour meter advances without waiting a full hour. */
    if (strstr((char*)p_ch->buf,CMD_INST_GET_S ":" CMD_PARAM_HOURS_ID_S))
    {
        uint8_t resp[CMD_MAX_LEN_C];
        snprintf((char*)resp, sizeof(resp), "%s:%s:%lu\r\n",
                 CMD_INST_GET_S, CMD_PARAM_HOURS_ID_S,
                 (unsigned long)hourmeter_get_on_hours());
        p_ch->p_send(resp, strlen((char*)resp));
        return;
    }

    if (strstr((char*)p_ch->buf,CMD_INST_GET_S ":" CMD_PARAM_ONSECS_ID_S))
    {
        uint8_t resp[CMD_MAX_LEN_C];
        snprintf((char*)resp, sizeof(resp), "%s:%s:%lu\r\n",
                 CMD_INST_GET_S, CMD_PARAM_ONSECS_ID_S,
                 (unsigned long)hourmeter_get_on_seconds());
        p_ch->p_send(resp, strlen((char*)resp));
        return;
    }

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

        p_arg_sta = strstr(p_ch->buf, string);
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

                                p_ch->p_send(string, strlen(string));
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

                                p_ch->p_send(string, strlen(string));
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

                            p_ch->p_send(string, strlen(string));
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

                        p_ch->p_send(string, strlen(string));
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

            p_ch->p_send(string, strlen(string));
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

        p_ch->p_send(string, strlen(string));
    }
}


/*** END OF FILE ***/