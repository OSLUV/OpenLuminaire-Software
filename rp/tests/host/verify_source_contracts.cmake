if(NOT DEFINED RP_SOURCE_DIR)
    message(FATAL_ERROR "RP_SOURCE_DIR is required")
endif()

file(READ "${RP_SOURCE_DIR}/persistance.c" PERSISTANCE_SOURCE)
if(PERSISTANCE_SOURCE MATCHES "printf[ \t\r\n]*\\(")
    message(FATAL_ERROR
        "persistance.c must not emit stdout text that can prefix command acknowledgements")
endif()

if(NOT PERSISTANCE_SOURCE MATCHES "uart_cmd_discard_rx_after_blocking[ \t\r\n]*\\(")
    message(FATAL_ERROR
        "persistence flash writes must discard UART RX made ambiguous while IRQs are masked")
endif()

file(READ "${RP_SOURCE_DIR}/CMakeLists.txt" RP_CMAKE_SOURCE)
string(REGEX MATCH
    "PICO_STDIO_USB_STDOUT_TIMEOUT_US=([0-9]+)"
    STDOUT_TIMEOUT_MATCH
    "${RP_CMAKE_SOURCE}")

if(NOT STDOUT_TIMEOUT_MATCH)
    message(FATAL_ERROR "PICO_STDIO_USB_STDOUT_TIMEOUT_US is not configured")
endif()

set(STDOUT_TIMEOUT_US "${CMAKE_MATCH_1}")
if(STDOUT_TIMEOUT_US LESS_EQUAL 1000)
    message(FATAL_ERROR
        "USB stdout timeout ${STDOUT_TIMEOUT_US} us is too short for multi-frame replies")
endif()

if(STDOUT_TIMEOUT_US GREATER_EQUAL 1500000)
    message(FATAL_ERROR
        "USB stdout timeout ${STDOUT_TIMEOUT_US} us is not below the watchdog interval")
endif()

string(REGEX MATCH
    "PICO_STDIO_DEADLOCK_TIMEOUT_MS=([0-9]+)"
    DEADLOCK_TIMEOUT_MATCH
    "${RP_CMAKE_SOURCE}")

if(NOT DEADLOCK_TIMEOUT_MATCH)
    message(FATAL_ERROR "PICO_STDIO_DEADLOCK_TIMEOUT_MS is not configured")
endif()

set(DEADLOCK_TIMEOUT_MS "${CMAKE_MATCH_1}")
if(DEADLOCK_TIMEOUT_MS GREATER_EQUAL 1500)
    message(FATAL_ERROR
        "stdio mutex timeout ${DEADLOCK_TIMEOUT_MS} ms is not below the watchdog interval")
endif()

message(STATUS
    "source output contracts passed (USB ${STDOUT_TIMEOUT_US} us; mutex ${DEADLOCK_TIMEOUT_MS} ms)")
