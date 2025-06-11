# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD waveshare_rp2040_zero CACHE STRING "Board type")

cmake_minimum_required(VERSION 3.13)
include(pico_sdk_import.cmake)

project(rp2040_zero_gb_pokemon C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(rp2040_zero_gb_pokemon
    main.c
    gb_link.c
    storage.c
    ui.c
)

# Enable USB output, disable UART output
pico_enable_stdio_usb(rp2040_zero_gb_pokemon 1)
pico_enable_stdio_uart(rp2040_zero_gb_pokemon 0)

# Add SD card and FatFs libraries if needed
# target_link_libraries(rp2040_zero_gb_pokemon pico_stdlib hardware_spi fatfs)

# For now, just link the standard library and hardware libraries
 target_link_libraries(rp2040_zero_gb_pokemon pico_stdlib hardware_gpio hardware_spi hardware_timer)

pico_add_extra_outputs(rp2040_zero_gb_pokemon) 