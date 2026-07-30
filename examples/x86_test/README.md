# FreeRTOS NTP Client - x86 Test Example

This example demonstrates the usage of the FreeRTOS NTP Client on an x86 POSIX platform. It shows the basic integration with FreeRTOS and how to query NTP servers, synchronize time, and use callbacks.

## Overview

This example:
- Initializes the FreeRTOS Scheduler on an x86 POSIX platform
- Configures the NTP Client with multiple public NTP servers
- Registers callbacks for time updates and skew adjustments
- Displays synchronization status periodically
- Runs continuously and synchronizes time at regular intervals

## Prerequisites

### In DevContainer

The DevContainer already contains all necessary dependencies. Make sure that:

1. You have opened the project in the DevContainer (`F1` → "Dev Containers: Reopen in Container")
2. FreeRTOS-Kernel and FreeRTOS-Plus-TCP have been automatically cloned (happens on first start)

### Outside DevContainer

If you're developing outside the DevContainer, you need:

```bash
sudo apt-get install -y \
    build-essential \
    gcc \
    make \
    cmake \
    libssl-dev \
    libmbedtls-dev \
    chrony
```

## Build Instructions

### Option 1: With Make (recommended for quick tests)

```bash
# Navigate to the example directory
cd examples/x86_test

# Build the example
make

# The binary will be created: ./ntp_test
```

**Note:** The Makefile uses FreeRTOS paths from the DevContainer by default (`/workspace/FreeRTOS-Kernel` and `/workspace/FreeRTOS-Plus-TCP`). If these differ, override the variables:

```bash
make FREERTOS_PATH=/path/to/FreeRTOS-Kernel \
     FREERTOS_TCP_PATH=/path/to/FreeRTOS-Plus-TCP
```

### Option 2: With CMake (recommended for integration)

```bash
# Navigate to the project root directory
cd /workspaces/freertos-ntp

# Create build directory
mkdir -p build
cd build

# Configure with CMake
cmake .. \
  -DFREERTOS_KERNEL_PATH=/workspace/FreeRTOS-Kernel \
  -DFREERTOS_PLUS_TCP_PATH=/workspace/FreeRTOS-Plus-TCP \
  -DFREERTOS_PORT=GCC/Posix \
  -DBUILD_NTP_EXAMPLE=ON

# Build everything
make

# The binary will be created: ./examples/x86_test/ntp_test
```

### Clean Build Artifacts

```bash
# With Make
cd examples/x86_test
make clean

# Clean everything including library
make clean-all

# With CMake
cd build
rm -rf *
```

## Starting Chrony NTP Server

For local testing, you should start the Chrony NTP Server:

```bash
# Start Chrony
.devcontainer/start-chrony.sh

# Check the status
chronyc tracking

# Show NTP sources
chronyc sources
```

Chrony will then run on `localhost:123` (UDP) and can be used by the example.

## Running the Example

### Built with Make

```bash
cd examples/x86_test
./ntp_test
```

### Built with CMake

```bash
cd build/examples/x86_test
./ntp_test
```

### Expected Output

```
NTP client initialized successfully
Callbacks registered
Added server: pool.ntp.org
Added server: time.google.com
Added server: time.cloudflare.com
Added server: time.nist.gov

NTP synchronization started

[NTP] Time set to: 2026-02-13 14:32:15.123456 UTC
[Status] NTP Status: SYNCED | Servers: 4 | Last Sync: 0 seconds ago
[Status] Current Time: 2026-02-13 14:32:15.234567 UTC
...
```

The program runs continuously and displays the status every 10 seconds. Stop it with `Ctrl+C`.

## Customizing Configuration

### Changing NTP Servers

Edit [main.c](main.c) and modify the server addresses:

```c
// For local testing with Chrony
if (xNTPAddServer(xNTPHandle, "127.0.0.1")) {
    printf("Added server: 127.0.0.1\n");
}

// Or use other NTP servers
if (xNTPAddServer(xNTPHandle, "de.pool.ntp.org")) {
    printf("Added server: de.pool.ntp.org\n");
}
```

### Changing Polling Interval

Modify the polling interval in the configuration:

```c
xNTPConfig.poll_interval = 16;  // 16 seconds (default value)
xNTPConfig.poll_interval = 64;  // 64 seconds for less frequent polling
```

### Timeout and Retries

```c
xNTPConfig.timeout_ms = 5000;    // 5 second timeout
xNTPConfig.max_retries = 3;      // Maximum retry attempts
```

## Troubleshooting

### "Failed to initialize NTP client"

**Cause:** FreeRTOS components could not be initialized.

**Solution:**
- Ensure that `FreeRTOSConfig.h` is configured correctly
- Check if enough heap memory is available

### "Network error" or no time updates

**Cause:** Network stack is not fully implemented or NTP server is unreachable.

**Solution:**
- This example is a demonstration of API usage
- For real network functionality, FreeRTOS-Plus-TCP must be fully integrated
- For local testing: Ensure that Chrony is running (`chronyc tracking`)

### Chrony is not reachable

```bash
# Check if Chrony is running
sudo systemctl status chrony

# Or check with netstat
sudo netstat -lnup | grep 123

# Restart Chrony
.devcontainer/start-chrony.sh
```

### Compilation Errors: "FreeRTOS.h not found"

**Solution:**
```bash
# Ensure that FreeRTOS has been cloned
ls /workspace/FreeRTOS-Kernel

# If not present, clone it
git clone --depth 1 https://github.com/FreeRTOS/FreeRTOS-Kernel.git /workspace/FreeRTOS-Kernel
```

### Linker Errors: "undefined reference to pthread_*"

**Solution:**
- Ensure that pthread is linked (`-lpthread`)
- This is already configured in the Makefile

## Extending the Example

### Adding Custom Callbacks

```c
// Custom callback for time synchronization
static void myTimeSetCallback(uint32_t seconds, uint32_t microseconds)
{
    // Set hardware RTC
    // Update system time
    // Log events
    
    printf("Time updated: %u.%06u\n", seconds, microseconds);
}

// Register the callback
vNTPRegisterTimeSetCallback(xNTPHandle, myTimeSetCallback);
```

### Multiple NTP Clients

You can create multiple NTP client instances for different purposes:

```c
NTPTaskHandle_t xClient1 = xNTPClientInit(&config1);
NTPTaskHandle_t xClient2 = xNTPClientInit(&config2);

// Client 1 for accurate time
// Client 2 for redundancy with other servers
```

### Integration into Existing FreeRTOS Project

1. Copy the NTP library sources into your project
2. Add the include paths
3. Link against `libfreertos_ntp.a` and OpenSSL
4. Call `xNTPClientInit()` at startup
5. Configure NTP servers and callbacks

## Further Information

- **API Documentation:** [../../API.md](../../API.md)
- **Configuration:** [../../CONFIGURATION.md](../../CONFIGURATION.md)
- **Quick Start Guide:** [../../QUICKSTART.md](../../QUICKSTART.md)
- **DevContainer Setup:** [../../.devcontainer/README.md](../../.devcontainer/README.md)

## License

SPDX-License-Identifier: MIT

Copyright (c) 2026 Daniel Glaser
