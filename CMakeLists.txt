cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
#忽略未使用变量的警告
add_compile_options(-Wno-unused-variable)
add_compile_options(-Wno-unused-parameter)
add_compile_options(-Wno-unused-function)
# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME test1)


# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})
target_link_options(test1 PRIVATE -u _printf_float)

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)#添加STM32CubeMX生成的代码
# Add LVGL library
add_subdirectory(LVGL/lvgl)
# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths

)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    ${CMAKE_SOURCE_DIR}/bsp/ILI9341.c
    ${CMAKE_SOURCE_DIR}/bsp/fonts.c
    ${CMAKE_SOURCE_DIR}/bsp/XPT2046.c
    ${CMAKE_SOURCE_DIR}/bsp/bsp_spi_flash.c
    ${CMAKE_SOURCE_DIR}/bsp/bsp_debug_usart.c
     ${CMAKE_SOURCE_DIR}/bsp/lv_port_disp.c
        ${CMAKE_SOURCE_DIR}/bsp/lv_port_indev.c
    ${CMAKE_SOURCE_DIR}/Application/test.c
     ${CMAKE_SOURCE_DIR}/bsp/image.c
    ${CMAKE_SOURCE_DIR}/bsp/BH1750.c
    ${CMAKE_SOURCE_DIR}/bsp/MAX30102.c
    ${CMAKE_SOURCE_DIR}/system/IIC.c
    ${CMAKE_SOURCE_DIR}/bsp/DHT11.c
    ${CMAKE_SOURCE_DIR}/Middlewares/Log/log.c
    ${CMAKE_SOURCE_DIR}/Application/timer.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    bsp
    system
    Application
    Middlewares/Log
    Middlewares/Status
    "LVGL/lvgl"
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    lvgl
    # Add user defined libraries
)

# Export firmware images after linking
set(FIRMWARE_ELF $<TARGET_FILE:${CMAKE_PROJECT_NAME}>)
set(FIRMWARE_HEX ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.hex)
set(FIRMWARE_BIN ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.bin)

add_custom_command(
    TARGET ${CMAKE_PROJECT_NAME}
    POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex ${FIRMWARE_ELF} ${FIRMWARE_HEX}
    COMMAND ${CMAKE_OBJCOPY} -O binary ${FIRMWARE_ELF} ${FIRMWARE_BIN}
    COMMENT "Generating ${CMAKE_PROJECT_NAME}.hex and ${CMAKE_PROJECT_NAME}.bin"
)

# Flash task (default: OpenOCD + DAPLink/CMSIS-DAP)
# Compatibility: if old cache still keeps FLASH_TOOL=st-flash, migrate to OpenOCD.
if(DEFINED FLASH_TOOL AND FLASH_TOOL STREQUAL "st-flash")
    set(FLASH_TOOL "openocd" CACHE STRING "Legacy flash utility command" FORCE)
endif()

set(OPENOCD_COMMAND "openocd" CACHE STRING "OpenOCD executable command")
set(OPENOCD_INTERFACE_CFG "interface/cmsis-dap.cfg" CACHE STRING "OpenOCD interface config")
set(OPENOCD_TARGET_CFG "target/stm32f1x.cfg" CACHE STRING "OpenOCD target config")
set(OPENOCD_PROGRAM_ARGS "program ${FIRMWARE_ELF} verify reset exit" CACHE STRING "OpenOCD program command")

add_custom_target(flash
    COMMAND ${OPENOCD_COMMAND}
            -f ${OPENOCD_INTERFACE_CFG}
            -f ${OPENOCD_TARGET_CFG}
            -c ${OPENOCD_PROGRAM_ARGS}
    DEPENDS ${CMAKE_PROJECT_NAME}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Flashing ${FIRMWARE_ELF} using OpenOCD"
)


