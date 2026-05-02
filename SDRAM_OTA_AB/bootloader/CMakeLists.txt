# bootloader 컴포넌트의 부모 컨테이너.
# Stage 1 (immutable, 8 KB) + Stage 2 (updateable, 240 KB) 의 nested 구조.                                                                                                               
                                                                                                                                                                                           
add_subdirectory(stage1)                                                                                                                                                                 
add_subdirectory(stage2) 