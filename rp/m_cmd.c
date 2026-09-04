/**
 * @file      m_cmd.c
 * @author    The OSLUV Project
 * @brief     Module for managing external commands
 *
 * @note Commands are accepted on two independent channels: the dedicated
 *       command UART (uart1, 9600 8N1) and the USB CDC stdio port. Each
 *       channel assembles commands in its own buffer and responses go back
 *       out on the channel the command arrived on. Firmware log output shares
 *       the USB stream and can appear between command responses.
 *
 * @note The USB channel is interactive: it echoes input (with backspace
 *       editing), shows command help on bare ENTER, "--help", or an invalid
 *       command, and allows 2 s between keystrokes. The UART channel preserves
 *       the compact legacy protocol: no echo or verbose help, backspace/DEL
 *       are ordinary data bytes, a bare terminator answers ::ERR, and the
 *       original 50 ms inter-byte timeout is enforced from ISR-recorded gaps.
 *
 * @note A line that overruns either receive buffer is answered with one ERR
 *       and discarded through its terminator, so a retained tail cannot be
 *       misread as a fresh command.
 */


/* Includes ------------------------------------------------------------------*/

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "pico/time.h"
#include "pico/stdio.h"
#include "m_cmd.h"
#include "d_uart_cmd.h"
#include "ui_main.h"
#include "serial.h"
#include "hourmeter.h"


/* Private define ------------------------------------------------------------*/

#define CMD_MAX_INST_LEN_C          16
#define CMD_MAX_PARAM_LEN_C         16
#define CMD_MAX_VAL_LEN_C           16
#define CMD_MAX_LEN_C               64
#define CMD_RX_CHUNK_LEN_C          6                                           /* At most one state-changing command per channel/pass. */

#if CMD_MAX_LEN_C < (CMD_MAX_INST_LEN_C + CMD_MAX_PARAM_LEN_C + CMD_MAX_VAL_LEN_C + 8)
#warning "CMD_MAX_LEN_C is not enough."
#endif

#define CMD_SEPARATOR_CHAR_C        ':'
#define CMD_CR_CHAR_C               '\r'
#define CMD_LF_CHAR_C               '\n'
#define CMD_BS_CHAR_C               '\b'                                        /* Backspace (0x08) */
#define CMD_DEL_CHAR_C              0x7F                                        /* DEL; sent for backspace by some terminals */
#define CMD_INST_SET_S              "S"
#define CMD_INST_GET_S              "G"
#define CMD_PARAM_LAMP_CTL_ID_S     "L"
#define CMD_PARAM_LAMP_DIM_ID_S     "D"
#define CMD_PARAM_SERIAL_ID_S       "N"                                        /* GET-only: unique serial number */
#define CMD_PARAM_HOURS_ID_S        "H"                                        /* GET-only: lamp-on time, whole hours */
#define CMD_PARAM_ONSECS_ID_S       "T"                                        /* GET-only: lamp-on time, seconds */

#define CMD_OK_S                    "OK"
#define CMD_ERR_S                   "ERR"
#define CMD_TMOUT_S                 "TOUT"
#define CMD_HELP_CMD_S              "--help"

#define CMD_TMOUT_UART_MS_C         (UART_CMD_INTERBYTE_TIMEOUT_US_C / 1000U)
#define CMD_TMOUT_USB_MS_C          2000U                                       /* Long enough for human typing. */
#define CMD_CRLF_PAIR_MS_C          100U                                        /* CR and LF must be adjacent in time to collapse. */
#define CMD_USB_SEND_MS_C           100U                                        /* Total budget for one USB response. */
#define CMD_USB_HANDLER_SEND_MS_C   500U                                        /* Aggregate response budget per main-loop pass. */
#define CMD_USB_WRITE_CHUNK_C       64U                                         /* One RP2040 CDC TX FIFO/endpoint packet. */


/* Private typedef -----------------------------------------------------------*/

