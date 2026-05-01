cmake_minimum_required(VERSION 3.20)
set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

project(MIMXRT1020_OTA_AB C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)

add_compile_options(
    -mcpu=cortex-m7 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16
    -O2 -g -Wall
)

include_directories(shared/include)
include_directories(bootloader/include)
include_directories(app_slot_a/include)

add_subdirectory(bootloader)
add_subdirectory(bootloader_stage1)
add_subdirectory(app_slot_a)
add_subdirectory(app_slot_b)
add_subdirectory(bootctrl)

