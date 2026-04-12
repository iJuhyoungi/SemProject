cmake_minimum_required(VERSION 3.20)
set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

project(MIMXRT1020_OTA C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)

# 공통 컴파일 옵션 (M7 코어, FPU 등)
add_compile_options(
    -mcpu=cortex-m7 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16
    -O2 -g -Wall
)

# 공통 헤더 폴더 (부트로더와 앱 모두 여기서 헤더를 찾음)
include_directories(shared/include)

# 하위 폴더 빌드 지시
add_subdirectory(bootloader)
add_subdirectory(app)
