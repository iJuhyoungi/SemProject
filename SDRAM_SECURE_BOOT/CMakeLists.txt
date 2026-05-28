cmake_minimum_required(VERSION 3.20)

# arm-none-eabi-gcc 는 호스트용 실행파일을 못 만든다. CMake 의 컴파일러
# 검사 단계가 'a.out 실행' 으로 실패하지 않도록 STATIC_LIBRARY 로 우회.
set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

project(MIMXRT1020_SECURE_BOOT C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)

# i.MX RT1020 = Cortex-M7 + 단정밀도 FPU(fpv5-sp-d16).
add_compile_options(
    -mcpu=cortex-m7 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16
    -O2 -g -Wall
)

# shared/ = OTA_AB 에서 검증된 공통 plumbing (startup, clock, uart, crc32 ...).
include_directories(shared/include)

add_subdirectory(bootloader)
add_subdirectory(app)
