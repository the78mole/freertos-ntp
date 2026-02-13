# FreeRTOS NTP Client - Configuration Guide

## Table of Contents
- [Basic Configuration](#basic-configuration)
- [Advanced Configuration](#advanced-configuration)
- [FreeRTOS Configuration](#freertos-configuration)
- [Network Configuration](#network-configuration)
- [Security Configuration](#security-configuration)
- [Performance Tuning](#performance-tuning)

## Basic Configuration

### Default Configuration

The simplest way to start is using default configuration:

```c
NTPConfig_t config;
vNTPGetDefaultConfig(&config);

// Defaults are:
// - use_sntp_mode = false (full NTP mode)
// - use_nts = false (NTS disabled by default)
// - enable_broadcast = false (broadcast disabled)
// - port = 123 (standard NTP port)
// - poll_interval = 64 seconds
// - timeout_ms = 5000 (5 second timeout)
// - task_priority = tskIDLE_PRIORITY + 2
// - task_stack_size = 2048 bytes
```

### Customizing Configuration

Override defaults as needed:

```c
NTPConfig_t config;
vNTPGetDefaultConfig(&config);

// Use SNTP mode for simplicity
config.use_sntp_mode = true;

// Poll more frequently
config.poll_interval = 32;

// Increase timeout for slow networks
config.timeout_ms = 10000;

// Give NTP task higher priority
config.task_priority = tskIDLE_PRIORITY + 3;
```

## Advanced Configuration

### Mode Selection

#### SNTP Mode (Simple)

Best for:
- Simple time synchronization needs
- Resource-constrained systems
- When metrics are not required

```c
config.use_sntp_mode = true;
config.poll_interval = 3600;  // Poll once per hour
```

#### Full NTP Mode (With Metrics)

Best for:
- Applications requiring high accuracy
- When monitoring server quality is important
- Multiple server environments

```c
config.use_sntp_mode = false;
config.poll_interval = 64;  // Standard NTP poll interval
```

### Server Configuration

#### Single Server

```c
xNTPAddServer(ntp, "pool.ntp.org");
```

#### Multiple Servers (Recommended)

```c
// Add primary servers
xNTPAddServer(ntp, "0.pool.ntp.org");
xNTPAddServer(ntp, "1.pool.ntp.org");
xNTPAddServer(ntp, "2.pool.ntp.org");

// Add backup servers
xNTPAddServer(ntp, "time.google.com");
xNTPAddServer(ntp, "time.cloudflare.com");
```

#### Geographic Distribution

For best results, use servers from different geographic locations:

```c
// North America
xNTPAddServer(ntp, "0.north-america.pool.ntp.org");

// Europe
xNTPAddServer(ntp, "0.europe.pool.ntp.org");

// Asia
xNTPAddServer(ntp, "0.asia.pool.ntp.org");
```

### Poll Interval Configuration

The poll interval determines how often servers are queried:

```c
// Fast updates (more network traffic)
config.poll_interval = 16;  // 16 seconds

// Standard (balanced)
config.poll_interval = 64;  // 64 seconds (default)

// Conservative (less traffic)
config.poll_interval = 1024;  // ~17 minutes

// Minimal (very infrequent)
config.poll_interval = 3600;  // 1 hour
```

**Recommendations:**
- Initial sync: 16-32 seconds for fast convergence
- Normal operation: 64-128 seconds
- Battery-powered: 600-3600 seconds

## FreeRTOS Configuration

### Required FreeRTOS Configuration

In your `FreeRTOSConfig.h`:

```c
// Enable required features
#define configUSE_PREEMPTION                    1
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             1
#define configUSE_TASK_NOTIFICATIONS            1

// Heap size (adjust based on your needs)
#define configTOTAL_HEAP_SIZE                   (64 * 1024)

// Tick rate (1ms recommended)
#define configTICK_RATE_HZ                      1000
```

### Task Priority

Choose appropriate priority based on your system:

```c
// Low priority (background synchronization)
config.task_priority = tskIDLE_PRIORITY + 1;

// Medium priority (default)
config.task_priority = tskIDLE_PRIORITY + 2;

// High priority (time-critical applications)
config.task_priority = tskIDLE_PRIORITY + 4;
```

### Stack Size

Adjust based on features used:

```c
// Minimal (SNTP only)
config.task_stack_size = 1536;

// Standard (full NTP)
config.task_stack_size = 2048;

// With NTS (requires more stack for TLS)
config.task_stack_size = 4096;

// With all features
config.task_stack_size = 6144;
```

## Network Configuration

### FreeRTOS-Plus-TCP Configuration

In your `FreeRTOSIPConfig.h`:

```c
// Enable UDP
#define ipconfigUSE_TCP                         0  // Not required for NTP

// Enable DNS
#define ipconfigUSE_DNS                         1
#define ipconfigDNS_CACHE_ENTRIES               8

// Socket buffer sizes
#define ipconfigTCP_RX_BUFFER_LENGTH            4096
#define ipconfigUDP_MAX_RX_PACKETS              8

// Enable multicast (for broadcast mode)
#define ipconfigUSE_IP_MULTICAST                1
```

### Timeout Configuration

```c
// Fast network (LAN)
config.timeout_ms = 1000;  // 1 second

// Normal network
config.timeout_ms = 5000;  // 5 seconds (default)

// Slow/unreliable network
config.timeout_ms = 10000;  // 10 seconds
```

### Port Configuration

```c
// Standard NTP port
config.port = 123;

// Custom port (if your NTP server uses non-standard port)
config.port = 8123;
```

## Security Configuration

### NTS (Network Time Security)

#### Basic NTS Setup

```c
// Add server
xNTPAddServer(ntp, "time.cloudflare.com");

// Enable NTS for that server
xNTPEnableNTS(ntp, "time.cloudflare.com", NULL, 0);
```

#### Custom NTS-KE Server

```c
// Use separate NTS-KE server
xNTPEnableNTS(ntp, "time.example.com", 
              "nts-ke.example.com", 4460);
```

#### NTS Configuration Requirements

- Increased stack size (4096+ bytes)
- TLS support in FreeRTOS-Plus-TCP
- More heap memory for TLS sessions

### Broadcast Mode Security

```c
// Enable broadcast with authentication requirement
xNTPEnableBroadcast(ntp, 0);

// In broadcast context (if you have direct access):
vNTPBroadcastSetAuthRequired(broadcast_ctx, true);
```

### Best Practices

1. **Use NTS when available** for secure time synchronization
2. **Validate server certificates** in production environments
3. **Use multiple independent servers** to prevent time manipulation
4. **Monitor server reachability** to detect issues

## Performance Tuning

### Memory Optimization

#### Reduce Server Count

```c
// Minimal configuration (1-2 servers)
xNTPAddServer(ntp, "pool.ntp.org");
xNTPAddServer(ntp, "time.google.com");
```

#### Disable Unused Features

```c
// Disable broadcast if not needed
config.enable_broadcast = false;

// Use SNTP instead of full NTP
config.use_sntp_mode = true;
```

### CPU Optimization

#### Reduce Poll Frequency

```c
// Less frequent polling = less CPU usage
config.poll_interval = 300;  // 5 minutes
```

#### Lower Task Priority

```c
// Run NTP at lower priority if not time-critical
config.task_priority = tskIDLE_PRIORITY + 1;
```

### Network Optimization

#### Batch Operations

Configure longer poll intervals to reduce network traffic:

```c
config.poll_interval = 1024;  // ~17 minutes
```

#### Limit Active Servers

The system polls up to 8 servers simultaneously. Configure fewer servers if bandwidth is limited:

```c
// Only configure 2-3 servers instead of 8
xNTPAddServer(ntp, "time.google.com");
xNTPAddServer(ntp, "time.cloudflare.com");
```

## Platform-Specific Configuration

### Embedded Systems

```c
NTPConfig_t config;
vNTPGetDefaultConfig(&config);

config.use_sntp_mode = true;       // Simpler implementation
config.poll_interval = 600;        // 10 minute intervals
config.timeout_ms = 10000;         // Longer timeout
config.task_priority = tskIDLE_PRIORITY + 1;
config.task_stack_size = 1536;     // Minimal stack
```

### IoT Devices

```c
NTPConfig_t config;
vNTPGetDefaultConfig(&config);

config.use_sntp_mode = false;      // Full NTP for better accuracy
config.enable_broadcast = true;     // Support local time server
config.poll_interval = 64;
config.timeout_ms = 5000;
config.task_priority = tskIDLE_PRIORITY + 2;
config.task_stack_size = 2048;
```

### High-Accuracy Applications

```c
NTPConfig_t config;
vNTPGetDefaultConfig(&config);

config.use_sntp_mode = false;      // Full NTP with metrics
config.use_nts = true;             // Enable security
config.poll_interval = 16;         // Fast updates
config.timeout_ms = 2000;          // Quick timeout
config.task_priority = tskIDLE_PRIORITY + 4;  // High priority
config.task_stack_size = 6144;     // Large stack for NTS
```

## Compile-Time Configuration

Edit `include/freertos_ntp_config.h`:

```c
/* Maximum number of servers (adjust if you need more/less) */
#define NTP_MAX_SERVERS 8

/* Maximum active servers */
#define NTP_MAX_ACTIVE_SERVERS 4

/* Default values (can be overridden at runtime) */
#define NTP_DEFAULT_POLL_INTERVAL 64
#define NTP_DEFAULT_TIMEOUT_MS 5000

/* Enable debug output */
#define NTP_ENABLE_DEBUG 1
```

## Example Configurations

### Minimal Configuration (SNTP)

```c
NTPConfig_t config;
vNTPGetDefaultConfig(&config);
config.use_sntp_mode = true;
config.poll_interval = 3600;
config.task_stack_size = 1536;

NTPTaskHandle_t ntp = xNTPClientInit(&config);
xNTPAddServer(ntp, "pool.ntp.org");
vNTPRegisterTimeSetCallback(ntp, my_time_callback);
xNTPStart(ntp);
```

### Standard Configuration (Full NTP)

```c
NTPConfig_t config;
vNTPGetDefaultConfig(&config);
config.use_sntp_mode = false;
config.poll_interval = 64;
config.task_stack_size = 2048;

NTPTaskHandle_t ntp = xNTPClientInit(&config);
xNTPAddServer(ntp, "0.pool.ntp.org");
xNTPAddServer(ntp, "1.pool.ntp.org");
xNTPAddServer(ntp, "time.google.com");

vNTPRegisterTimeSetCallback(ntp, my_time_callback);
vNTPRegisterSkewSetCallback(ntp, my_skew_callback);
xNTPStart(ntp);
```

### Secure Configuration (NTS)

```c
NTPConfig_t config;
vNTPGetDefaultConfig(&config);
config.use_sntp_mode = false;
config.poll_interval = 64;
config.task_stack_size = 4096;

NTPTaskHandle_t ntp = xNTPClientInit(&config);
xNTPAddServer(ntp, "time.cloudflare.com");
xNTPEnableNTS(ntp, "time.cloudflare.com", NULL, 0);

vNTPRegisterTimeSetCallback(ntp, my_time_callback);
vNTPRegisterSkewSetCallback(ntp, my_skew_callback);
xNTPStart(ntp);
```

## Troubleshooting

### Common Issues

1. **DNS Resolution Failure**
   - Ensure DNS is configured in FreeRTOS-Plus-TCP
   - Try using IP addresses instead of hostnames
   - Check network connectivity

2. **Timeout Issues**
   - Increase `timeout_ms`
   - Check firewall rules (UDP port 123)
   - Verify network latency

3. **Stack Overflow**
   - Increase `task_stack_size`
   - Especially important when using NTS

4. **High CPU Usage**
   - Increase `poll_interval`
   - Reduce number of active servers
   - Lower task priority

5. **Memory Issues**
   - Reduce `NTP_MAX_SERVERS` in config
   - Use SNTP mode instead of full NTP
   - Increase heap size in FreeRTOSConfig.h
