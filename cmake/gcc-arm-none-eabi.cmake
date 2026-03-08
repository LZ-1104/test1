set(CMAKE_SYSTEM_NAME               Generic)
set(CMAKE_SYSTEM_PROCESSOR          arm)

# Some default GCC settings
# arm-none-eabi- must be part of path environment
set(TOOLCHAIN_PREFIX                arm-none-eabi-)

find_program(ARM_NONE_EABI_GCC      NAMES ${TOOLCHAIN_PREFIX}gcc REQUIRED)
find_program(ARM_NONE_EABI_GXX      NAMES ${TOOLCHAIN_PREFIX}g++ REQUIRED)
find_program(ARM_NONE_EABI_OBJCOPY  NAMES ${TOOLCHAIN_PREFIX}objcopy REQUIRED)
find_program(ARM_NONE_EABI_SIZE     NAMES ${TOOLCHAIN_PREFIX}size REQUIRED)

set(CMAKE_C_COMPILER                ${ARM_NONE_EABI_GCC} CACHE FILEPATH "C compiler" FORCE)
set(CMAKE_ASM_COMPILER              ${ARM_NONE_EABI_GCC} CACHE FILEPATH "ASM compiler" FORCE)
set(CMAKE_CXX_COMPILER              ${ARM_NONE_EABI_GXX} CACHE FILEPATH "CXX compiler" FORCE)
set(CMAKE_LINKER                    ${ARM_NONE_EABI_GXX} CACHE FILEPATH "Linker" FORCE)
set(CMAKE_OBJCOPY                   ${ARM_NONE_EABI_OBJCOPY} CACHE FILEPATH "Objcopy" FORCE)
set(CMAKE_SIZE                      ${ARM_NONE_EABI_SIZE} CACHE FILEPATH "Size" FORCE)
#调试器
set(CMAKE_C_DEBUGGER                ${TOOLCHAIN_PREFIX}gdb)

set(CMAKE_EXECUTABLE_SUFFIX_ASM     ".elf")
set(CMAKE_EXECUTABLE_SUFFIX_C       ".elf")
set(CMAKE_EXECUTABLE_SUFFIX_CXX     ".elf")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# MCU specific flags
set(TARGET_FLAGS "-mcpu=cortex-m3 ")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${TARGET_FLAGS}")
set(CMAKE_ASM_FLAGS "${CMAKE_C_FLAGS} -x assembler-with-cpp -MMD -MP")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic -fdata-sections -ffunction-sections")

set(CMAKE_C_FLAGS_DEBUG "-O0 -g3")
set(CMAKE_C_FLAGS_RELEASE "-Os -g0")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3")
set(CMAKE_CXX_FLAGS_RELEASE "-Os -g0")

set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -fno-rtti -fno-exceptions -fno-threadsafe-statics")

set(CMAKE_C_LINK_FLAGS "${TARGET_FLAGS}")
set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -T \"${CMAKE_SOURCE_DIR}/STM32F103XX_FLASH.ld\"")
set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} --specs=nano.specs")
set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -Wl,-Map=${CMAKE_PROJECT_NAME}.map -Wl,--gc-sections")
set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -Wl,--start-group -lc -lm -Wl,--end-group")
set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -Wl,--print-memory-usage")

set(CMAKE_CXX_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -Wl,--start-group -lstdc++ -lsupc++ -Wl,--end-group")