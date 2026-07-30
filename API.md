# FreeRTOS NTP Client - API Documentation

## Table of Contents
- [Overview](#overview)
- [Data Types](#data-types)
- [Core API](#core-api)
- [NTS API](#nts-api)
- [Broadcast API](#broadcast-api)
- [Callback Functions](#callback-functions)
- [Usage Examples](#usage-examples)

## Overview

The FreeRTOS NTP client provides a comprehensive time synchronization solution with support for SNTP, full NTP with metrics, NTS security, and broadcast mode.

## Data Types

### NTPConfig_t

Configuration structure for initializing the NTP client.

```c
typedef struct {
    bool     use_sntp_mode;      // Use simple SNTP mode (no metrics)
    bool     use_nts;            // Use NTS (Network Time Security)
    bool     enable_broadcast;   // Enable broadcast mode reception
    uint16_t port;               // NTP server port (default 123)
    uint32_t poll_interval;      // Poll interval in seconds
    uint32_t timeout_ms;         // Response timeout in milliseconds
    uint16_t task_priority;      // FreeRTOS task priority
    uint32_t task_stack_size;    // FreeRTOS task stack size
} NTPConfig_t;
```

### NTPServerStats_t

Server statistics structure containing metrics and status information.

```c
typedef struct {
    char     hostname[64];       // Server hostname or IP
    uint32_t ip_address;         // Resolved IP address
    uint8_t  stratum;            // Server stratum
    uint8_t  reach;              // Reachability register (8-bit shift register)
    uint16_t flags;              // Status flags
    int32_t  offset_us;          // Clock offset in microseconds
    int32_t  delay_us;           // Round-trip delay in microseconds
    int32_t  jitter_us;          // Jitter in microseconds
    uint32_t poll_interval;      // Polling interval in seconds
    uint32_t last_update;        // Last successful update timestamp
    uint32_t ref_id;             // Reference identifier
} NTPServerStats_t;
```

### Status Flags

```c
#define NTP_SERVER_FLAG_ACTIVE      (1 << 0)  // Server is active
#define NTP_SERVER_FLAG_VALID       (1 << 1)  // Server has valid data
#define NTP_SERVER_FLAG_REACHABLE   (1 << 2)  // Server is reachable
#define NTP_SERVER_FLAG_SELECTED    (1 << 3)  // Server is selected for sync
```

## Core API

### vNTPGetDefaultConfig

Get default NTP configuration.

```c
void vNTPGetDefaultConfig(NTPConfig_t *config);
```

**Parameters:**
- `config`: Pointer to configuration structure to fill

**Returns:** None

**Example:**
```c
NTPConfig_t config;
vNTPGetDefaultConfig(&config);
config.poll_interval = 32;  // Customize as needed
```

### xNTPClientInit

Initialize NTP client with configuration.

```c
NTPTaskHandle_t xNTPClientInit(const NTPConfig_t *config);
```

**Parameters:**
- `config`: Pointer to NTP configuration structure

**Returns:** Handle to NTP task, or NULL on failure

**Example:**
```c
NTPTaskHandle_t ntp = xNTPClientInit(&config);
if (ntp == NULL) {
    // Handle initialization failure
}
```

### xNTPAddServer

Add NTP server to configuration.

```c
bool xNTPAddServer(NTPTaskHandle_t handle, const char *hostname);
```

**Parameters:**
- `handle`: NTP task handle
- `hostname`: Server hostname or IP address

**Returns:** true if server added successfully, false otherwise

**Example:**
```c
xNTPAddServer(ntp, "pool.ntp.org");
xNTPAddServer(ntp, "time.google.com");
```

### xNTPRemoveServer

Remove NTP server from configuration.

```c
bool xNTPRemoveServer(NTPTaskHandle_t handle, const char *hostname);
```

**Parameters:**
- `handle`: NTP task handle
- `hostname`: Server hostname or IP address

**Returns:** true if server removed successfully, false otherwise

### xNTPStart

Start NTP synchronization.

```c
bool xNTPStart(NTPTaskHandle_t handle);
```

**Parameters:**
- `handle`: NTP task handle

**Returns:** true if started successfully, false otherwise

**Example:**
```c
if (!xNTPStart(ntp)) {
    // Handle start failure
}
```

### vNTPStop

Stop NTP synchronization.

```c
void vNTPStop(NTPTaskHandle_t handle);
```

**Parameters:**
- `handle`: NTP task handle

**Returns:** None

### ulNTPGetServerStats

Get status of all configured servers.

```c
uint32_t ulNTPGetServerStats(NTPTaskHandle_t handle, 
                              NTPServerStats_t *stats, 
                              uint32_t max_servers);
```

**Parameters:**
- `handle`: NTP task handle
- `stats`: Array to store server statistics
- `max_servers`: Maximum number of servers to return

**Returns:** Number of servers returned

**Example:**
```c
NTPServerStats_t stats[16];
uint32_t count = ulNTPGetServerStats(ntp, stats, 16);
for (uint32_t i = 0; i < count; i++) {
    printf("Server: %s, Offset: %d us\n", 
           stats[i].hostname, stats[i].offset_us);
}
```

### ulNTPGetStatusString

Get formatted status string (similar to ntpq -p).

```c
uint32_t ulNTPGetStatusString(NTPTaskHandle_t handle, 
                               char *buffer, 
                               uint32_t buffer_size);
```

**Parameters:**
- `handle`: NTP task handle
- `buffer`: Buffer to store formatted string
- `buffer_size`: Size of the buffer

**Returns:** Number of characters written

**Example:**
```c
char buffer[2048];
ulNTPGetStatusString(ntp, buffer, sizeof(buffer));
printf("%s", buffer);
```

## NTS API

### xNTPEnableNTS

Enable NTS (Network Time Security) for a server.

```c
bool xNTPEnableNTS(NTPTaskHandle_t handle, 
                   const char *hostname,
                   const char *nts_ke_server, 
                   uint16_t nts_ke_port);
```

**Parameters:**
- `handle`: NTP task handle
- `hostname`: Server hostname
- `nts_ke_server`: NTS-KE server hostname (NULL to use same as NTP server)
- `nts_ke_port`: NTS-KE server port (0 for default 4460)

**Returns:** true if NTS enabled successfully, false otherwise

**Example:**
```c
// Add server first
xNTPAddServer(ntp, "time.cloudflare.com");

// Enable NTS for that server
if (xNTPEnableNTS(ntp, "time.cloudflare.com", NULL, 0)) {
    printf("NTS enabled for time.cloudflare.com\n");
}
```

## Broadcast API

### xNTPEnableBroadcast

Enable NTP broadcast mode reception.

```c
bool xNTPEnableBroadcast(NTPTaskHandle_t handle, uint32_t multicast_addr);
```

**Parameters:**
- `handle`: NTP task handle
- `multicast_addr`: Multicast address (0 for default 224.0.1.1)

**Returns:** true if broadcast mode enabled, false otherwise

**Example:**
```c
if (xNTPEnableBroadcast(ntp, 0)) {
    printf("Broadcast mode enabled\n");
}
```

### vNTPDisableBroadcast

Disable NTP broadcast mode reception.

```c
void vNTPDisableBroadcast(NTPTaskHandle_t handle);
```

**Parameters:**
- `handle`: NTP task handle

**Returns:** None

## Callback Functions

### vNTPRegisterTimeSetCallback

Register callback for setting system time.

```c
void vNTPRegisterTimeSetCallback(NTPTaskHandle_t handle, 
                                  NTPTimeSetCallback_t callback);
```

**Callback Type:**
```c
typedef void (*NTPTimeSetCallback_t)(uint32_t seconds, uint32_t microseconds);
```

**Parameters:**
- `handle`: NTP task handle
- `callback`: Function to call when time needs to be set

**Example:**
```c
void time_set_callback(uint32_t seconds, uint32_t microseconds) {
    // Set system time
    struct timespec ts;
    ts.tv_sec = seconds;
    ts.tv_nsec = microseconds * 1000;
    clock_settime(CLOCK_REALTIME, &ts);
}

vNTPRegisterTimeSetCallback(ntp, time_set_callback);
```

### vNTPRegisterSkewSetCallback

Register callback for adjusting time skew.

```c
void vNTPRegisterSkewSetCallback(NTPTaskHandle_t handle, 
                                  NTPSkewSetCallback_t callback);
```

**Callback Type:**
```c
typedef void (*NTPSkewSetCallback_t)(int32_t skew_us);
```

**Parameters:**
- `handle`: NTP task handle
- `callback`: Function to call for time skew adjustments

**Example:**
```c
void skew_callback(int32_t skew_us) {
    // Adjust system time by skew amount
    // This is called for small corrections after initial sync
    struct timeval delta;
    delta.tv_sec = skew_us / 1000000;
    delta.tv_usec = skew_us % 1000000;
    adjtime(&delta, NULL);
}

vNTPRegisterSkewSetCallback(ntp, skew_callback);
```

## Usage Examples

### Complete Initialization Example

```c
#include "freertos_ntp.h"

void initialize_ntp(void) {
    NTPConfig_t config;
    NTPTaskHandle_t ntp;
    
    // Get default configuration
    vNTPGetDefaultConfig(&config);
    
    // Customize configuration
    config.use_sntp_mode = false;    // Use full NTP mode
    config.use_nts = false;           // NTS enabled per-server
    config.enable_broadcast = true;   // Enable broadcast reception
    config.poll_interval = 64;        // Poll every 64 seconds
    config.timeout_ms = 5000;         // 5 second timeout
    config.task_priority = tskIDLE_PRIORITY + 2;
    config.task_stack_size = 4096;
    
    // Initialize NTP client
    ntp = xNTPClientInit(&config);
    if (ntp == NULL) {
        printf("Failed to initialize NTP client\n");
        return;
    }
    
    // Add multiple servers
    xNTPAddServer(ntp, "pool.ntp.org");
    xNTPAddServer(ntp, "time.google.com");
    xNTPAddServer(ntp, "time.cloudflare.com");
    
    // Enable NTS for Cloudflare
    xNTPEnableNTS(ntp, "time.cloudflare.com", NULL, 0);
    
    // Register callbacks
    vNTPRegisterTimeSetCallback(ntp, my_time_set_callback);
    vNTPRegisterSkewSetCallback(ntp, my_skew_callback);
    
    // Start synchronization
    if (!xNTPStart(ntp)) {
        printf("Failed to start NTP client\n");
        return;
    }
    
    printf("NTP client started successfully\n");
}
```

### Monitoring Example

```c
void monitor_ntp_status(NTPTaskHandle_t ntp) {
    char status_buffer[2048];
    NTPServerStats_t stats[16];
    
    // Get formatted status string
    ulNTPGetStatusString(ntp, status_buffer, sizeof(status_buffer));
    printf("\n%s\n", status_buffer);
    
    // Get detailed server statistics
    uint32_t count = ulNTPGetServerStats(ntp, stats, 16);
    
    printf("Detailed Statistics:\n");
    for (uint32_t i = 0; i < count; i++) {
        if (stats[i].flags & NTP_SERVER_FLAG_VALID) {
            printf("  %s:\n", stats[i].hostname);
            printf("    Stratum: %u\n", stats[i].stratum);
            printf("    Offset:  %d us\n", stats[i].offset_us);
            printf("    Delay:   %d us\n", stats[i].delay_us);
            printf("    Jitter:  %d us\n", stats[i].jitter_us);
            printf("    Reach:   0x%02x\n", stats[i].reach);
        }
    }
}
```

### SNTP Mode Example

```c
void simple_sntp_client(void) {
    NTPConfig_t config;
    NTPTaskHandle_t ntp;
    
    vNTPGetDefaultConfig(&config);
    config.use_sntp_mode = true;  // Simple SNTP mode
    config.poll_interval = 3600;  // Poll once per hour
    
    ntp = xNTPClientInit(&config);
    xNTPAddServer(ntp, "pool.ntp.org");
    vNTPRegisterTimeSetCallback(ntp, time_set_callback);
    xNTPStart(ntp);
}
```

## Error Handling

Most API functions return boolean or NULL values to indicate failure:

```c
// Check initialization
if (ntp == NULL) {
    printf("ERROR: Failed to initialize NTP client\n");
    return;
}

// Check server addition
if (!xNTPAddServer(ntp, "invalid.server")) {
    printf("WARNING: Could not add server (DNS resolution failed?)\n");
}

// Check start
if (!xNTPStart(ntp)) {
    printf("ERROR: Failed to start NTP task\n");
    return;
}

// Check NTS enabling
if (!xNTPEnableNTS(ntp, "time.cloudflare.com", NULL, 0)) {
    printf("WARNING: NTS-KE handshake failed\n");
}
```

## Thread Safety

All API functions are thread-safe and use internal mutexes for synchronization. You can safely call API functions from multiple tasks.

## Performance Considerations

- **Stack Size**: Ensure adequate stack size for NTP task (minimum 2048 bytes, recommended 4096 for NTS)
- **Poll Interval**: Default is 64 seconds; adjust based on accuracy requirements
- **Server Count**: Maximum 16 configured servers, 8 actively polled
- **Network**: Requires working FreeRTOS-Plus-TCP network stack

## Debugging

Enable debug output by defining `NTP_ENABLE_DEBUG` in `freertos_ntp_config.h`:

```c
#define NTP_ENABLE_DEBUG 1
```

This will enable debug printf statements throughout the code.
