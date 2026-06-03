set(TARGET app_a)

# 공통 소스(shared)와 app 전용 소스(src)
file(GLOB SOURCES "src/*.c" "../../shared/src/*.c")

add_executable(${TARGET}.elf ${SOURCES})

# IS_BOOTLOADER — startup_MIMXRT1020.c 의 UART init + .data/.bss reloc 분기 활용
# (App 도 같은 startup 사용, bootloader 는 아니지만 reloc 동작은 동일)
target_compile_definitions(${TARGET}.elf PRIVATE IS_BOOTLOADER)

# string_min.c 자기-치환 방지 옵션
set_source_files_properties(
    ${CMAKE_SOURCE_DIR}/shared/src/string_min.c
    PROPERTIES COMPILE_OPTIONS "-fno-tree-loop-distribute-patterns;-fno-builtin"
)

set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/linker/app_a.ld)
target_link_options(${TARGET}.elf PRIVATE
    -T${LINKER_SCRIPT}
    -nostdlib
    "-Wl,--gc-sections"
    "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.map"
)

# Post-build: objcopy + RSA 서명 (Stage 2 와 동일 — C-2 에서 검증)
add_custom_command(TARGET ${TARGET}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary ${TARGET}.elf ${TARGET}.bin
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/patch_stage2_header.py ${TARGET}.bin --version 1
    COMMENT "Building 🛡️  ${TARGET}.bin + signing with RSA-2048 (App A, v1)"
)
