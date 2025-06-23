cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico)

include(pico_sdk_import.cmake)

project(test_project C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(app
	main.c
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
	safety_logic.c
	persistance.c
	display.c

	ui_main.c
	ui_loading.c
)

pico_enable_stdio_usb(app 1)
pico_enable_stdio_uart(app 0)

pico_add_extra_outputs(app)

set(LV_CONF_PATH "${CMAKE_CURRENT_LIST_DIR}/lv_conf.h")
target_include_directories(app PRIVATE ${CMAKE_CURRENT_LIST_DIR}/vendor/lvgl/src)
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/vendor/lvgl" lvgl)

target_link_libraries(app
	pico_stdlib
	hardware_pwm
	hardware_spi
	hardware_i2c
	hardware_adc
	hardware_dma
	hardware_flash

	lvgl
)
