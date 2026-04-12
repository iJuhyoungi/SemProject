set(TARGET app)

# 공통 소스(shared)와 앱 전용 소스(src)를 합쳐서 빌드
file(GLOB SOURCES "src/*.c" "../shared/src/*.c")

add_executable(${TARGET}.elf ${SOURCES})

# App Slot A 전용 링커 스크립트 연결
set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/linker/app_slota.ld)
target_link_options(${TARGET}.elf PRIVATE
    -T${LINKER_SCRIPT}
    -nostdlib
    "-Wl,--gc-sections"
    "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.map"
)

add_custom_command(TARGET ${TARGET}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary ${TARGET}.elf ${TARGET}.bin
    COMMENT "Building 🚀 ${TARGET}.bin (Main Firmware)"
)