typedef struct {

    const char *inst;
    const char *param;
    int16_t     (*p_callback)(uint16_t);

} CMD_CTL_T;

typedef struct {

    uint8_t         buf[CMD_MAX_LEN_C];                                         /* Command assembly buffer */
    uint16_t        idx;                                                        /* Next free position in buf */
    absolute_time_t tmout;                                                      /* Idle timeout deadline */
    absolute_time_t crlf_deadline;                                              /* Maximum adjacency window for CR+LF */
    absolute_time_t send_deadline;                                              /* Whole-handler output budget (USB) */
    uint32_t        tmout_ms;                                                   /* Idle timeout for this channel */
    bool            interactive;                                               /* Echo input + show help (terminal use) */
    bool            overflow;                                                  /* Discarding a damaged/oversized line */
    uint8_t         prev_rx;                                                    /* Previous received byte for CR+LF collapse */
    uint16_t        (*p_recv)(uint8_t *p_buf, uint8_t *p_rx_flags, uint16_t max_len);
    void            (*p_send)(const uint8_t *p_data, uint16_t len);

} CMD_CHANNEL_T;


/* Private variables ---------------------------------------------------------*/

static const CMD_CTL_T cmd_list[] =
{
    {CMD_INST_SET_S, CMD_PARAM_LAMP_CTL_ID_S, ui_main_lamp_set_stt},
    {CMD_INST_GET_S, CMD_PARAM_LAMP_CTL_ID_S, ui_main_lamp_get_stt},
    {CMD_INST_SET_S, CMD_PARAM_LAMP_DIM_ID_S, ui_main_lamp_set_dim},
    {CMD_INST_GET_S, CMD_PARAM_LAMP_DIM_ID_S, ui_main_lamp_get_dim},
    {NULL,           NULL,                    NULL}
};

static CMD_CHANNEL_T cmd_uart_channel;                                         /* Dedicated command UART (uart1) */
static CMD_CHANNEL_T cmd_usb_channel;                                          /* USB CDC stdio port */

/* Keep in sync with cmd_list and the GET-only special cases in m_cmd_process. */
static const char cmd_help_str[] =
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


/* Private function prototypes -----------------------------------------------*/

static void            m_cmd_channel_handler(CMD_CHANNEL_T *p_ch);
static void            m_cmd_process(CMD_CHANNEL_T *p_ch);
static void            m_cmd_reset_line(CMD_CHANNEL_T *p_ch);
static void            m_cmd_send_help(CMD_CHANNEL_T *p_ch);
static void            m_cmd_send_timeout_resp(CMD_CHANNEL_T *p_ch);
static bool            m_cmd_match_exact(const CMD_CHANNEL_T *p_ch, const char *p_cmd);
static bool            m_cmd_split_fields(const CMD_CHANNEL_T *p_ch,
                                          char *p_inst,
                                          char *p_param,
                                          char *p_value,
                                          bool *p_has_value);
static bool            m_cmd_parse_u16(const char *p_str, uint16_t *p_value);
static const CMD_CTL_T *m_cmd_find_ctl(const char *p_inst, const char *p_param);
static void            m_cmd_send_get_resp(CMD_CHANNEL_T *p_ch, const char *p_param, const char *p_value);
static void            m_cmd_send_err_resp(CMD_CHANNEL_T *p_ch,
                                           const char *p_inst,
                                           const char *p_param,
                                           const char *p_value);
static uint16_t        m_cmd_uart_recv(uint8_t *p_buf, uint8_t *p_rx_flags, uint16_t max_len);
static uint16_t        m_cmd_usb_recv(uint8_t *p_buf, uint8_t *p_rx_flags, uint16_t max_len);
static void            m_cmd_usb_send(const uint8_t *p_data, uint16_t len);


/* Exported functions --------------------------------------------------------*/

/**
 * @brief External commands module initialization procedure
 */
