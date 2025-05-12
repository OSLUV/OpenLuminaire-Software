cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico)

include(pico_sdk_import.cmake)

project(test_project C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(app
	main.c
	st7789.c
	imu.c
	mag.c
	usbpd.c
	lamp.c
	buttons.c
	sense.c
	radar.c
	menu.c
	fan.c
	radio.c
)

pico_enable_stdio_usb(app 1)
pico_enable_stdio_uart(app 0)

pico_add_extra_outputs(app)

target_link_libraries(app
	pico_stdlib
	hardware_pwm
	hardware_spi
	hardware_i2c
	hardware_adc
	hardware_dma
	pico_multicore
)
