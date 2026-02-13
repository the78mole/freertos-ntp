# FreeRTOS NTP Client Makefile
# This is a simple Makefile for building the library
# Adjust paths according to your FreeRTOS installation

# Compiler
CC = gcc
AR = ar

# Directories
SRC_DIR = source
INC_DIR = include
BUILD_DIR = build
LIB_DIR = lib

# FreeRTOS paths (adjust these to your installation)
FREERTOS_PATH ?= ../FreeRTOS-Kernel
FREERTOS_TCP_PATH ?= ../FreeRTOS-Plus-TCP
FREERTOS_PORT ?= GCC/Posix

# Include paths
INCLUDES = -I$(INC_DIR) \
           -I$(FREERTOS_PATH)/include \
           -I$(FREERTOS_TCP_PATH)/include \
           -I$(FREERTOS_TCP_PATH)/portable/$(FREERTOS_PORT)

# Compiler flags
CFLAGS = -Wall -Wextra -O2 $(INCLUDES)

# Source files
SOURCES = $(SRC_DIR)/freertos_ntp.c \
          $(SRC_DIR)/freertos_ntp_nts.c \
          $(SRC_DIR)/freertos_ntp_broadcast.c

# Object files
OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES))

# Library name
LIBRARY = $(LIB_DIR)/libfreertos_ntp.a

# Default target
all: $(LIBRARY)

# Create directories
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(LIB_DIR):
	mkdir -p $(LIB_DIR)

# Compile source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Create library
$(LIBRARY): $(OBJECTS) | $(LIB_DIR)
	$(AR) rcs $@ $(OBJECTS)

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR) $(LIB_DIR)

# Install (copy headers and library)
install: $(LIBRARY)
	mkdir -p /usr/local/include/freertos_ntp
	mkdir -p /usr/local/lib
	cp $(INC_DIR)/*.h /usr/local/include/freertos_ntp/
	cp $(LIBRARY) /usr/local/lib/

.PHONY: all clean install