void m_cmd_init(void)
{
    memset(&cmd_uart_channel, 0, sizeof(cmd_uart_channel));
    cmd_uart_channel.interactive = false;
    cmd_uart_channel.tmout_ms    = CMD_TMOUT_UART_MS_C;
    cmd_uart_channel.p_recv      = m_cmd_uart_recv;
    cmd_uart_channel.p_send      = uart_cmd_send_data;

    memset(&cmd_usb_channel, 0, sizeof(cmd_usb_channel));
    cmd_usb_channel.interactive = true;
    cmd_usb_channel.tmout_ms    = CMD_TMOUT_USB_MS_C;
    cmd_usb_channel.p_recv      = m_cmd_usb_recv;
    cmd_usb_channel.p_send      = m_cmd_usb_send;

    uart_cmd_init();
}

/**
 * @brief Handles external commands over the dedicated command UART and USB CDC
 */
void m_cmd_handler(void)
{
    m_cmd_channel_handler(&cmd_uart_channel);
    m_cmd_channel_handler(&cmd_usb_channel);
}


/* Private functions ---------------------------------------------------------*/

/**
 * @brief Accumulates and processes received bytes for one command channel
 *
 * UART receive metadata marks physical inter-byte gaps and driver data loss.
 * A gap expires a pending fragment before the following byte is considered;
 * data loss discards the damaged line through its terminator.
 *
 * @param p_ch Command channel context
 */
