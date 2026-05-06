cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico)

include(pico_sdk_import.cmake)

project(test_project C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# --- Host tool: convert splash images to C source (RGB565) ----------
# All image formats supported by stb_image:
set(STB_IMAGE_EXTENSIONS png jpg jpeg bmp tga psd gif hdr pic pnm pgm ppm)
set(PNG_TO_CSOURCE ${CMAKE_CURRENT_BINARY_DIR}/png_to_csource)

add_custom_command(
	OUTPUT  ${PNG_TO_CSOURCE}
	COMMAND cc -o ${PNG_TO_CSOURCE}
		${CMAKE_CURRENT_SOURCE_DIR}/png_to_csource.c -lm
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/png_to_csource.c
		${CMAKE_CURRENT_SOURCE_DIR}/stb_image.h
		${CMAKE_CURRENT_SOURCE_DIR}/stb_image_resize2.h
	COMMENT "Building png_to_csource (host tool)"
)

set(SPLASH_IMAGES basic default dimmable)
set(SPLASH_C_FILES)
foreach(img ${SPLASH_IMAGES})
	# Find the source image by trying each supported extension
	set(SPLASH_SRC_${img})
	foreach(ext ${STB_IMAGE_EXTENSIONS})
		if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/image_${img}.${ext})
			set(SPLASH_SRC_${img} ${CMAKE_CURRENT_SOURCE_DIR}/image_${img}.${ext})
			break()
		endif()
	endforeach()
	if(NOT SPLASH_SRC_${img})
		message(FATAL_ERROR "No image file found for image_${img} (tried: ${STB_IMAGE_EXTENSIONS})")
	endif()

	add_custom_command(
		OUTPUT  ${CMAKE_CURRENT_SOURCE_DIR}/image_${img}.c
		COMMAND ${PNG_TO_CSOURCE}
			${SPLASH_SRC_${img}}
			-o ${CMAKE_CURRENT_SOURCE_DIR}/image_${img}.c
		DEPENDS ${PNG_TO_CSOURCE}
			${SPLASH_SRC_${img}}
		COMMENT "Converting ${SPLASH_SRC_${img}} -> image_${img}.c"
	)
	list(APPEND SPLASH_C_FILES ${CMAKE_CURRENT_SOURCE_DIR}/image_${img}.c)
endforeach()

add_custom_target(splash_images DEPENDS ${SPLASH_C_FILES})

# --------------------------------------------------------------------

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	add_executable(app
		main.c
		Modules/mod_ctrl_mgr.c
		Modules/mod_pow_mgr.c
		Modules/mod_system.c
		Modules/mod_ui_mgr.c
		Modules/mod_ui_scrn_debug.c
		Modules/mod_ui_scrn_main.c
		Modules/mod_ui_scrn_loading.c
		Drivers/drv_accelerometer.c
		Drivers/drv_adc_volt.c
		Drivers/drv_buttons.c
		Drivers/drv_config.c
		Drivers/drv_debug.c
		Drivers/drv_display.c
		Drivers/drv_i2c.c
		Drivers/drv_fan.c
		Drivers/drv_lamp.c
		Drivers/drv_magnetometer.c
		Drivers/drv_radar.c
		Drivers/drv_usb_pd.c
		safety_logic.c
		splash_img.c
	)

	add_dependencies(app splash_images)

    target_compile_options(app PRIVATE -O0 -g3)
    target_compile_definitions(app PRIVATE DEBUG_BUILD)

	target_compile_definitions(app PRIVATE
        PICO_DEFAULT_UART=1
        PICO_DEFAULT_UART_TX_PIN=8
        PICO_DEFAULT_UART_RX_PIN=9)

	pico_enable_stdio_usb(app 0)
	pico_enable_stdio_uart(app 1)
else()
	add_executable(app
		main.c
		Modules/mod_ctrl_mgr.c
		Modules/mod_comm_mgr.c
		Modules/mod_pow_mgr.c
		Modules/mod_ser_cmd.c
		Modules/mod_system.c
		Modules/mod_ui_mgr.c
		Modules/mod_ui_scrn_debug.c
		Modules/mod_ui_scrn_main.c
		Modules/mod_ui_scrn_loading.c
		Drivers/drv_accelerometer.c
		Drivers/drv_adc_volt.c
		Drivers/drv_buttons.c
		Drivers/drv_config.c
		Drivers/drv_display.c
		Drivers/drv_i2c.c
		Drivers/drv_fan.c
		Drivers/drv_lamp.c
		Drivers/drv_magnetometer.c
		Drivers/drv_radar.c
		Drivers/drv_uart_cmd.c
		Drivers/drv_usb_pd.c
		safety_logic.c
		splash_img.c
	)

	add_dependencies(app splash_images)
    target_compile_definitions(app PRIVATE RELEASE_BUILD)

	pico_enable_stdio_usb(app 1)
	pico_enable_stdio_uart(app 0)
endif()

pico_add_extra_outputs(app)

set(LV_CONF_PATH "${CMAKE_CURRENT_LIST_DIR}/lv_conf.h")
target_include_directories(app PRIVATE ${CMAKE_CURRENT_LIST_DIR}/vendor/lvgl/src)
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/vendor/lvgl" lvgl)

target_link_libraries(app
	pico_stdlib
	pico_stdio_uart
	hardware_pwm
	hardware_spi
	hardware_i2c
	hardware_adc
	hardware_dma
	hardware_flash
	hardware_uart

	lvgl
)
