# FreeRTOS NTP Client

A comprehensive NTP client implementation for FreeRTOS with support for:
- **SNTP** (Simple Network Time Protocol) mode
- **Full NTP** client with metrics (offset, delay, jitter)
- **NTS** (Network Time Security) for encrypted time synchronization
- **Broadcast mode** for receiving NTP broadcasts
- **Multiple servers** (up to 16 configured, up to 8 actively polled)
- **Status reporting** similar to `ntpq -p`
- **Callbacks** for time and skew adjustments
- **x86 test environment** for development and testing

## Features

### Protocol Support
- **SNTP Mode**: Simple time synchronization without metrics
- **NTP Mode**: Full NTP implementation with offset, delay, and jitter calculation
- **NTS Mode**: Secure NTP with Network Time Security (RFC 8915)
- **Broadcast Mode**: Receive time updates from NTP broadcast servers

### Metrics & Monitoring
- Per-server statistics (delay, offset, jitter)
- Server reachability tracking
- Automatic best server selection
- Status output compatible with `ntpq -p` format

### Compatibility
- FreeRTOS versions: 10.4.6 to 11.2.0 and main branch
- FreeRTOS-Plus-TCP versions: 4.0.0 to 4.3.4 and main branch

## Quick Start

### Basic Configuration

```c
#include "freertos_ntp.h"

NTPConfig_t config;
NTPTaskHandle_t ntp_handle;

/* Get default configuration */
vNTPGetDefaultConfig(&config);

/* Customize if needed */
config.use_sntp_mode = false;  /* Use full NTP mode */
config.poll_interval = 64;     /* Poll every 64 seconds */

/* Initialize NTP client */
ntp_handle = xNTPClientInit(&config);

/* Add NTP servers */
xNTPAddServer(ntp_handle, "pool.ntp.org");
xNTPAddServer(ntp_handle, "time.google.com");

/* Register callbacks */
vNTPRegisterTimeSetCallback(ntp_handle, my_time_set_callback);
vNTPRegisterSkewSetCallback(ntp_handle, my_skew_callback);

/* Start synchronization */
xNTPStart(ntp_handle);
```

### Using NTS (Network Time Security)

```c
/* Add server and enable NTS */
xNTPAddServer(ntp_handle, "time.cloudflare.com");
xNTPEnableNTS(ntp_handle, "time.cloudflare.com", NULL, 0);
```

### Using Broadcast Mode

```c
/* Enable broadcast reception */
config.enable_broadcast = true;  /* Enable at initialization */
/* Or enable later */
xNTPEnableBroadcast(ntp_handle, 0);  /* Use default multicast address */
```

### Getting Status Information

```c
char status_buffer[2048];
uint32_t len = ulNTPGetStatusString(ntp_handle, status_buffer, sizeof(status_buffer));
printf("%s", status_buffer);
```

Output example:
```
     remote           refid      st t when poll reach   delay   offset  jitter
==============================================================================
*pool.ntp.org     132.163.96.1    2 u   45   64  377     15      -2       1
+time.google.com  142.251.10.46   1 u   32   64  377     28       3       2
+time.cloudflare  10.2.3.4        1 s   18   64  377     12      -1       1
```

Legend:
- `*` = Currently selected server
- `+` = Reachable and valid server
- `-` = Unreachable server
- `t` column: `u`=unicast, `s`=NTS, `b`=broadcast

## API Reference

### Initialization

```c
NTPTaskHandle_t xNTPClientInit(const NTPConfig_t *config);
void vNTPGetDefaultConfig(NTPConfig_t *config);
```

### Server Management

```c
bool xNTPAddServer(NTPTaskHandle_t handle, const char *hostname);
bool xNTPRemoveServer(NTPTaskHandle_t handle, const char *hostname);
```

### NTS (Network Time Security)

```c
bool xNTPEnableNTS(NTPTaskHandle_t handle, const char *hostname, 
                   const char *nts_ke_server, uint16_t nts_ke_port);
```

### Broadcast Mode

```c
bool xNTPEnableBroadcast(NTPTaskHandle_t handle, uint32_t multicast_addr);
void vNTPDisableBroadcast(NTPTaskHandle_t handle);
```

### Callbacks

```c
void vNTPRegisterTimeSetCallback(NTPTaskHandle_t handle, NTPTimeSetCallback_t callback);
void vNTPRegisterSkewSetCallback(NTPTaskHandle_t handle, NTPSkewSetCallback_t callback);
```

Callback types:
```c
typedef void (*NTPTimeSetCallback_t)(uint32_t seconds, uint32_t microseconds);
typedef void (*NTPSkewSetCallback_t)(int32_t skew_us);
```

### Status & Control

```c
bool xNTPStart(NTPTaskHandle_t handle);
void vNTPStop(NTPTaskHandle_t handle);
uint32_t ulNTPGetServerStats(NTPTaskHandle_t handle, NTPServerStats_t *stats, uint32_t max_servers);
uint32_t ulNTPGetStatusString(NTPTaskHandle_t handle, char *buffer, uint32_t buffer_size);
```

## Configuration Options

The `NTPConfig_t` structure supports the following options:

```c
typedef struct {
    bool     use_sntp_mode;      /* Use simple SNTP mode (no metrics) */
    bool     use_nts;            /* Use NTS (Network Time Security) */
    bool     enable_broadcast;   /* Enable broadcast mode reception */
    uint16_t port;               /* NTP server port (default 123) */
    uint32_t poll_interval;      /* Poll interval in seconds */
    uint32_t timeout_ms;         /* Response timeout in milliseconds */
    uint16_t task_priority;      /* FreeRTOS task priority */
    uint32_t task_stack_size;    /* FreeRTOS task stack size */
} NTPConfig_t;
```

## Building

### Source Files

Include these files in your project:
- `source/freertos_ntp.c` - Main NTP client implementation
- `source/freertos_ntp_nts.c` - NTS support
- `source/freertos_ntp_broadcast.c` - Broadcast mode support

### Header Files

Add `include/` directory to your include path:
- `include/freertos_ntp.h` - Main API
- `include/freertos_ntp_nts.h` - NTS API
- `include/freertos_ntp_broadcast.h` - Broadcast API
- `include/freertos_ntp_config.h` - Configuration options

### Dependencies

- FreeRTOS kernel (10.4.6+)
- FreeRTOS-Plus-TCP (4.0.0+)
- Standard C library (string.h, stdio.h, math.h)

## Example Application

See `examples/x86_test/` for a complete x86 example that demonstrates:
- Basic NTP client setup
- Multiple server configuration
- Callback implementation
- Status monitoring

## License

MIT License - See LICENSE file for details

## Contributing

Contributions are welcome! Please ensure:
- Code follows existing style
- Changes are tested with x86 example
- Documentation is updated

## Support

For issues and questions, please use the GitHub issue tracker.