static void m_cmd_channel_handler(CMD_CHANNEL_T *p_ch)
{
    uint16_t data_len;
    uint8_t  chunk[CMD_RX_CHUNK_LEN_C];
    uint8_t  rx_flags[CMD_RX_CHUNK_LEN_C];

    if (p_ch->interactive)
    {
        p_ch->send_deadline = make_timeout_time_ms(CMD_USB_HANDLER_SEND_MS_C);
    }

    if ((p_ch->prev_rx == CMD_CR_CHAR_C) &&
        (p_ch->crlf_deadline != 0) &&
        time_reached(p_ch->crlf_deadline))
    {
        p_ch->prev_rx = 0;
        p_ch->crlf_deadline = 0;
    }

    data_len = p_ch->p_recv(chunk, rx_flags, sizeof(chunk));
    if (data_len > 0)
    {
        for (uint16_t idx = 0; idx < data_len; idx++)
        {
            uint8_t rx_byte;
            bool    collapse_lf;

            rx_byte = chunk[idx];

            if (((rx_flags[idx] & UART_CMD_RX_FLAG_DATA_LOSS_C) != 0) &&
                ((rx_flags[idx] & UART_CMD_RX_FLAG_GAP_BEFORE_C) == 0))
            {
                bool already_discarding;

                already_discarding = p_ch->overflow;
                m_cmd_reset_line(p_ch);
                p_ch->overflow = true;
                p_ch->prev_rx = 0;
                p_ch->crlf_deadline = 0;

                if (!already_discarding)
                {
                    static const uint8_t err_resp[] = "\r\n:" CMD_ERR_S "\r\n";
                    p_ch->p_send(err_resp, (uint16_t)(sizeof(err_resp) - 1));
                }
            }
            else if ((rx_flags[idx] & UART_CMD_RX_FLAG_GAP_BEFORE_C) != 0)
            {
                bool had_pending;
                bool was_overflow;

                had_pending = p_ch->idx > 0;
                was_overflow = p_ch->overflow;
                m_cmd_reset_line(p_ch);
                p_ch->prev_rx = 0;
                p_ch->crlf_deadline = 0;

                if ((rx_flags[idx] & UART_CMD_RX_FLAG_DATA_LOSS_C) != 0)
                {
                    static const uint8_t err_resp[] = "\r\n:" CMD_ERR_S "\r\n";
                    p_ch->p_send(err_resp, (uint16_t)(sizeof(err_resp) - 1));
                }
                else if (!was_overflow && had_pending)
                {
                    m_cmd_send_timeout_resp(p_ch);
                }
            }

            collapse_lf = (rx_byte == CMD_LF_CHAR_C) &&
                          (p_ch->prev_rx == CMD_CR_CHAR_C) &&
                          (p_ch->crlf_deadline != 0) &&
                          !time_reached(p_ch->crlf_deadline) &&
                          ((rx_flags[idx] & (UART_CMD_RX_FLAG_GAP_BEFORE_C |
                                             UART_CMD_RX_FLAG_DATA_LOSS_C)) == 0);

            if ((rx_byte == CMD_CR_CHAR_C) || (rx_byte == CMD_LF_CHAR_C))
            {
                if (p_ch->overflow)
                {
                    p_ch->overflow = false;                                    /* Damaged line ends here. */
                }
                else if (p_ch->idx == 0)
                {
                    if (!collapse_lf)
                    {
                        if (p_ch->interactive)
                        {
                            m_cmd_send_help(p_ch);
                        }
                        else
                        {
                            static const uint8_t err_resp[] = "::" CMD_ERR_S "\r\n";
                            p_ch->p_send(err_resp, (uint16_t)(sizeof(err_resp) - 1));
                        }
                    }
                }
                else
                {
                    if (p_ch->interactive)
                    {
                        static const uint8_t newline[] = "\r\n";
                        p_ch->p_send(newline, (uint16_t)(sizeof(newline) - 1));
                    }

                    p_ch->buf[p_ch->idx++] = rx_byte;
                    m_cmd_process(p_ch);
                    m_cmd_reset_line(p_ch);
                }
            }
            else if (p_ch->overflow)
            {
                /* Discard the rest of the damaged line. */
            }
            else if (p_ch->interactive &&
                     ((rx_byte == CMD_BS_CHAR_C) || (rx_byte == CMD_DEL_CHAR_C)))
            {
                if (p_ch->idx > 0)
                {
                    static const uint8_t erase[] = "\b \b";

                    p_ch->idx--;
                    p_ch->buf[p_ch->idx] = 0;
                    p_ch->p_send(erase, (uint16_t)(sizeof(erase) - 1));
                }
            }
            else if (p_ch->idx >= (CMD_MAX_LEN_C - 2))
            {
                static const uint8_t err_resp[] = "\r\n:" CMD_ERR_S "\r\n";

                m_cmd_reset_line(p_ch);
                p_ch->overflow = true;
                p_ch->p_send(err_resp, (uint16_t)(sizeof(err_resp) - 1));
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
            if (rx_byte == CMD_CR_CHAR_C)
            {
                p_ch->crlf_deadline = make_timeout_time_ms(CMD_CRLF_PAIR_MS_C);
            }
            else
            {
                p_ch->crlf_deadline = 0;
            }
        }

        p_ch->tmout = ((p_ch->idx > 0) || p_ch->overflow)
                          ? make_timeout_time_ms(p_ch->tmout_ms)
                          : 0;
    }
    else if ((p_ch->tmout != 0) && time_reached(p_ch->tmout))
    {
        bool was_overflow;

        was_overflow = p_ch->overflow;
        m_cmd_reset_line(p_ch);

        if (!was_overflow)
        {
            m_cmd_send_timeout_resp(p_ch);
        }
    }

    if (p_ch->interactive)
    {
        p_ch->send_deadline = 0;
    }
}

/**
 * @brief Clears the current line without changing CR/LF receive history
 */
static void m_cmd_reset_line(CMD_CHANNEL_T *p_ch)
{
    memset(p_ch->buf, 0, sizeof(p_ch->buf));
    p_ch->idx = 0;
    p_ch->tmout = 0;
    p_ch->overflow = false;
}

/**
 * @brief Pulls bytes and receive metadata from the dedicated command UART
 */
static uint16_t m_cmd_uart_recv(uint8_t *p_buf, uint8_t *p_rx_flags, uint16_t max_len)
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

    return uart_cmd_get_data(p_buf, p_rx_flags, data_len);
}

