cmake_minimum_required(VERSION 3.16)

project(openluminaire_rp_host_tests C)

enable_testing()

add_executable(command_tests
    host/fake_host.c
    host/test_commands.c
    ../d_uart_cmd.c
    ../m_cmd.c
)

target_include_directories(command_tests PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/host
    ${CMAKE_CURRENT_LIST_DIR}/host/include
    ${CMAKE_CURRENT_LIST_DIR}/..
)

target_compile_features(command_tests PRIVATE c_std_11)
target_compile_options(command_tests PRIVATE
    -O1
    -g3
    -fno-omit-frame-pointer
    -Wall
    -Wextra
    -Wpedantic
    -Werror
    -fsanitize=address,undefined
    -fno-sanitize-recover=all
)
target_link_options(command_tests PRIVATE
    -fsanitize=address,undefined
    -fno-sanitize-recover=all
)

add_test(NAME command_tests COMMAND command_tests)
set_tests_properties(command_tests PROPERTIES
    ENVIRONMENT "ASAN_OPTIONS=halt_on_error=1;UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1"
)

add_test(
    NAME source_output_contracts
    COMMAND ${CMAKE_COMMAND}
        -DRP_SOURCE_DIR=${CMAKE_CURRENT_LIST_DIR}/..
        -P ${CMAKE_CURRENT_LIST_DIR}/host/verify_source_contracts.cmake
)
