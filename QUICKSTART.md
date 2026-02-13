# FreeRTOS NTP - Quick Reference

## Installation

```c
#include "freertos_ntp.h"
```

Link against: `libfreertos_ntp.a`

## Basic Setup (5 steps)

```c
// 1. Get default config
NTPConfig_t config;
vNTPGetDefaultConfig(&config);

// 2. Initialize
NTPTaskHandle_t ntp = xNTPClientInit(&config);

// 3. Add servers
xNTPAddServer(ntp, "pool.ntp.org");

// 4. Register callback
vNTPRegisterTimeSetCallback(ntp, my_time_callback);

// 5. Start
xNTPStart(ntp);
```

## Configuration Presets

### Minimal (SNTP)
```c
config.use_sntp_mode = true;
config.poll_interval = 3600;
config.task_stack_size = 1536;
```

### Standard (NTP)
```c
config.use_sntp_mode = false;
config.poll_interval = 64;
config.task_stack_size = 2048;
```

### Secure (NTS)
```c
config.use_sntp_mode = false;
config.task_stack_size = 4096;
// Then: xNTPEnableNTS(ntp, server, NULL, 0);
```

### Broadcast
```c
config.enable_broadcast = true;
```

## Common Servers

```c
// Public pools
xNTPAddServer(ntp, "pool.ntp.org");
xNTPAddServer(ntp, "time.nist.gov");

// Major providers
xNTPAddServer(ntp, "time.google.com");
xNTPAddServer(ntp, "time.cloudflare.com");
xNTPAddServer(ntp, "time.apple.com");

// Regional
xNTPAddServer(ntp, "0.north-america.pool.ntp.org");
xNTPAddServer(ntp, "0.europe.pool.ntp.org");
```

## API Quick Reference

### Initialization
```c
void vNTPGetDefaultConfig(NTPConfig_t *config);
NTPTaskHandle_t xNTPClientInit(const NTPConfig_t *config);
```

### Server Management
```c
bool xNTPAddServer(NTPTaskHandle_t handle, const char *hostname);
bool xNTPRemoveServer(NTPTaskHandle_t handle, const char *hostname);
```

### Control
```c
bool xNTPStart(NTPTaskHandle_t handle);
void vNTPStop(NTPTaskHandle_t handle);
```

### Status
```c
uint32_t ulNTPGetServerStats(NTPTaskHandle_t handle, 
                              NTPServerStats_t *stats, 
                              uint32_t max_servers);
                              
uint32_t ulNTPGetStatusString(NTPTaskHandle_t handle, 
                               char *buffer, 
                               uint32_t buffer_size);
```

### Callbacks
```c
void vNTPRegisterTimeSetCallback(NTPTaskHandle_t handle, 
                                  NTPTimeSetCallback_t callback);
                                  
void vNTPRegisterSkewSetCallback(NTPTaskHandle_t handle, 
                                  NTPSkewSetCallback_t callback);
```

### NTS
```c
bool xNTPEnableNTS(NTPTaskHandle_t handle, 
                   const char *hostname,
                   const char *nts_ke_server, 
                   uint16_t nts_ke_port);
```

### Broadcast
```c
bool xNTPEnableBroadcast(NTPTaskHandle_t handle, 
                         uint32_t multicast_addr);
                         
void vNTPDisableBroadcast(NTPTaskHandle_t handle);
```

## Callback Signatures

```c
typedef void (*NTPTimeSetCallback_t)(uint32_t seconds, 
                                     uint32_t microseconds);
                                     
typedef void (*NTPSkewSetCallback_t)(int32_t skew_us);
```

## Status Output Format

```
     remote           refid      st t when poll reach   delay   offset  jitter
==============================================================================
*pool.ntp.org     132.163.96.1    2 u   45   64  377     15      -2       1
```

- `*` = Selected server
- `+` = Valid server  
- `-` = Unreachable
- `t`: `u`=unicast, `s`=NTS, `b`=broadcast

## Error Handling

```c
// Check return values
if (ntp == NULL) {
    // Init failed
}

if (!xNTPAddServer(ntp, hostname)) {
    // DNS resolution failed
}

if (!xNTPStart(ntp)) {
    // Task creation failed
}
```

## Configuration Values

| Parameter | Min | Default | Max | Unit |
|-----------|-----|---------|-----|------|
| poll_interval | 1 | 64 | 3600 | seconds |
| timeout_ms | 100 | 5000 | 30000 | ms |
| task_priority | 0 | idle+2 | max | - |
| task_stack_size | 1536 | 2048 | 8192 | bytes |

## Memory Usage

| Mode | RAM | Stack | Code |
|------|-----|-------|------|
| SNTP | ~2 KB | 1.5 KB | ~4 KB |
| NTP | ~4 KB | 2 KB | ~8 KB |
| NTS | ~6 KB | 4+ KB | ~12 KB |
| Broadcast | +2 KB | +1 KB | +3 KB |

## Build Options

### CMake
```cmake
add_subdirectory(freertos-ntp)
target_link_libraries(myapp freertos_ntp)
```

### Makefile
```make
CFLAGS += -I/path/to/freertos-ntp/include
LDFLAGS += -L/path/to/freertos-ntp/lib -lfreertos_ntp
```

## Troubleshooting

| Problem | Solution |
|---------|----------|
| DNS fails | Check ipconfigUSE_DNS = 1 |
| Timeout | Increase timeout_ms |
| Stack overflow | Increase task_stack_size |
| High CPU | Increase poll_interval |
| No NTS | Increase stack to 4096+ |

## Best Practices

1. **Use multiple servers** (3-5 recommended)
2. **Enable NTS** for security
3. **Monitor status** regularly
4. **Start after network** is ready
5. **Register callbacks** before starting
6. **Use appropriate poll interval** (64s default)
7. **Allocate enough stack** (2KB minimum)

## Example: Complete Setup

```c
#include "freertos_ntp.h"
#include <stdio.h>

static NTPTaskHandle_t ntp_handle;

void time_callback(uint32_t sec, uint32_t us) {
    printf("Time set: %u.%06u\n", sec, us);
    // Set system time here
}

void skew_callback(int32_t skew_us) {
    printf("Time adjusted: %d us\n", skew_us);
    // Adjust system time here
}

void setup_ntp(void) {
    NTPConfig_t config;
    
    // Configure
    vNTPGetDefaultConfig(&config);
    config.use_sntp_mode = false;
    config.poll_interval = 64;
    
    // Initialize
    ntp_handle = xNTPClientInit(&config);
    if (!ntp_handle) return;
    
    // Add servers
    xNTPAddServer(ntp_handle, "pool.ntp.org");
    xNTPAddServer(ntp_handle, "time.google.com");
    xNTPAddServer(ntp_handle, "time.cloudflare.com");
    
    // Enable NTS for Cloudflare
    xNTPEnableNTS(ntp_handle, "time.cloudflare.com", NULL, 0);
    
    // Register callbacks
    vNTPRegisterTimeSetCallback(ntp_handle, time_callback);
    vNTPRegisterSkewSetCallback(ntp_handle, skew_callback);
    
    // Start
    xNTPStart(ntp_handle);
}

void show_ntp_status(void) {
    char buffer[2048];
    ulNTPGetStatusString(ntp_handle, buffer, sizeof(buffer));
    printf("%s\n", buffer);
}
```

## Resources

- Full docs: `README.md`
- API reference: `API.md`
- Configuration: `CONFIGURATION.md`
- Examples: `examples/x86_test/`

## License

MIT - Free for commercial use
