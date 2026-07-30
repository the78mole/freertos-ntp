cmake_minimum_required(VERSION 3.13)

project(freertos-ntp C)

# Library sources
set(NTP_SOURCES
    source/freertos_ntp.c
    source/freertos_ntp_nts.c
    source/freertos_ntp_broadcast.c
)

# Library headers
set(NTP_HEADERS
    include/freertos_ntp.h
    include/freertos_ntp_nts.h
    include/freertos_ntp_broadcast.h
    include/freertos_ntp_config.h
)

# Create library
add_library(freertos_ntp STATIC ${NTP_SOURCES})

# Include directories
target_include_directories(freertos_ntp PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# This library requires FreeRTOS and FreeRTOS-Plus-TCP
# These paths should be provided by the parent project
target_include_directories(freertos_ntp PUBLIC
    ${FREERTOS_KERNEL_PATH}/include
    ${FREERTOS_PLUS_TCP_PATH}/include
    ${FREERTOS_PLUS_TCP_PATH}/portable/${FREERTOS_PORT}
)

# Compiler options
target_compile_options(freertos_ntp PRIVATE
    -Wall
    -Wextra
    -Werror
)

# Optional: Build example
option(BUILD_NTP_EXAMPLE "Build x86 test example" OFF)

if(BUILD_NTP_EXAMPLE)
    add_subdirectory(examples/x86_test)
endif()

# Installation
install(TARGETS freertos_ntp
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES ${NTP_HEADERS}
    DESTINATION include/freertos_ntp
)