/**
 * @brief Pulls bytes from USB CDC; USB has no UART receive metadata
 */
static uint16_t m_cmd_usb_recv(uint8_t *p_buf, uint8_t *p_rx_flags, uint16_t max_len)
{
    uint16_t count;
    int      chr;

    count = 0;
    while (count < max_len)
    {
        chr = getchar_timeout_us(0);
        if (chr < 0)
        {
            break;
        }

        p_buf[count] = (uint8_t)chr;
        p_rx_flags[count] = 0;
        count++;
    }

    return count;
}

/**
 * @brief Sends raw response bytes to USB within per-response and per-handler budgets
 *
 * The Pico SDK's stdout and USB inactivity limits bound any individual write.
 * These absolute deadlines additionally prevent a slowly draining host or a
 * burst of help/error responses from starving the main-loop watchdog.
 */
static void m_cmd_usb_send(const uint8_t *p_data, uint16_t len)
{
    absolute_time_t response_deadline;
    uint16_t        idx;

    response_deadline = make_timeout_time_ms(CMD_USB_SEND_MS_C);
    idx = 0;

    while (idx < len)
    {
        uint16_t chunk_len;

        if (time_reached(response_deadline) ||
            ((cmd_usb_channel.send_deadline != 0) &&
             time_reached(cmd_usb_channel.send_deadline)))
        {
            break;
        }

        chunk_len = len - idx;
        if (chunk_len > CMD_USB_WRITE_CHUNK_C)
        {
            chunk_len = CMD_USB_WRITE_CHUNK_C;
        }

        (void)stdio_put_string((const char *)&p_data[idx], chunk_len, false, false);
        idx += chunk_len;
    }
}

/**
 * @brief Sends terminal help on the interactive USB channel
 */
static void m_cmd_send_help(CMD_CHANNEL_T *p_ch)
{
    p_ch->p_send((const uint8_t *)cmd_help_str, (uint16_t)strlen(cmd_help_str));
}

/**
 * @brief Sends the idle partial-command timeout response
 */
static void m_cmd_send_timeout_resp(CMD_CHANNEL_T *p_ch)
{
    static const uint8_t timeout_resp[] = "\r\n:" CMD_TMOUT_S "\r\n";
    p_ch->p_send(timeout_resp, (uint16_t)(sizeof(timeout_resp) - 1));
}

/**
 * @brief Tests whether the assembled line is exactly p_cmd plus one terminator
 */
static bool m_cmd_match_exact(const CMD_CHANNEL_T *p_ch, const char *p_cmd)
{
    size_t len;

    len = strlen(p_cmd);

    return (p_ch->idx == (len + 1U)) &&
           (memcmp(p_ch->buf, p_cmd, len) == 0) &&
           ((p_ch->buf[len] == CMD_CR_CHAR_C) ||
            (p_ch->buf[len] == CMD_LF_CHAR_C));
}

/**
 * @brief Splits an anchored line into exactly I:P or I:P:V fields
 */
