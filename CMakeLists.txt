cmake_minimum_required(VERSION 3.13)

#set(PICO_BOARD pico_w)

# Load Pico SDK
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

# Proje adı
project(cv-pico-seq C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_COMPILER /usr/bin/arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER /usr/bin/arm-none-eabi-g++)

pico_sdk_init()

find_program(PICOTOOL_EXECUTABLE picotool PATHS /usr/bin)

add_executable(${CMAKE_PROJECT_NAME}
    main.cpp
    clock.cpp
    io.cpp
    sequencer.cpp
    ui.cpp
    eeprom.cpp
)

target_link_libraries(${CMAKE_PROJECT_NAME} 
    pico_stdlib
    pico_multicore
    hardware_timer
    hardware_i2c)
    # Add SPI hardware library for MCP4822 driver
target_link_libraries(${CMAKE_PROJECT_NAME} hardware_spi)

add_compile_definitions(PICO_DISABLE_SPI=0)

# Enable USB reset interface for picotool reboot capability (must be before pico_enable_stdio_usb)
add_compile_definitions(PICO_STDIO_USB_ENABLE_RESET_VIA_VENDOR_INTERFACE=1)

pico_enable_stdio_usb(${CMAKE_PROJECT_NAME} 1)
pico_enable_stdio_uart(${CMAKE_PROJECT_NAME} 0)

pico_add_extra_outputs(${CMAKE_PROJECT_NAME})

set(OUTPUT_UF2 "${CMAKE_PROJECT_NAME}.uf2")

# Help VS Code C/C++ extension find include paths
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


install(
    CODE
        "
execute_process(
COMMAND sudo picotool load -v -x ${OUTPUT_UF2} -f
RESULT_VARIABLE result
)
if(NOT result EQUAL 0)
message(FATAL_ERROR \"Picotool command failed\")
endif()
"
)
