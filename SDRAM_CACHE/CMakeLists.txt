cmake_minimum_required(VERSION 3.20)

# CMake 컴파일러 테스트 우회
set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

project(MIMXRT1020_SDRAM_CACHE C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)

# [추가] 디버깅을 위한 빌드 타입 설정 (기본값을 Debug로 설정)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# 하드웨어 플래그 설정
set(MCPU_FLAGS "-mcpu=cortex-m7 -mthumb -mfloat-abi=hard -mfpu=fpv5-d16")

# 공통 플래그 업데이트: -g3(상세 디버그 심볼) 및 -O0(최적화 해제) 추가
# -fno-common은 변수 정의 중복 에러를 방지하고 디버깅 시 변수 추적을 돕습니다.
set(COMMON_FLAGS "${MCPU_FLAGS} --specs=nosys.specs -ffreestanding -fdata-sections -ffunction-sections -fno-common -Wall -Wextra")

# C 컴파일 플래그 (Debug 모드일 때 -g3 -O0 자동 적용되도록 구성 가능하지만, 명시적으로 추가)
set(CMAKE_C_FLAGS_DEBUG "-g3 -O0")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMMON_FLAGS} ${CMAKE_C_FLAGS_DEBUG}")

# ASM 플래그
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} ${COMMON_FLAGS} -g3")

# 링커 플래그: 링커에서도 디버그 정보를 유지하도록 설정
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${MCPU_FLAGS} -T${CMAKE_SOURCE_DIR}/linker/MIMXRT1020_flexspi.ld -Wl,--gc-sections -Wl,-Map=${CMAKE_BINARY_DIR}/${PROJECT_NAME}.map -g3")

add_executable(${PROJECT_NAME}
    src/main.c
    src/system_MIMXRT1020.c
    src/boot_header.c
    src/clock.c
    src/flexspi_nor_config.c
    src/led.c
    src/semc.c
    src/uart.c
    startup/startup_MIMXRT1020.c
    src/mpu.c
    src/cache.c
)

target_include_directories(${PROJECT_NAME} PRIVATE
    include
)
