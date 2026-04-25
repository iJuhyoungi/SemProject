# set(TARGET app_slot_b)

# file(GLOB SOURCES "src/*.c" "../shared/src/*.c")

# add_executable(${TARGET}.elf ${SOURCES})

# target_compile_definitions(${TARGET}.elf PRIVATE IS_DUMMY_APP)

# set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/linker/app_slot_b.ld)
# target_link_options(${TARGET}.elf PRIVATE
#     -T${LINKER_SCRIPT}
#     -nostdlib
#     "-Wl,--gc-sections"
#     "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.map"
# )

# add_custom_command(TARGET ${TARGET}.elf POST_BUILD
#     COMMAND ${CMAKE_OBJCOPY} -O binary ${TARGET}.elf ${TARGET}.bin
#     COMMENT "Building ${TARGET}.bin"
# )

set(TARGET app_slot_b)

file(GLOB SOURCES "src/*.c" "../shared/src/*.c")

add_executable(${TARGET}.elf ${SOURCES})

# 더 이상 dummy app으로 빌드하지 않음.
# B도 Slot confirm write를 해야 하므로 SDRAM/ramfunc relocation 경로를 탄다.
# target_compile_definitions(${TARGET}.elf PRIVATE IS_DUMMY_APP)

target_include_directories(${TARGET}.elf PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/linker/app_slot_b.ld)
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