set(TARGET app_slot_a)

file(GLOB SOURCES "src/*.c" "../shared/src/*.c")

add_executable(${TARGET}.elf ${SOURCES})

# target_compile_definitions(${TARGET}.elf PRIVATE IS_DUMMY_APP)

set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/linker/app_slot_a.ld)
target_link_options(${TARGET}.elf PRIVATE
    -T${LINKER_SCRIPT}
    -nostdlib
    "-Wl,--gc-sections"
    "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.map"
)

add_custom_command(TARGET ${TARGET}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary ${TARGET}.elf ${TARGET}.bin
    COMMENT "Building ${TARGET}.bin"
)
