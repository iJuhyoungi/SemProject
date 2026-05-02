set(TARGET bootloader_recovery)                                                                                                                                                          
                                                        
# 공통 소스(shared)와 recovery 전용 소스(src). nested depth: ../../                                                                                                                      
file(GLOB SOURCES "src/*.c" "../../shared/src/*.c")
                                                                                                                                                                                        
add_executable(${TARGET}.elf ${SOURCES})                  
                                                                                                                                                                                        
# IS_BOOTLOADER 매크로 — startup 의 부트로더 분기 활용                                                                                                                                   
# (UART init + .data/.ramfunc reloc + .bss zero, FLASH XIP 실행)
target_compile_definitions(${TARGET}.elf PRIVATE IS_BOOTLOADER)                                                                                                                          
                                                                                                                                                                                        
set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/linker/recovery.ld)                                                                                                                        
target_link_options(${TARGET}.elf PRIVATE                                                                                                                                                
    -T${LINKER_SCRIPT}                                                                                                                                                                   
    -nostdlib
    "-Wl,--gc-sections"                                                                                                                                                                  
    "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.map"  
)                                                                                                                                                                                        

add_custom_command(TARGET ${TARGET}.elf POST_BUILD                                                                                                                                       
    COMMAND ${CMAKE_OBJCOPY} -O binary ${TARGET}.elf ${TARGET}.bin
    COMMENT "Building 🛡️  ${TARGET}.bin (Recovery, 64KB region @ 0x603E0000)"                                                                                                             
)