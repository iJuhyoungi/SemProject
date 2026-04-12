set(TARGET boot_ctrl)

add_executable(${TARGET}.elf
    src/boot_ctrl_default.c
)

target_include_directories(${TARGET}.elf PRIVATE
    ${CMAKE_SOURCE_DIR}/bootloader/include
)

target_link_options(${TARGET}.elf PRIVATE
    -T${CMAKE_CURRENT_SOURCE_DIR}/linker/boot_ctrl.ld
    -nostdlib
    "-Wl,--gc-sections"
    "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.map"
)

add_custom_command(TARGET ${TARGET}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary ${TARGET}.elf ${TARGET}.bin
    COMMENT "Building ${TARGET}.bin"
)

