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
 *
 * @note The USB channel is interactive: it echoes input (with backspace
 *       editing), shows a command help on bare ENTER, "--help", or an
 *       invalid command, and allows 2 s between keystrokes. The UART
 *       channel preserves the legacy protocol for existing external
 *       controllers: no echo, compact ERR-only replies, backspace/DEL are
 *       ordinary data bytes, a bare terminator answers ::ERR, and the
 *       original 50 ms inter-byte timeout so a stale fragment cannot merge
 *       into the next command ("--help" still answers there, since no
 *       controller ever sends it).
 *
 * @note A line that overruns the command buffer is answered with one ERR
 *       and discarded through its terminator, so its tail cannot be
 *       misread as a fresh command.
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
#define CMD_BS_CHAR_C           '\b'                                            /* Backspace (0x08) */
#define CMD_DEL_CHAR_C          0x7F                                            /* DEL; sent for backspace by some terminals */
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
#define CMD_HELP_CMD_S          "--help"

#define CMD_TMOUT_UART_MS_C     50                                              /* Inter-byte timeout in ms on the command UART.
                                                                                   Kept at the legacy value so a stale fragment
                                                                                   (lost terminator) is flushed before an external
                                                                                   controller's next command can merge with it. */

#define CMD_TMOUT_USB_MS_C      2000                                            /* Inter-byte timeout in ms on the USB terminal.
                                                                                   Long enough to type commands interactively in a
                                                                                   terminal that sends per keystroke; it only exists
                                                                                   to flush stale partial commands. */

#define CMD_USB_SEND_MS_C       100                                             /* Upper bound in ms on one USB response send. A
                                                                                   host that holds DTR but stops draining the CDC
                                                                                   port must not stall the main loop (and its
                                                                                   1500 ms watchdog); the rest of the response is
                                                                                   dropped instead. */


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
    uint32_t        tmout_ms;                                                   /* Inter-byte timeout for this channel */
    bool            interactive;                                                /* Echo input + show help (terminal use) */
    bool            overflow;                                                   /* Discarding an oversized line up to its terminator */
    uint8_t         prev_rx;                                                    /* Previous received byte (CR+LF collapsing) */
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

/* Keep in sync with cmd_list and the GET-only special cases in m_cmd_process */
static const char       cmd_help_str[] =
    "\r\n"
    "OSLUV lamp serial commands\r\n"
    "  G:L                 lamp state (1=on 0=off)\r\n"
    "  S:L:<0|1>           lamp off / on\r\n"
    "  G:D                 dim level (percent)\r\n"
    "  S:D:<20|40|70|100>  set dim level (dimmable lamps only)\r\n"
    "  G:N                 serial number\r\n"
    "  G:H                 lamp-on time, whole hours\r\n"
    "  G:T                 lamp-on time, seconds\r\n"
    "  --help              this help\r\n"
    "End commands with ENTER. Failures answer :ERR; idle partial\r\n"
    "input is dropped with :TOUT.\r\n";


/* Callback prototypes -------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/

static void     m_cmd_channel_handler(CMD_CHANNEL_T *p_ch);
static void     m_cmd_process(CMD_CHANNEL_T *p_ch);
static void     m_cmd_send_help(CMD_CHANNEL_T *p_ch);
static bool     m_cmd_match_exact(CMD_CHANNEL_T *p_ch, const char *p_cmd);
static void     m_cmd_send_get_resp(CMD_CHANNEL_T *p_ch, const char *p_param, const char *p_value);
static void     m_cmd_send_err_resp(CMD_CHANNEL_T *p_ch, uint8_t *p_inst, uint8_t *p_param, uint8_t *p_value);
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
    cmd_uart_channel.interactive = false;                                       /* Legacy protocol: no echo, no help spam */
    cmd_uart_channel.tmout_ms    = CMD_TMOUT_UART_MS_C;
    cmd_uart_channel.p_recv      = m_cmd_uart_recv;
    cmd_uart_channel.p_send      = uart_cmd_send_data;

    memset(&cmd_usb_channel, 0, sizeof(cmd_usb_channel));
    cmd_usb_channel.interactive  = true;                                        /* Interactive terminal port */
    cmd_usb_channel.tmout_ms     = CMD_TMOUT_USB_MS_C;
    cmd_usb_channel.p_recv       = m_cmd_usb_recv;
    cmd_usb_channel.p_send       = m_cmd_usb_send;

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
 * @brief Accumulates received bytes for one command channel and processes each
 * command as its CR/LF terminator arrives
 *
 * @note Interactive niceties on channels with the interactive flag: input is
 * echoed back (Enter as CRLF, backspace/DEL as erase) and a bare ENTER press
 * shows the command help. On non-interactive channels backspace/DEL stay
 * ordinary data bytes and a bare terminator answers ::ERR, both as in the
 * legacy protocol.
 *
 * @note A command that overruns the buffer is answered with @ref CMD_ERR_S
 * once and discarded through its terminator; a partial command with no
 * further bytes for the channel's timeout is dropped and answered with
 * @ref CMD_TMOUT_S.
 *
 * @param p_ch Command channel context
 */
