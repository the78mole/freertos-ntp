cmake_minimum_required(VERSION 3.13)
project(freertos-ntp-x86-test C)

# FreeRTOS Kernel source files
set(FREERTOS_KERNEL_SOURCES
    ${FREERTOS_KERNEL_PATH}/tasks.c
    ${FREERTOS_KERNEL_PATH}/queue.c
    ${FREERTOS_KERNEL_PATH}/list.c
    ${FREERTOS_KERNEL_PATH}/timers.c
    ${FREERTOS_KERNEL_PATH}/event_groups.c
    ${FREERTOS_KERNEL_PATH}/stream_buffer.c
    ${FREERTOS_KERNEL_PATH}/portable/GCC/Posix/port.c
    ${FREERTOS_KERNEL_PATH}/portable/MemMang/heap_3.c
)

# Example source files
set(EXAMPLE_SOURCES
    main.c
)

# Create executable
add_executable(ntp_test 
    ${EXAMPLE_SOURCES}
    ${FREERTOS_KERNEL_SOURCES}
)

# Include directories
target_include_directories(ntp_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${FREERTOS_KERNEL_PATH}/include
    ${FREERTOS_KERNEL_PATH}/portable/GCC/Posix
    ${FREERTOS_PLUS_TCP_PATH}/include
    ${FREERTOS_PLUS_TCP_PATH}/portable/Compiler/GCC
)

# Link libraries
target_link_libraries(ntp_test PRIVATE
    freertos_ntp
    pthread
    ssl
    crypto
)

# Compiler options
target_compile_options(ntp_test PRIVATE
    -Wall
    -Wextra
    -pthread
    -DprojCOVERAGE_TEST=0
)

# Link options
target_link_options(ntp_test PRIVATE
    -pthread
)
