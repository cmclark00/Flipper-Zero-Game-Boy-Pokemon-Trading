cmake_minimum_required(VERSION 3.13)

# Initialize PICO_SDK from PICO_SDK_PATH if it is set
# else default to searching for PICO_SDK to be alongside this project
set(PICO_SDK_PATH "$ENV{PICO_SDK_PATH}" CACHE PATH "Path to the Raspberry Pi Pico SDK")
if(NOT PICO_SDK_PATH)
    set(PICO_SDK_PATH ../../pico-sdk CACHE PATH "Path to the Raspberry Pi Pico SDK")
endif()

# PICO_SDK_PATH should be a PICO SDK
if(NOT EXISTS ${PICO_SDK_PATH}/pico_sdk_init.cmake)
    message(FATAL_ERROR "Pico SDK not found at ${PICO_SDK_PATH}. Either set PICO_SDK_PATH or place pico-sdk alongside the project.")
endif()

include(${PICO_SDK_PATH}/pico_sdk_init.cmake)

project(rp2040_firmware C CXX ASM)

# Initialize the SDK
pico_sdk_init()

# Add executable. Default name is project name - with a .elf extension
add_executable(${PROJECT_NAME}
        main.c
        gb_link_protocol.c
        pokemon_data.c
        pokemon_table.c
        named_list.c
        stat_nl.c
        item_nl.c
        move_nl.c
        type_nl.c
        pokemon_char_encode.c
        pokemon_storage.c
        web_server.c
        cgi_handlers.c
        gb_link_protocol.c
        trade_logic.c # Added this line
        )

# Generate PIO header
pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/gb_link_protocol.pio.h)

# Add include directory for our project headers
target_include_directories(${PROJECT_NAME} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR} # For lwipopts.h
        ${CMAKE_CURRENT_BINARY_DIR} # For generated PIO header
)

# Add pico_stdlib, hardware_flash, lwip, and usb_net libraries
target_link_libraries(${PROJECT_NAME}
        pico_stdlib
        hardware_flash
        pico_cyw43_arch_none # Or pico_cyw43_arch_poll / pico_cyw43_arch_freertos if WiFi was used. For RNDIS only, 'none' or a minimal arch might be okay.
                            # We need an arch for some lwip utilities. If pico_usb_net provides one, this might not be needed.
                            # Let's use pico_lwip_sys_threadsafe_background for a non-RTOS lwIP.
        pico_lwip_sys_threadsafe_background
        pico_usb_net
)

# Enable USB output for stdio, and also UART
pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 1)

# Add defines for TinyUSB - RNDIS configuration
# These are typically set by pico_usb_net, but let's ensure they are present or don't conflict.
# For RNDIS, the default IP is often 192.168.7.1
# target_compile_definitions(${PROJECT_NAME} PRIVATE
#     CFG_TUSB_RHPORT0_MODE=OPT_MODE_DEVICE
#     CFG_TUD_NET_RNDIS=1
#     CFG_TUD_NET_CDC_NCM=0
#     CFG_TUD_NET_ECM_RNDIS=0 # Ensure only RNDIS is active for ECM style
# )
# The above defines are usually handled by pico_usb_net target.

# If using makefsdata (not in this step as we are embedding the HTML string)
# add_custom_target(fsdata_target ALL COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tools/makefsdata
#     ${CMAKE_CURRENT_SOURCE_DIR}/fs
#     -o ${CMAKE_CURRENT_BINARY_DIR}/httpfsdata.c
#     -f fsdata
#     -r -h -q -e c
# )
# list(APPEND PICO_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/httpfsdata.c)
# add_dependencies(${PROJECT_NAME} fsdata_target)

# Add other libraries if needed
# target_link_libraries(${PROJECT_NAME} hardware_spi hardware_i2c)

# Create map/bin/hex file etc.
pico_add_extra_outputs(${PROJECT_NAME})
