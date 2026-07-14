cmake_minimum_required(VERSION 3.20)

# arm-none-eabi-gcc는 호스트 a.out을 못 만든다 → STATIC_LIBRARY로 컴파일러 검사 우회
set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

project(RT1020_FLS C ASM)

set(CMAKE_C_STANDARD 11)

# i.MX RT1020 = Cortex-M7 + 단정밀도 FPU
add_compile_options(
    -mcpu=cortex-m7 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16
    -O2 -g -Wall
)

include_directories(shared/include)
include_directories(include)            # 프로젝트 전용 헤더 (Fls.h 등)

set(TARGET rt1020_fls)

# 진입 소스(src) + 공통 plumbing(shared)
file(GLOB SOURCES "src/*.c" "shared/src/*.c")
add_executable(${TARGET}.elf ${SOURCES})

# startup의 IS_BOOTLOADER 분기 사용 = flash XIP + UART init + 최소 reloc
target_compile_definitions(${TARGET}.elf PRIVATE IS_BOOTLOADER)

# string_min.c가 자기 자신을 memset 호출로 치환당하는 것 방지 (stage1과 동일)
set_source_files_properties(
    ${CMAKE_SOURCE_DIR}/shared/src/string_min.c
    PROPERTIES COMPILE_OPTIONS "-fno-tree-loop-distribute-patterns;-fno-builtin"
)

set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/linker/fls.ld)
target_link_options(${TARGET}.elf PRIVATE
    -T${LINKER_SCRIPT}
    -nostdlib
    "-Wl,--gc-sections"
    "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.map"
)

add_custom_command(TARGET ${TARGET}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary ${TARGET}.elf ${TARGET}.bin
    COMMAND ${CMAKE_SIZE} ${TARGET}.elf
    COMMENT "Building ${TARGET}.bin (fls single image)"
)
