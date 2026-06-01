# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
# Purpose: Defines the Pico firmware build for the picoscope executable.
# Targets C++17 on a Pico (RP2040) and links only the
# Pico SDK libraries needed by ADC, DMA, GPIO, SPI, and stdio.

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_EXPORT_COMPILE_COMMANDS)
  set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES 
      ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
endif()

if (NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(PICO_BOARD pico CACHE STRING "Board type")

if (NOT PICO_TOOLCHAIN_PATH)
    set(_LOCAL_PICO_TOOLCHAIN "$ENV{HOME}/.pico-sdk/toolchain/14_2_Rel1")
    if (EXISTS "${_LOCAL_PICO_TOOLCHAIN}/bin/arm-none-eabi-gcc")
        set(PICO_TOOLCHAIN_PATH "${_LOCAL_PICO_TOOLCHAIN}" CACHE PATH "Path to Pico ARM toolchain")
    endif()
endif()

if (NOT picotool_DIR)
    set(_LOCAL_PICOTOOL_DIR "$ENV{HOME}/.pico-sdk/picotool/2.2.0-a4/picotool")
    if (EXISTS "${_LOCAL_PICOTOOL_DIR}/picotoolConfig.cmake")
        set(picotool_DIR "${_LOCAL_PICOTOOL_DIR}" CACHE PATH "Path to picotool CMake package")
    endif()
endif()

include(pico_sdk_import.cmake)

project(picoscope C CXX ASM)

pico_sdk_init()

add_executable(picoscope
    src/app/capture_pipeline.cpp
    src/app/render_pipeline.cpp
    src/core/acquisition_engine.cpp
    src/core/history_acquisition.cpp
    src/core/rotary_decoder.cpp
    src/core/scope_control.cpp
    src/hw/adc_sampler.cpp
    src/hw/display_dma.cpp
    src/hw/encoder_manager.cpp
    src/hw/ili9341.cpp
    src/ui/font_config.cpp
    src/ui/strip_renderer.cpp
    src/main.cpp
)

target_include_directories(picoscope PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include
)

target_compile_options(picoscope PRIVATE
    -Wall
    -Wextra
)

target_compile_definitions(picoscope PRIVATE
    PICO_STACK_SIZE=0x1000
    PICO_CORE1_STACK_SIZE=0x1000
)

target_link_libraries(picoscope
    pico_stdlib
    pico_multicore
    hardware_adc
    hardware_dma
    hardware_gpio
    hardware_sync
    hardware_spi
)

pico_enable_stdio_usb(picoscope 1)
pico_enable_stdio_uart(picoscope 0)

pico_add_extra_outputs(picoscope)