static bool m_cmd_split_fields(const CMD_CHANNEL_T *p_ch,
                               char *p_inst,
                               char *p_param,
                               char *p_value,
                               bool *p_has_value)
{
    const uint8_t *p_sep_1;
    const uint8_t *p_sep_2;
    size_t         line_len;
    size_t         inst_len;
    size_t         param_len;
    size_t         value_len;

    memset(p_inst, 0, CMD_MAX_INST_LEN_C);
    memset(p_param, 0, CMD_MAX_PARAM_LEN_C);
    memset(p_value, 0, CMD_MAX_VAL_LEN_C);
    *p_has_value = false;

    if ((p_ch->idx < 2) ||
        ((p_ch->buf[p_ch->idx - 1] != CMD_CR_CHAR_C) &&
         (p_ch->buf[p_ch->idx - 1] != CMD_LF_CHAR_C)))
    {
        return false;
    }

    line_len = p_ch->idx - 1U;
    if (memchr(p_ch->buf, 0, line_len) != NULL)
    {
        return false;
    }

    p_sep_1 = memchr(p_ch->buf, CMD_SEPARATOR_CHAR_C, line_len);
    if (p_sep_1 == NULL)
    {
        return false;
    }

    p_sep_2 = memchr(p_sep_1 + 1,
                     CMD_SEPARATOR_CHAR_C,
                     line_len - (size_t)((p_sep_1 + 1) - p_ch->buf));

    if ((p_sep_2 != NULL) &&
        (memchr(p_sep_2 + 1,
                CMD_SEPARATOR_CHAR_C,
                line_len - (size_t)((p_sep_2 + 1) - p_ch->buf)) != NULL))
    {
        return false;
    }

    inst_len  = (size_t)(p_sep_1 - p_ch->buf);
    param_len = (p_sep_2 != NULL)
                    ? (size_t)(p_sep_2 - (p_sep_1 + 1))
                    : line_len - (size_t)((p_sep_1 + 1) - p_ch->buf);
    value_len = (p_sep_2 != NULL)
                    ? line_len - (size_t)((p_sep_2 + 1) - p_ch->buf)
                    : 0U;

    if ((inst_len == 0) || (inst_len >= CMD_MAX_INST_LEN_C) ||
        (param_len == 0) || (param_len >= CMD_MAX_PARAM_LEN_C) ||
        (value_len >= CMD_MAX_VAL_LEN_C))
    {
        return false;
    }

    memcpy(p_inst, p_ch->buf, inst_len);
    memcpy(p_param, p_sep_1 + 1, param_len);

    if (p_sep_2 != NULL)
    {
        memcpy(p_value, p_sep_2 + 1, value_len);
        *p_has_value = true;
    }

    return true;
}

/**
 * @brief Parses an entire nonempty decimal field without signs or wrapping
 */
static bool m_cmd_parse_u16(const char *p_str, uint16_t *p_value)
{
    uint32_t value;

    if (p_str[0] == '\0')
    {
        return false;
    }

    value = 0;
    for (size_t idx = 0; p_str[idx] != '\0'; idx++)
    {
        if ((p_str[idx] < '0') || (p_str[idx] > '9'))
        {
            return false;
        }

        value = (value * 10U) + (uint32_t)(p_str[idx] - '0');
        if (value > UINT16_MAX)
        {
            return false;
        }
    }

    *p_value = (uint16_t)value;
    return true;
}

/**
 * @brief Finds an exact instruction/parameter callback entry
 */
static const CMD_CTL_T *m_cmd_find_ctl(const char *p_inst, const char *p_param)
{
    for (const CMD_CTL_T *p_ctl = cmd_list; p_ctl->inst != NULL; p_ctl++)
    {
        if ((strcmp(p_ctl->inst, p_inst) == 0) &&
            (strcmp(p_ctl->param, p_param) == 0))
        {
            return p_ctl;
        }
    }

    return NULL;
}

/**
 * @brief Sends a GET reply of the form G:<param>:<value>
 */
static void m_cmd_send_get_resp(CMD_CHANNEL_T *p_ch, const char *p_param, const char *p_value)
{
    char resp[CMD_MAX_LEN_C];

    snprintf(resp, sizeof(resp), "%s:%s:%s\r\n",
             CMD_INST_GET_S, p_param, p_value);

    p_ch->p_send((const uint8_t *)resp, (uint16_t)strlen(resp));
}

/**
 * @brief Sends an error reply, plus help on the interactive USB channel
 *
 * p_value == NULL means the line had no value field. A non-NULL empty string
 * represents an explicitly empty third field.
 */
