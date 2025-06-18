cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico_w)

include(pico_sdk_import.cmake)

project(test_project C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_compile_definitions(
	-DXOSC_KHZ=8000
	-DPLL_SYS_VCO_FREQ_KHZ=1500000
	-DPLL_SYS_POSTDIV1=6
	-DPLL_SYS_POSTDIV2=2
	-DPLL_USB_VCO_FREQ_KHZ=1200000
	-DPLL_USB_POSTDIV1=5
	-DPLL_USB_POSTDIV2=5
)

add_executable(app
	radar.c
	main.c
)

pico_enable_stdio_usb(app 1)
pico_enable_stdio_uart(app 0)

pico_add_extra_outputs(app)

target_link_libraries(app
	pico_stdlib
	hardware_pwm
	hardware_uart
	pico_multicore
)