static void m_cmd_channel_handler(CMD_CHANNEL_T *p_ch)
{
    uint16_t data_len;
    uint8_t rx_byte;
    uint8_t chunk[CMD_MAX_LEN_C];
    uint8_t string[CMD_MAX_LEN_C];

    data_len = p_ch->p_recv(chunk, sizeof(chunk));
    if (data_len)
    {
        for (uint16_t idx = 0; idx < data_len; idx++)
        {
            rx_byte = chunk[idx];

            if ((rx_byte == CMD_CR_CHAR_C) || (rx_byte == CMD_LF_CHAR_C))
            {
                if (p_ch->overflow)                                             /* Oversized line ends here: drop it whole */
                {
                    p_ch->overflow = false;
                }
                else if (p_ch->idx == 0)                                        /* No pending command */
                {
                    if (!((rx_byte == CMD_LF_CHAR_C) &&
                          (p_ch->prev_rx == CMD_CR_CHAR_C)))                    /* The LF completing a CR+LF pair is not a new ENTER */
                    {
                        if (p_ch->interactive)
                        {
                            m_cmd_send_help(p_ch);                              /* Bare ENTER: show the help */
                        }
                        else
                        {
                            p_ch->p_send((uint8_t*)"::" CMD_ERR_S "\r\n",       /* Legacy protocol reply to a bare terminator */
                                         strlen("::" CMD_ERR_S "\r\n"));
                        }
                    }
                }
                else
                {
                    if (p_ch->interactive)
                    {
                        p_ch->p_send((uint8_t*)"\r\n", 2);
                    }

                    p_ch->buf[p_ch->idx++] = rx_byte;                           /* Parser delimits fields on the terminator */

                    m_cmd_process(p_ch);

                    memset(p_ch->buf, 0, sizeof(p_ch->buf));

                    p_ch->idx = 0;
                }
            }
            else if (p_ch->overflow)
            {
                /* Discarding the rest of an oversized line */
            }
            else if (p_ch->interactive &&
                     ((rx_byte == CMD_BS_CHAR_C) || (rx_byte == CMD_DEL_CHAR_C)))
            {                                                                   /* Terminal line editing; on the legacy UART these are data */
                if (p_ch->idx > 0)
                {
                    p_ch->idx--;
                    p_ch->buf[p_ch->idx] = 0;

                    p_ch->p_send((uint8_t*)"\b \b", 3);                         /* Erase the char on the terminal */
                }
            }
            else if (p_ch->idx >= (CMD_MAX_LEN_C - 2))                          /* Buffer full: keep room for terminator + null */
            {
                p_ch->overflow = true;                                          /* One ERR per oversized line; its tail is discarded */

                memset(p_ch->buf, 0, sizeof(p_ch->buf));

                p_ch->idx = 0;

                sprintf(string,
                        "\r\n:%s\r\n",
                        CMD_ERR_S);

                p_ch->p_send(string, strlen(string));
            }
            else
            {
                p_ch->buf[p_ch->idx++] = rx_byte;

                if (p_ch->interactive)
                {
                    p_ch->p_send(&rx_byte, 1);
                }
            }

            p_ch->prev_rx = rx_byte;
        }

        p_ch->tmout = ((p_ch->idx > 0) || p_ch->overflow)
                          ? make_timeout_time_ms(p_ch->tmout_ms)
                          : 0;
    }
    else if ((p_ch->tmout != 0) && (get_absolute_time() > p_ch->tmout))         /* Is timeout over? */
    {
        p_ch->tmout = 0;

        p_ch->overflow = false;

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
 * @note The send is bounded to @ref CMD_USB_SEND_MS_C. A host that holds DTR
 * asserted but stops draining the port makes each blocked write wait out
 * PICO_STDIO_USB_STDOUT_TIMEOUT_US; unbounded, a long response (the help
 * text) could stall the main loop past its 1500 ms watchdog and reboot the
 * lamp. The remainder of the response is dropped instead.
 *
 * @param p_data Response bytes to send
 * @param len    Number of bytes to send
 */
static void m_cmd_usb_send(uint8_t *p_data, uint16_t len)
{
    absolute_time_t deadline = make_timeout_time_ms(CMD_USB_SEND_MS_C);

    for (uint16_t idx = 0; idx < len; idx++)
    {
        putchar_raw(p_data[idx]);

        if (get_absolute_time() > deadline)                                     /* Host stopped draining: drop the rest */
        {
            break;
        }
    }

    stdio_flush();
}

/**
 * @brief Sends the terminal help text listing all commands
 *
 * @param p_ch Command channel to answer on
 */
static void m_cmd_send_help(CMD_CHANNEL_T *p_ch)
{
    p_ch->p_send((uint8_t*)cmd_help_str, strlen(cmd_help_str));
}

/**
 * @brief Tests whether the assembled command is exactly the given command
 * string followed by its CR/LF terminator
 *
 * @note Anchored at the start of the buffer and terminated right after, so a
 * command merely containing the string (e.g. "G:HELP" vs "G:H") does not
 * match and falls through to the generic parse and its ERR reply.
 *
 * @param p_ch  Command channel holding the assembled command
 * @param p_cmd Command string to test for
 * @return true if the buffer holds exactly this command
 */
static bool m_cmd_match_exact(CMD_CHANNEL_T *p_ch, const char *p_cmd)
{
    uint16_t len = (uint16_t)strlen(p_cmd);

    if (strncmp((char*)p_ch->buf, p_cmd, len) != 0)
    {
        return false;
    }

    return (p_ch->buf[len] == CMD_CR_CHAR_C) || (p_ch->buf[len] == CMD_LF_CHAR_C);
}

/**
 * @brief Sends a GET reply of the form G:<param>:<value>
 *
 * @param p_ch    Command channel to answer on
 * @param p_param Parameter identifier string
 * @param p_value Value string
 */
static void m_cmd_send_get_resp(CMD_CHANNEL_T *p_ch, const char *p_param, const char *p_value)
{
    uint8_t resp[CMD_MAX_LEN_C];

    snprintf((char*)resp, sizeof(resp), "%s:%s:%s\r\n",
             CMD_INST_GET_S, p_param, p_value);

    p_ch->p_send(resp, strlen((char*)resp));
}

/**
 * @brief Sends an error reply for a failed command, plus the help text on
 * interactive channels
 *
 * @param p_ch    Command channel to answer on
 * @param p_inst  Instruction field (may be empty)
 * @param p_param Parameter field (may be empty)
 * @param p_value Value field (may be empty), or NULL to omit the field and
 *                its separator entirely
 */
static void m_cmd_send_err_resp(CMD_CHANNEL_T *p_ch, uint8_t *p_inst, uint8_t *p_param, uint8_t *p_value)
{
    uint8_t string[CMD_MAX_LEN_C];

    if (p_value != 0)
    {
        snprintf((char*)string, sizeof(string),
                 "%s:%s:%s:%s\r\n",
                 p_inst,
                 p_param,
                 p_value,
                 CMD_ERR_S);
    }
    else
    {
        snprintf((char*)string, sizeof(string),
                 "%s:%s:%s\r\n",
                 p_inst,
                 p_param,
                 CMD_ERR_S);
    }

    p_ch->p_send(string, strlen(string));

    if (p_ch->interactive)
    {
        m_cmd_send_help(p_ch);
    }
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

    /* Explicit help request (answered on any channel). */
    if (m_cmd_match_exact(p_ch, CMD_HELP_CMD_S))
    {
        m_cmd_send_help(p_ch);
        return;
    }

    /* GET-only parameters whose values don't fit the int16_t GET-callback
     * path are answered directly here, before the generic parse. Matched
     * exactly, so malformed lines merely containing them still get the
     * generic parse's ERR reply. */

    /* OL1.2: serial is a 13-char Base32 string. */
    if (m_cmd_match_exact(p_ch, CMD_INST_GET_S ":" CMD_PARAM_SERIAL_ID_S))
    {
        m_cmd_send_get_resp(p_ch, CMD_PARAM_SERIAL_ID_S, serial_get_string());
        return;
    }

    /* bangladesh-study: lamp-on time is a uint32_t counter. G:H answers in
     * whole hours; G:T answers the same counter in seconds so test software
     * can verify the hour meter advances without waiting a full hour. */
    if (m_cmd_match_exact(p_ch, CMD_INST_GET_S ":" CMD_PARAM_HOURS_ID_S))
    {
        char num_str[12];

        snprintf(num_str, sizeof(num_str), "%lu",
                 (unsigned long)hourmeter_get_on_hours());

        m_cmd_send_get_resp(p_ch, CMD_PARAM_HOURS_ID_S, num_str);
        return;
    }

    if (m_cmd_match_exact(p_ch, CMD_INST_GET_S ":" CMD_PARAM_ONSECS_ID_S))
    {
        char num_str[12];

        snprintf(num_str, sizeof(num_str), "%lu",
                 (unsigned long)hourmeter_get_on_seconds());

        m_cmd_send_get_resp(p_ch, CMD_PARAM_ONSECS_ID_S, num_str);
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
                    if (((p_arg_end - p_arg_sta) < CMD_MAX_PARAM_LEN_C) &&
                        (strlen((char*)(p_arg_end + 1)) < CMD_MAX_VAL_LEN_C))   /* Fields must fit their buffers; oversized
                                                                                   fields fall through to the ERR reply */
                    {
                        strncpy(param_str, p_arg_sta, (p_arg_end - p_arg_sta));

                        strcpy(value_str, p_arg_end + 1);

                        if ((value_str[0] != CMD_CR_CHAR_C) &&
                            (value_str[0] != CMD_LF_CHAR_C))                    /* value argument is not empty? */
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
                            value_str[0] = 0;                                   /* Since argument is empty, make sure is null */
                        }
                    }
                }
                else
                {
                    p_arg_end = strchr(p_arg_sta, CMD_CR_CHAR_C);
                    if (p_arg_end == 0)
                    {
                        p_arg_end = strchr(p_arg_sta, CMD_LF_CHAR_C);
                    }

                    if ((p_arg_end != 0) &&
                        ((p_arg_end - p_arg_sta) < CMD_MAX_PARAM_LEN_C))        /* End of command, param fits its buffer ? */
                    {
                        strncpy(param_str, p_arg_sta, (p_arg_end - p_arg_sta));

                        args_found = 1;
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

                                sprintf(string, "%d", value);

                                m_cmd_send_err_resp(p_ch, inst_str, param_str, string);
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
                        m_cmd_send_err_resp(p_ch, inst_str, param_str,
                                            strlen(value_str) ? value_str : 0);
                    }
                }
            }

            idx++;
        }

        if (!args_valid)
        {
            m_cmd_send_err_resp(p_ch, inst_str, param_str, value_str);
        }
    }
    else
    {
        m_cmd_send_err_resp(p_ch, inst_str, param_str,
                            strlen(value_str) ? value_str : 0);
    }
}


/*** END OF FILE ***/