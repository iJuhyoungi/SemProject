set(TARGET bootloader_stage1)

# 공통 소스(shared)와 stage1 전용 소스(src)
file(GLOB SOURCES "src/*.c" "../shared/src/*.c")

add_executable(${TARGET}.elf ${SOURCES})

# IS_BOOTLOADER 매크로 — startup_MIMXRT1020.c 의 부트로더 분기 활용
# (UART init + .data/.ramfunc reloc + .bss zero)
target_compile_definitions(${TARGET}.elf PRIVATE IS_BOOTLOADER)

set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/linker/stage1.ld)
target_link_options(${TARGET}.elf PRIVATE
    -T${LINKER_SCRIPT}
    -nostdlib
    "-Wl,--gc-sections"
    "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.map"
)

add_custom_command(TARGET ${TARGET}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary ${TARGET}.elf ${TARGET}.bin
    COMMENT "Building 🛡️ ${TARGET}.bin (Stage 1 immutable, ~4KB)"
)
