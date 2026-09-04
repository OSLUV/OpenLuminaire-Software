#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "d_uart_cmd.h"
#include "fake_host.h"
#include "m_cmd.h"

#define ARRAY_LEN_C(array) (sizeof(array) / sizeof((array)[0]))

static const uint8_t expected_help[] =
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

static uint16_t callback_lamp_state;
static uint16_t callback_dim_level;
static unsigned callback_set_lamp_calls;
static unsigned callback_get_lamp_calls;
static unsigned callback_set_dim_calls;
static unsigned callback_get_dim_calls;
static uint16_t callback_last_lamp_value;
static uint16_t callback_last_dim_value;

static unsigned assertion_failures;

#define CHECK(condition)                                                        \
    do                                                                          \
    {                                                                           \
        if (!(condition))                                                       \
        {                                                                       \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n",                     \
                    __FILE__, __LINE__, #condition);                            \
            assertion_failures++;                                               \
            return false;                                                       \
        }                                                                       \
    } while (0)

static bool bytes_equal(const uint8_t *actual,
                        size_t actual_len,
                        const void *expected,
                        size_t expected_len)
{
    return (actual_len == expected_len) &&
           ((expected_len == 0) || (memcmp(actual, expected, expected_len) == 0));
}

static const uint8_t *find_bytes(const uint8_t *haystack,
                                 size_t haystack_len,
                                 const void *needle,
                                 size_t needle_len)
{
    const uint8_t *needle_bytes;

    needle_bytes = needle;
    if (needle_len == 0)
    {
        return haystack;
    }

    if (needle_len > haystack_len)
    {
        return NULL;
    }

    for (size_t idx = 0; idx <= (haystack_len - needle_len); idx++)
    {
        if (memcmp(haystack + idx, needle_bytes, needle_len) == 0)
        {
            return haystack + idx;
        }
    }

    return NULL;
}

static size_t count_bytes(const uint8_t *haystack,
                          size_t haystack_len,
                          const void *needle,
                          size_t needle_len)
{
    size_t count;
    size_t offset;

    count = 0;
    offset = 0;
    while (offset <= haystack_len)
    {
        const uint8_t *match;

        match = find_bytes(haystack + offset,
                           haystack_len - offset,
                           needle,
                           needle_len);
        if (match == NULL)
        {
            break;
        }

        count++;
        offset = (size_t)(match - haystack) + needle_len;
    }

    return count;
}

static void reset_callbacks(void)
{
    callback_lamp_state = 1;
    callback_dim_level = 70;
    callback_set_lamp_calls = 0;
    callback_get_lamp_calls = 0;
    callback_set_dim_calls = 0;
    callback_get_dim_calls = 0;
    callback_last_lamp_value = UINT16_MAX;
    callback_last_dim_value = UINT16_MAX;
}

static void reset_system(void)
{
    fake_host_reset();
    reset_callbacks();
    m_cmd_init();
    fake_uart_tx_clear();
    fake_usb_tx_clear();
}

static void receive_uart(const void *data, size_t len)
{
    fake_uart_receive(data, len);
}

static void receive_usb(const void *data, size_t len)
{
    fake_usb_receive(data, len);
}

static void drain_command_uart(void)
{
    unsigned passes;

    passes = 0;
    while (uart_cmd_get_rcvd_data_len() > 0)
    {
        m_cmd_handler();
        passes++;
        if (passes > 256U)
        {
            fprintf(stderr, "UART did not drain after 256 handler passes\n");
            abort();
        }
    }
}

int16_t ui_main_lamp_set_stt(uint16_t req_state)
{
    callback_set_lamp_calls++;
    callback_last_lamp_value = req_state;

    if (req_state > 1U)
    {
        return 0;
    }

    callback_lamp_state = req_state;
    return 1;
}

int16_t ui_main_lamp_get_stt(uint16_t ignored)
{
    (void)ignored;
    callback_get_lamp_calls++;
    return (int16_t)callback_lamp_state;
}

int16_t ui_main_lamp_set_dim(uint16_t level)
{
    callback_set_dim_calls++;
    callback_last_dim_value = level;

    if ((level != 20U) && (level != 40U) &&
        (level != 70U) && (level != 100U))
    {
        return 0;
    }

    callback_dim_level = level;
    return 1;
}

int16_t ui_main_lamp_get_dim(uint16_t ignored)
{
    (void)ignored;
    callback_get_dim_calls++;
    return (int16_t)callback_dim_level;
}

const char *serial_get_string(void)
{
    return "ABC234567DEFG";
}

uint32_t hourmeter_get_on_hours(void)
{
    return 12U;
}

uint32_t hourmeter_get_on_seconds(void)
{
    return 43210U;
}

static bool test_documented_uart_commands(void)
{
    static const uint8_t set_lamp[] = "S:L:0\r";
    static const uint8_t get_lamp[] = "G:L\n";
    static const uint8_t set_dim[] = "S:D:20\r";
    static const uint8_t get_dim[] = "G:D\r";
    static const uint8_t get_serial[] = "G:N\r";
    static const uint8_t get_hours[] = "G:H\r";
    static const uint8_t get_seconds[] = "G:T\r";

    reset_system();

    receive_uart(set_lamp, sizeof(set_lamp) - 1U);
    m_cmd_handler();
    CHECK(callback_set_lamp_calls == 1U);
    CHECK(callback_last_lamp_value == 0U);
    CHECK(callback_lamp_state == 0U);
    CHECK(bytes_equal(fake_uart_tx_data(), fake_uart_tx_len(),
                      "S:L:0:OK\r\n", strlen("S:L:0:OK\r\n")));

    fake_uart_tx_clear();
    receive_uart(get_lamp, sizeof(get_lamp) - 1U);
    m_cmd_handler();
    CHECK(callback_get_lamp_calls == 1U);
    CHECK(bytes_equal(fake_uart_tx_data(), fake_uart_tx_len(),
                      "G:L:0\r\n", strlen("G:L:0\r\n")));

    fake_uart_tx_clear();
    receive_uart(set_dim, sizeof(set_dim) - 1U);
    drain_command_uart();
    CHECK(callback_set_dim_calls == 1U);
    CHECK(callback_last_dim_value == 20U);
    CHECK(bytes_equal(fake_uart_tx_data(), fake_uart_tx_len(),
                      "S:D:20:OK\r\n", strlen("S:D:20:OK\r\n")));

    fake_uart_tx_clear();
    receive_uart(get_dim, sizeof(get_dim) - 1U);
    m_cmd_handler();
    CHECK(callback_get_dim_calls == 1U);
    CHECK(bytes_equal(fake_uart_tx_data(), fake_uart_tx_len(),
                      "G:D:20\r\n", strlen("G:D:20\r\n")));

    fake_uart_tx_clear();
    receive_uart(get_serial, sizeof(get_serial) - 1U);
    m_cmd_handler();
    CHECK(bytes_equal(fake_uart_tx_data(), fake_uart_tx_len(),
                      "G:N:ABC234567DEFG\r\n", strlen("G:N:ABC234567DEFG\r\n")));

    fake_uart_tx_clear();
    receive_uart(get_hours, sizeof(get_hours) - 1U);
    m_cmd_handler();
    CHECK(bytes_equal(fake_uart_tx_data(), fake_uart_tx_len(),
                      "G:H:12\r\n", strlen("G:H:12\r\n")));

    fake_uart_tx_clear();
    receive_uart(get_seconds, sizeof(get_seconds) - 1U);
    m_cmd_handler();
    CHECK(bytes_equal(fake_uart_tx_data(), fake_uart_tx_len(),
                      "G:T:43210\r\n", strlen("G:T:43210\r\n")));

    return true;
}

static bool test_fragmentation_at_every_boundary(void)
{
    static const uint8_t command[] = "S:L:1\r";

    for (size_t split = 0; split <= (sizeof(command) - 1U); split++)
    {
        reset_system();

        receive_uart(command, split);
        m_cmd_handler();
        receive_uart(command + split, (sizeof(command) - 1U) - split);
        m_cmd_handler();

        CHECK(callback_set_lamp_calls == 1U);
        CHECK(callback_lamp_state == 1U);
        CHECK(bytes_equal(fake_uart_tx_data(), fake_uart_tx_len(),
                          "S:L:1:OK\r\n", strlen("S:L:1:OK\r\n")));
    }

    return true;
}

static bool assert_uart_line_is_rejected(const uint8_t *line, size_t len)
{
    static const uint8_t err[] = "ERR";
    static const uint8_t ok[] = "OK";
    static const uint8_t help_title[] = "OSLUV lamp serial commands";

    reset_system();
    receive_uart(line, len);
    drain_command_uart();

    CHECK(callback_set_lamp_calls == 0U);
    CHECK(callback_get_lamp_calls == 0U);
    CHECK(callback_set_dim_calls == 0U);
    CHECK(callback_get_dim_calls == 0U);
    CHECK(callback_lamp_state == 1U);
    CHECK(callback_dim_level == 70U);
    CHECK(find_bytes(fake_uart_tx_data(), fake_uart_tx_len(), err, sizeof(err) - 1U) != NULL);
    CHECK(find_bytes(fake_uart_tx_data(), fake_uart_tx_len(), ok, sizeof(ok) - 1U) == NULL);
    CHECK(find_bytes(fake_uart_tx_data(), fake_uart_tx_len(),
                     help_title, sizeof(help_title) - 1U) == NULL);

    return true;
}

static bool test_strict_parser_rejects_malformed_lines(void)
{
    static const char *const invalid_lines[] = {
        "XS:L:1\r",
        "G:L:S:L:1\r",
        "S:L\r",
        "S:L:\r",
        "S:L:not-a-number\r",
        "S:L:1junk\r",
        "S:L:-1\r",
        "S:L:+1\r",
        "S:L: 1\r",
        "S:L:65536\r",
        "S:L:65537\r",
        "S:L:999999999999999\r",
        "G:L:1\r",
        "G:N:1\r",
        "G:H:1\r",
        "G:T:1\r",
        "S:L:1:tail\r",
        "S::1\r",
        ":L:1\r",
        "S:X:1\r"
    };
    static const uint8_t embedded_nul[] = {'S', ':', 'L', ':', '1', 0, 'X', '\r'};

    for (size_t idx = 0; idx < ARRAY_LEN_C(invalid_lines); idx++)
    {
        CHECK(assert_uart_line_is_rejected((const uint8_t *)invalid_lines[idx],
                                           strlen(invalid_lines[idx])));
    }

    CHECK(assert_uart_line_is_rejected(embedded_nul, sizeof(embedded_nul)));
    return true;
}

static bool test_semantic_callback_rejection(void)
{
    static const uint8_t invalid_state[] = "S:L:2\r";
    static const uint8_t invalid_dim[] = "S:D:21\r";

    reset_system();
    receive_uart(invalid_state, sizeof(invalid_state) - 1U);
    m_cmd_handler();
    CHECK(callback_set_lamp_calls == 1U);
    CHECK(callback_lamp_state == 1U);
    CHECK(bytes_equal(fake_uart_tx_data(), fake_uart_tx_len(),
                      "S:L:2:ERR\r\n", strlen("S:L:2:ERR\r\n")));

    fake_uart_tx_clear();
    receive_uart(invalid_dim, sizeof(invalid_dim) - 1U);
    drain_command_uart();
    CHECK(callback_set_dim_calls == 1U);
    CHECK(callback_dim_level == 70U);
    CHECK(bytes_equal(fake_uart_tx_data(), fake_uart_tx_len(),
                      "S:D:21:ERR\r\n", strlen("S:D:21:ERR\r\n")));

    return true;
}

static bool test_oversized_line_discards_embedded_command(void)
{
    uint8_t oversized[160];
    static const uint8_t valid[] = "S:L:0\r";
    static const uint8_t one_err[] = "\r\n:ERR\r\n";

    memset(oversized, 'X', sizeof(oversized));
    memcpy(oversized + 80U, "S:L:0", strlen("S:L:0"));
    oversized[sizeof(oversized) - 1U] = '\r';

    reset_system();
    receive_uart(oversized, sizeof(oversized));
    drain_command_uart();

    CHECK(callback_set_lamp_calls == 0U);
    CHECK(callback_lamp_state == 1U);
    CHECK(count_bytes(fake_uart_tx_data(), fake_uart_tx_len(),
                      one_err, sizeof(one_err) - 1U) == 1U);

    fake_uart_tx_clear();
    receive_uart(valid, sizeof(valid) - 1U);
    m_cmd_handler();
    CHECK(callback_set_lamp_calls == 1U);
    CHECK(callback_lamp_state == 0U);
    CHECK(bytes_equal(fake_uart_tx_data(), fake_uart_tx_len(),
                      "S:L:0:OK\r\n", strlen("S:L:0:OK\r\n")));

    return true;
}

static bool test_uart_help_is_compact_and_watchdog_safe(void)
{
    static const uint8_t requests[] =
        "--help\r--help\r--help\r--help\r";
    static const uint8_t expected[] =
        "::ERR\r\n::ERR\r\n::ERR\r\n::ERR\r\n";
    uint64_t wire_time_us;

    reset_system();
    receive_uart(requests, sizeof(requests) - 1U);
    drain_command_uart();

    CHECK(bytes_equal(fake_uart_tx_data(), fake_uart_tx_len(),
                      expected, sizeof(expected) - 1U));
    CHECK(find_bytes(fake_uart_tx_data(), fake_uart_tx_len(),
                     expected_help, sizeof(expected_help) - 1U) == NULL);

    wire_time_us = ((uint64_t)fake_uart_tx_len() * 10U * 1000000U + 9599U) / 9600U;
    CHECK(wire_time_us < 1500000U);
    return true;
}

static bool test_receive_budget_limits_set_callbacks_per_pass(void)
{
    static const uint8_t commands[] = "S:L:0\rS:L:1\rS:L:0\r";
    static const uint8_t staged_fragment[] = "S:L:0";
    static const uint8_t staged_tail[] = "\rS:L:1\r";

    reset_system();
    receive_uart(commands, sizeof(commands) - 1U);

    m_cmd_handler();
    CHECK(callback_set_lamp_calls == 1U);
    m_cmd_handler();
    CHECK(callback_set_lamp_calls == 2U);
    m_cmd_handler();
    CHECK(callback_set_lamp_calls == 3U);
    CHECK(callback_lamp_state == 0U);
    CHECK(uart_cmd_get_rcvd_data_len() == 0U);

    reset_system();
    receive_uart(staged_fragment, sizeof(staged_fragment) - 1U);
    m_cmd_handler();
    CHECK(callback_set_lamp_calls == 0U);

    receive_uart(staged_tail, sizeof(staged_tail) - 1U);
    m_cmd_handler();
    CHECK(callback_set_lamp_calls == 1U);                                      /* Remaining five bytes cannot complete SET #2. */
    m_cmd_handler();
    CHECK(callback_set_lamp_calls == 2U);
    return true;
}

static bool test_usb_help_is_complete_bulk_write(void)
{
    static const uint8_t request[] = "--help\r";
    const uint8_t *help_at;
    size_t help_offset;
    size_t covered;

    reset_system();
    receive_usb(request, sizeof(request) - 1U);
    m_cmd_handler();
    m_cmd_handler();

    help_at = find_bytes(fake_usb_tx_data(), fake_usb_tx_len(),
                         expected_help, sizeof(expected_help) - 1U);
    CHECK(help_at != NULL);
    help_offset = (size_t)(help_at - fake_usb_tx_data());

    covered = 0;
    for (size_t idx = 0; idx < fake_stdio_call_count(); idx++)
    {
        const fake_stdio_call_t *call;

        call = &fake_stdio_calls()[idx];
        if (call->offset == (help_offset + covered))
        {
            CHECK(call->len > 0U);
            CHECK(call->len <= 64U);
            CHECK(!call->newline);
            CHECK(!call->cr_translation);
            covered += call->len;
            if (covered == (sizeof(expected_help) - 1U))
            {
                break;
            }
            CHECK(covered < (sizeof(expected_help) - 1U));
        }
    }

    CHECK(covered == (sizeof(expected_help) - 1U));
    CHECK(fake_stdio_flush_count() == 0U);
    return true;
}

static bool test_interactive_echo_editing_and_channel_isolation(void)
{
    static const uint8_t uart_fragment[] = "S:L:";
    static const uint8_t usb_get[] = "G:L\r";
    static const uint8_t uart_tail[] = "1\r";
    static const uint8_t edit[] = {'S', ':', 'L', ':', '0', '\b', '1', '\r'};

    reset_system();
    receive_uart(uart_fragment, sizeof(uart_fragment) - 1U);
    m_cmd_handler();

    receive_usb(usb_get, sizeof(usb_get) - 1U);
    m_cmd_handler();
    CHECK(callback_get_lamp_calls == 1U);
    CHECK(callback_set_lamp_calls == 0U);
    CHECK(bytes_equal(fake_usb_tx_data(), fake_usb_tx_len(),
                      "G:L\r\nG:L:1\r\n", strlen("G:L\r\nG:L:1\r\n")));

    receive_uart(uart_tail, sizeof(uart_tail) - 1U);
    m_cmd_handler();
    CHECK(callback_set_lamp_calls == 1U);
    CHECK(callback_lamp_state == 1U);

    reset_system();
    receive_usb(edit, sizeof(edit));
    m_cmd_handler();
    m_cmd_handler();
    CHECK(callback_set_lamp_calls == 1U);
    CHECK(callback_lamp_state == 1U);
    CHECK(find_bytes(fake_usb_tx_data(), fake_usb_tx_len(),
                     "\b \b", strlen("\b \b")) != NULL);
    CHECK(find_bytes(fake_usb_tx_data(), fake_usb_tx_len(),
                     "S:L:1:OK\r\n", strlen("S:L:1:OK\r\n")) != NULL);

    return true;
}

static bool test_idle_timeouts(void)
{
    static const uint8_t fragment[] = "S:L:";
    static const uint8_t timeout[] = "\r\n:TOUT\r\n";

    reset_system();
    receive_uart(fragment, sizeof(fragment) - 1U);
    m_cmd_handler();
    CHECK(fake_uart_tx_len() == 0U);

    fake_time_advance_us(50001U);
    m_cmd_handler();
    CHECK(bytes_equal(fake_uart_tx_data(), fake_uart_tx_len(),
                      timeout, sizeof(timeout) - 1U));
    CHECK(callback_set_lamp_calls == 0U);

    reset_system();
    receive_usb(fragment, sizeof(fragment) - 1U);
    m_cmd_handler();
    fake_usb_tx_clear();

    fake_time_advance_us(2000000U);
    m_cmd_handler();
    CHECK(bytes_equal(fake_usb_tx_data(), fake_usb_tx_len(),
                      timeout, sizeof(timeout) - 1U));
    CHECK(callback_set_lamp_calls == 0U);
    return true;
}

static bool test_isr_gap_splits_stale_fragment(void)
{
    static const uint8_t fragment[] = "S:L:";
    static const uint8_t getter[] = "G:L\r";
    static const uint8_t expected[] = "\r\n:TOUT\r\nG:L:1\r\n";

    reset_system();
    receive_uart(fragment, sizeof(fragment) - 1U);
    m_cmd_handler();

    fake_time_advance_us(50001U);
    receive_uart(getter, sizeof(getter) - 1U);
    fake_time_advance_us(250000U);                                             /* Main loop wakes much later. */
    m_cmd_handler();

    CHECK(callback_set_lamp_calls == 0U);
    CHECK(callback_get_lamp_calls == 1U);
    CHECK(callback_lamp_state == 1U);
    CHECK(bytes_equal(fake_uart_tx_data(), fake_uart_tx_len(),
                      expected, sizeof(expected) - 1U));
    return true;
}

static bool test_exact_gap_boundary_uses_arrival_time(void)
{
    static const uint8_t fragment[] = "S:L:";
    static const uint8_t tail[] = "1\r";

    reset_system();
    receive_uart(fragment, sizeof(fragment) - 1U);
    m_cmd_handler();

    fake_time_advance_us(50000U);                                              /* Not greater than the 50 ms contract. */
    receive_uart(tail, sizeof(tail) - 1U);
    fake_time_advance_us(500000U);                                             /* Handler latency must not invent a receive gap. */
    m_cmd_handler();

    CHECK(callback_set_lamp_calls == 1U);
    CHECK(callback_lamp_state == 1U);
    CHECK(bytes_equal(fake_uart_tx_data(), fake_uart_tx_len(),
                      "S:L:1:OK\r\n", strlen("S:L:1:OK\r\n")));
    return true;
}

static bool test_multiple_isr_gaps_remain_ordered(void)
{
    static const uint8_t old_fragment[] = "S:L:";
    static const uint8_t next_fragment[] = "G:";
    static const uint8_t getter[] = "G:L\r";
    static const uint8_t expected[] =
        "\r\n:TOUT\r\n\r\n:TOUT\r\nG:L:1\r\n";

    reset_system();
    receive_uart(old_fragment, sizeof(old_fragment) - 1U);
    m_cmd_handler();

    fake_time_advance_us(60000U);
    receive_uart(next_fragment, sizeof(next_fragment) - 1U);
    fake_time_advance_us(60000U);
    receive_uart(getter, sizeof(getter) - 1U);
    fake_time_advance_us(250000U);
    m_cmd_handler();

    CHECK(callback_set_lamp_calls == 0U);
    CHECK(callback_get_lamp_calls == 1U);
    CHECK(bytes_equal(fake_uart_tx_data(), fake_uart_tx_len(),
                      expected, sizeof(expected) - 1U));
    return true;
}

static bool test_crlf_pair_and_aging(void)
{
    static const uint8_t crlf[] = "\r\n";
    static const uint8_t cr[] = "\r";
    static const uint8_t lf[] = "\n";
    static const uint8_t uart_bare_err[] = "::ERR\r\n";
    static const uint8_t help_title[] = "OSLUV lamp serial commands";

    reset_system();
    receive_uart(crlf, sizeof(crlf) - 1U);
    m_cmd_handler();
    CHECK(bytes_equal(fake_uart_tx_data(), fake_uart_tx_len(),
                      uart_bare_err, sizeof(uart_bare_err) - 1U));

    reset_system();
    receive_uart(cr, sizeof(cr) - 1U);
    m_cmd_handler();
    fake_time_advance_us(101000U);
    receive_uart(lf, sizeof(lf) - 1U);
    m_cmd_handler();
    CHECK(count_bytes(fake_uart_tx_data(), fake_uart_tx_len(),
                      uart_bare_err, sizeof(uart_bare_err) - 1U) == 2U);

    reset_system();
    receive_usb(crlf, sizeof(crlf) - 1U);
    m_cmd_handler();
    CHECK(count_bytes(fake_usb_tx_data(), fake_usb_tx_len(),
                      help_title, sizeof(help_title) - 1U) == 1U);

    reset_system();
    receive_usb(cr, sizeof(cr) - 1U);
    m_cmd_handler();
    fake_time_advance_us(101000U);
    receive_usb(lf, sizeof(lf) - 1U);
    m_cmd_handler();
    CHECK(count_bytes(fake_usb_tx_data(), fake_usb_tx_len(),
                      help_title, sizeof(help_title) - 1U) == 2U);
    return true;
}

static bool test_ring_capacity_and_data_loss_flag(void)
{
    uint8_t input[1024];
    uint8_t output[1024];
    uint8_t flags[1024];
    uint16_t count;

    for (size_t idx = 0; idx < sizeof(input); idx++)
    {
        input[idx] = (uint8_t)(idx % 251U);
    }

    fake_host_reset();
    uart_cmd_init();
    fake_uart_receive(input, sizeof(input));

    CHECK(uart_cmd_get_rcvd_data_len() == 1023U);
    count = uart_cmd_get_data(output, flags, (uint16_t)ARRAY_LEN_C(output));
    CHECK(count == 1023U);
    CHECK(memcmp(output, input + 1U, 1023U) == 0);
    CHECK((flags[0] & UART_CMD_RX_FLAG_DATA_LOSS_C) != 0U);
    CHECK(uart_cmd_get_rcvd_data_len() == 0U);
    return true;
}

static bool test_ring_index_wrap_preserves_order(void)
{
    uint8_t first[800];
    uint8_t second[900];
    uint8_t discarded[700];
    uint8_t discarded_flags[700];
    uint8_t output[1000];
    uint8_t flags[1000];
    uint16_t count;

    for (size_t idx = 0; idx < sizeof(first); idx++)
    {
        first[idx] = (uint8_t)((idx * 3U) % 251U);
    }
    for (size_t idx = 0; idx < sizeof(second); idx++)
    {
        second[idx] = (uint8_t)((idx * 7U + 1U) % 251U);
    }

    fake_host_reset();
    uart_cmd_init();
    fake_uart_receive(first, sizeof(first));
    count = uart_cmd_get_data(discarded, discarded_flags,
                              (uint16_t)ARRAY_LEN_C(discarded));
    CHECK(count == ARRAY_LEN_C(discarded));

    fake_uart_receive(second, sizeof(second));
    CHECK(uart_cmd_get_rcvd_data_len() == 1000U);
    count = uart_cmd_get_data(output, flags, (uint16_t)ARRAY_LEN_C(output));
    CHECK(count == ARRAY_LEN_C(output));
    CHECK(memcmp(output, first + 700U, 100U) == 0);
    CHECK(memcmp(output + 100U, second, sizeof(second)) == 0);
    CHECK(uart_cmd_get_rcvd_data_len() == 0U);
    return true;
}

static bool test_ring_records_exact_and_over_limit_gaps(void)
{
    uint8_t byte;
    uint8_t flag;

    fake_host_reset();
    uart_cmd_init();

    fake_time_set_us(100U);
    fake_uart_receive((const uint8_t *)"A", 1U);
    CHECK(uart_cmd_get_data(&byte, &flag, 1U) == 1U);
    CHECK(byte == 'A');
    CHECK(flag == 0U);

    fake_time_set_us(50100U);
    fake_uart_receive((const uint8_t *)"B", 1U);
    CHECK(uart_cmd_get_data(&byte, &flag, 1U) == 1U);
    CHECK(byte == 'B');
    CHECK((flag & UART_CMD_RX_FLAG_GAP_BEFORE_C) == 0U);

    fake_time_set_us(100101U);
    fake_uart_receive((const uint8_t *)"C", 1U);
    CHECK(uart_cmd_get_data(&byte, &flag, 1U) == 1U);
    CHECK(byte == 'C');
    CHECK((flag & UART_CMD_RX_FLAG_GAP_BEFORE_C) != 0U);
    return true;
}

static bool test_ring_gap_uses_full_64_bit_clock(void)
{
    uint8_t byte;
    uint8_t flag;
    uint64_t start_us;

    fake_host_reset();
    uart_cmd_init();

    start_us = 7U;
    fake_time_set_us(start_us);
    fake_uart_receive((const uint8_t *)"A", 1U);
    CHECK(uart_cmd_get_data(&byte, &flag, 1U) == 1U);
    CHECK(flag == 0U);

    /* A 32-bit timestamp truncates this delta to exactly 50,000 us and misses
     * the gap. The production ISR must retain the full time_us_64 value. */
    fake_time_set_us(start_us + (uint64_t)UINT32_MAX + 50001U);
    fake_uart_receive((const uint8_t *)"B", 1U);
    CHECK(uart_cmd_get_data(&byte, &flag, 1U) == 1U);
    CHECK(byte == 'B');
    CHECK((flag & UART_CMD_RX_FLAG_GAP_BEFORE_C) != 0U);
    return true;
}

static bool test_ring_data_loss_suffix_cannot_actuate(void)
{
    uint8_t input[1024];
    static const uint8_t valid[] = "S:L:0\r";

    memset(input, 'Q', sizeof(input));
    input[0] = 'X';                                                            /* This byte is overwritten. */
    memcpy(input + 1U, "S:L:0\r", strlen("S:L:0\r"));                     /* Retained tail looks exactly valid. */
    input[sizeof(input) - 1U] = '\r';

    reset_system();
    receive_uart(input, sizeof(input));
    drain_command_uart();

    CHECK(callback_set_lamp_calls == 0U);
    CHECK(callback_lamp_state == 1U);

    fake_uart_tx_clear();
    receive_uart(valid, sizeof(valid) - 1U);
    m_cmd_handler();
    CHECK(callback_set_lamp_calls == 1U);
    CHECK(callback_lamp_state == 0U);
    CHECK(bytes_equal(fake_uart_tx_data(), fake_uart_tx_len(),
                      "S:L:0:OK\r\n", strlen("S:L:0:OK\r\n")));
    return true;
}

static bool test_combined_gap_and_data_loss_resynchronizes(void)
{
    uint8_t retained[1023];
    static const uint8_t error[] = "\r\n:ERR\r\n";
    static const uint8_t ok[] = "S:L:0:OK\r\n";

    memset(retained, 'Q', sizeof(retained));
    memcpy(retained, "S:L:0\r", strlen("S:L:0\r"));
    retained[sizeof(retained) - 1U] = '\r';

    reset_system();
    receive_uart("X", 1U);                                                     /* Will be overwritten before the main loop runs. */
    fake_time_advance_us(50001U);                                               /* Establishes a trustworthy new line boundary. */
    receive_uart(retained, sizeof(retained));
    drain_command_uart();

    CHECK(callback_set_lamp_calls == 1U);
    CHECK(callback_last_lamp_value == 0U);
    CHECK(callback_lamp_state == 0U);
    CHECK(find_bytes(fake_uart_tx_data(), fake_uart_tx_len(),
                     error, sizeof(error) - 1U) != NULL);
    CHECK(find_bytes(fake_uart_tx_data(), fake_uart_tx_len(),
                     ok, sizeof(ok) - 1U) != NULL);
    return true;
}

static bool test_flash_blackout_rx_is_discarded(void)
{
    static const uint8_t fragment[] = "S:L:";
    static const uint8_t tail[] = "1\r";
    static const uint8_t untrusted_line[] = "S:L:1\r";
    static const uint8_t valid[] = "S:L:0\r";

    reset_system();
    /* Model a stale fragment and its tail arriving more than 50 ms apart while
     * flash code masks IRQs. The pending ISR stamps the whole FIFO at once. */
    fake_uart_receive_deferred(fragment, sizeof(fragment) - 1U);
    fake_time_advance_us(60000U);
    fake_uart_receive_deferred(tail, sizeof(tail) - 1U);
    fake_uart_service_irq();                                                   /* Pending IRQ runs as flash restores interrupts. */
    CHECK(uart_cmd_get_rcvd_data_len() ==
          ((sizeof(fragment) - 1U) + (sizeof(tail) - 1U)));
    uart_cmd_discard_rx_after_blocking();
    CHECK(uart_cmd_get_rcvd_data_len() == 0U);

    /* A no-gap line after the purge is untrusted and discarded through its
     * terminator, so a transmission that straddled the purge cannot actuate. */
    receive_uart(untrusted_line, sizeof(untrusted_line) - 1U);
    m_cmd_handler();
    CHECK(callback_set_lamp_calls == 0U);
    CHECK(callback_get_lamp_calls == 0U);
    CHECK(callback_lamp_state == 1U);

    fake_uart_tx_clear();
    fake_time_advance_us(50001U);
    receive_uart(valid, sizeof(valid) - 1U);
    m_cmd_handler();
    CHECK(callback_set_lamp_calls == 1U);
    CHECK(callback_lamp_state == 0U);
    CHECK(bytes_equal(fake_uart_tx_data(), fake_uart_tx_len(),
                      "S:L:0:OK\r\n", strlen("S:L:0:OK\r\n")));
    return true;
}

static uint32_t random_next(uint32_t *state)
{
    uint32_t value;

    value = *state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    *state = value;
    return value;
}

static bool exact_setter_grammar(const uint8_t *line,
                                 size_t len,
                                 bool *is_lamp,
                                 uint16_t *parsed_value)
{
    uint32_t value;

    if ((len < 6U) || (line[0] != 'S') || (line[1] != ':') ||
        ((line[2] != 'L') && (line[2] != 'D')) || (line[3] != ':') ||
        (line[len - 1U] != '\r'))
    {
        return false;
    }

    value = 0;
    for (size_t idx = 4U; idx < (len - 1U); idx++)
    {
        if ((line[idx] < '0') || (line[idx] > '9'))
        {
            return false;
        }

        value = (value * 10U) + (uint32_t)(line[idx] - '0');
        if (value > UINT16_MAX)
        {
            return false;
        }
    }

    *is_lamp = line[2] == 'L';
    *parsed_value = (uint16_t)value;
    return true;
}

static bool test_deterministic_random_byte_property(void)
{
    uint32_t random_state;

    random_state = 0x6f736c75U;
    for (unsigned iteration = 0; iteration < 4096U; iteration++)
    {
        uint8_t line[81];
        size_t line_len;
        bool expected_setter;
        bool expected_lamp;
        uint16_t expected_value;
        unsigned setter_calls;

        line_len = 1U + (random_next(&random_state) % 80U);
        for (size_t idx = 0; idx < (line_len - 1U); idx++)
        {
            uint8_t byte;

            byte = (uint8_t)random_next(&random_state);
            if ((byte == '\r') || (byte == '\n'))
            {
                byte = 0xffU;                                                  /* Keep exactly one line terminator for the oracle. */
            }
            line[idx] = byte;
        }
        line[line_len - 1U] = '\r';

        /* Periodically splice a syntactically exact setter with a random
         * numeric value into the byte stream so both sides of the property
         * are exercised, including callback-level semantic rejection. */
        if ((iteration % 64U) == 0U)
        {
            unsigned generated_value;
            int written;

            generated_value = random_next(&random_state) % 70000U;
            written = snprintf((char *)line, sizeof(line),
                               (iteration & 64U) ? "S:L:%u\r" : "S:D:%u\r",
                               generated_value);
            CHECK(written > 0);
            CHECK((size_t)written < sizeof(line));
            line_len = (size_t)written;
        }

        expected_lamp = false;
        expected_value = 0;
        expected_setter = exact_setter_grammar(line, line_len,
                                               &expected_lamp, &expected_value);

        reset_system();
        receive_uart(line, line_len);
        drain_command_uart();

        setter_calls = callback_set_lamp_calls + callback_set_dim_calls;
        CHECK(setter_calls == (expected_setter ? 1U : 0U));
        if (!expected_setter)
        {
            CHECK(callback_lamp_state == 1U);
            CHECK(callback_dim_level == 70U);
        }
        else if (expected_lamp)
        {
            CHECK(callback_set_lamp_calls == 1U);
            CHECK(callback_last_lamp_value == expected_value);
            CHECK(callback_set_dim_calls == 0U);
            CHECK(callback_lamp_state == ((expected_value <= 1U) ? expected_value : 1U));
        }
        else
        {
            bool valid_dim;

            valid_dim = (expected_value == 20U) || (expected_value == 40U) ||
                        (expected_value == 70U) || (expected_value == 100U);
            CHECK(callback_set_dim_calls == 1U);
            CHECK(callback_last_dim_value == expected_value);
            CHECK(callback_set_lamp_calls == 0U);
            CHECK(callback_dim_level == (valid_dim ? expected_value : 70U));
        }
    }

    return true;
}

typedef bool (*test_fn_t)(void);

typedef struct test_case {
    const char *name;
    test_fn_t   fn;
} test_case_t;

int main(void)
{
    static const test_case_t tests[] = {
        {"documented UART commands", test_documented_uart_commands},
        {"fragmentation at every boundary", test_fragmentation_at_every_boundary},
        {"strict malformed-line rejection", test_strict_parser_rejects_malformed_lines},
        {"semantic callback rejection", test_semantic_callback_rejection},
        {"oversized line discard", test_oversized_line_discards_embedded_command},
        {"UART help watchdog bound", test_uart_help_is_compact_and_watchdog_safe},
        {"one SET callback per handler pass", test_receive_budget_limits_set_callbacks_per_pass},
        {"USB help complete bulk write", test_usb_help_is_complete_bulk_write},
        {"interactive editing and channel isolation", test_interactive_echo_editing_and_channel_isolation},
        {"idle timeouts", test_idle_timeouts},
        {"ISR gap splits stale fragment", test_isr_gap_splits_stale_fragment},
        {"exact gap uses arrival time", test_exact_gap_boundary_uses_arrival_time},
        {"multiple queued ISR gaps", test_multiple_isr_gaps_remain_ordered},
        {"CRLF adjacency aging", test_crlf_pair_and_aging},
        {"ring capacity/data-loss marker", test_ring_capacity_and_data_loss_flag},
        {"ring index wrap", test_ring_index_wrap_preserves_order},
        {"ring gap threshold", test_ring_records_exact_and_over_limit_gaps},
        {"ring 64-bit gap clock", test_ring_gap_uses_full_64_bit_clock},
        {"data-loss suffix fail closed", test_ring_data_loss_suffix_cannot_actuate},
        {"combined gap/loss resync", test_combined_gap_and_data_loss_resynchronizes},
        {"flash blackout RX discard", test_flash_blackout_rx_is_discarded},
        {"deterministic random-byte property", test_deterministic_random_byte_property}
    };
    unsigned failed_tests;

    failed_tests = 0;
    for (size_t idx = 0; idx < ARRAY_LEN_C(tests); idx++)
    {
        unsigned before;

        before = assertion_failures;
        if (!tests[idx].fn() || (assertion_failures != before))
        {
            fprintf(stderr, "not ok - %s\n", tests[idx].name);
            failed_tests++;
        }
        else
        {
            printf("ok - %s\n", tests[idx].name);
        }
    }

    if (failed_tests != 0U)
    {
        fprintf(stderr, "%u of %zu host command tests failed\n",
                failed_tests, ARRAY_LEN_C(tests));
        return EXIT_FAILURE;
    }

    printf("all %zu host command tests passed\n", ARRAY_LEN_C(tests));
    return EXIT_SUCCESS;
}