static void m_cmd_send_err_resp(CMD_CHANNEL_T *p_ch,
                                const char *p_inst,
                                const char *p_param,
                                const char *p_value)
{
    char resp[CMD_MAX_LEN_C];

    if (p_value != NULL)
    {
        snprintf(resp, sizeof(resp), "%s:%s:%s:%s\r\n",
                 p_inst, p_param, p_value, CMD_ERR_S);
    }
    else
    {
        snprintf(resp, sizeof(resp), "%s:%s:%s\r\n",
                 p_inst, p_param, CMD_ERR_S);
    }

    p_ch->p_send((const uint8_t *)resp, (uint16_t)strlen(resp));

    if (p_ch->interactive)
    {
        m_cmd_send_help(p_ch);
    }
}

/**
 * @brief Strictly parses one complete command and dispatches its callback
 */
static void m_cmd_process(CMD_CHANNEL_T *p_ch)
{
    const CMD_CTL_T *p_ctl;
    char             inst[CMD_MAX_INST_LEN_C];
    char             param[CMD_MAX_PARAM_LEN_C];
    char             value_str[CMD_MAX_VAL_LEN_C];
    char             num_str[12];
    bool             has_value;
    uint16_t         value;

    if (m_cmd_match_exact(p_ch, CMD_HELP_CMD_S))
    {
        if (p_ch->interactive)
        {
            m_cmd_send_help(p_ch);
        }
        else
        {
            m_cmd_send_err_resp(p_ch, "", "", NULL);                         /* Compact legacy reply; never send verbose help. */
        }
        return;
    }

    if (!m_cmd_split_fields(p_ch, inst, param, value_str, &has_value))
    {
        m_cmd_send_err_resp(p_ch, "", "", NULL);
        return;
    }

    if (strcmp(inst, CMD_INST_GET_S) == 0)
    {
        if (has_value)
        {
            m_cmd_send_err_resp(p_ch, inst, param, value_str);
            return;
        }

        if (strcmp(param, CMD_PARAM_SERIAL_ID_S) == 0)
        {
            m_cmd_send_get_resp(p_ch, param, serial_get_string());
            return;
        }

        if (strcmp(param, CMD_PARAM_HOURS_ID_S) == 0)
        {
            snprintf(num_str, sizeof(num_str), "%lu",
                     (unsigned long)hourmeter_get_on_hours());
            m_cmd_send_get_resp(p_ch, param, num_str);
            return;
        }

        if (strcmp(param, CMD_PARAM_ONSECS_ID_S) == 0)
        {
            snprintf(num_str, sizeof(num_str), "%lu",
                     (unsigned long)hourmeter_get_on_seconds());
            m_cmd_send_get_resp(p_ch, param, num_str);
            return;
        }

        p_ctl = m_cmd_find_ctl(inst, param);
        if (p_ctl == NULL)
        {
            m_cmd_send_err_resp(p_ch, inst, param, NULL);
            return;
        }

        snprintf(num_str, sizeof(num_str), "%d", p_ctl->p_callback(0));
        m_cmd_send_get_resp(p_ch, param, num_str);
        return;
    }

    if (strcmp(inst, CMD_INST_SET_S) == 0)
    {
        if (!has_value || !m_cmd_parse_u16(value_str, &value))
        {
            m_cmd_send_err_resp(p_ch, inst, param, has_value ? value_str : NULL);
            return;
        }

        p_ctl = m_cmd_find_ctl(inst, param);
        if (p_ctl == NULL)
        {
            m_cmd_send_err_resp(p_ch, inst, param, value_str);
            return;
        }

        snprintf(num_str, sizeof(num_str), "%u", (unsigned)value);
        if (p_ctl->p_callback(value) != 0)
        {
            char resp[CMD_MAX_LEN_C];

            snprintf(resp, sizeof(resp), "%s:%s:%s:%s\r\n",
                     inst, param, num_str, CMD_OK_S);
            p_ch->p_send((const uint8_t *)resp, (uint16_t)strlen(resp));
        }
        else
        {
            m_cmd_send_err_resp(p_ch, inst, param, num_str);
        }
        return;
    }

    m_cmd_send_err_resp(p_ch, inst, param, has_value ? value_str : NULL);
}


/*** END OF FILE ***/
