# Simple Network Time Protocol (SNTP) - Skill Context

## Overview

Simple Network Time Protocol (SNTP) is a simplified version of NTP designed for clients that do not require the full complexity of NTP. SNTP is ideal for embedded systems, IoT devices, and applications that need basic time synchronization without sophisticated clock discipline algorithms.

## Key Specifications

- **RFC 4330**: Simple Network Time Protocol (SNTP) Version 4 for IPv4, IPv6 and OSI
- **RFC 5905**: Network Time Protocol Version 4 (SNTP is a subset)
- **RFC 2030**: Simple Network Time Protocol (SNTP) Version 4 (obsoleted by RFC 4330)

## SNTP vs NTP Comparison

| Feature | SNTP | NTP |
|---------|------|-----|
| **Purpose** | Client-only time sync | Full client/server with discipline |
| **Complexity** | Simple, minimal state | Complex with clock discipline |
| **Code Size** | ~2-5 KB | ~50-100 KB |
| **Clock Discipline** | None (application handles) | Built-in PLL/FLL |
| **Server Selection** | Manual or simple | Sophisticated algorithm |
| **Frequency Correction** | No | Yes |
| **Drift Compensation** | No | Yes |
| **Multiple Servers** | Optional | Recommended |
| **Typical Accuracy** | 10-100 ms | 1-10 ms (LAN) |
| **Best For** | Embedded, IoT, clients | Servers, high-precision needs |

## SNTP Operating Mode

SNTP clients operate in **unicast mode** (Mode 3):
1. Client sends request to server
2. Server responds with time information
3. Client calculates offset and adjusts local clock
4. Process repeats at configured interval

## SNTP Packet Structure

SNTP uses the same 48-byte packet structure as NTP:

```c
typedef struct __attribute__((packed)) {
    uint8_t  li_vn_mode;      // Leap Indicator (2), Version (3), Mode (3)
    uint8_t  stratum;         // Stratum level
    uint8_t  poll;            // Poll interval
    int8_t   precision;       // Clock precision
    uint32_t root_delay;      // Round-trip delay to reference
    uint32_t root_dispersion; // Nominal error
    uint32_t ref_id;          // Reference clock identifier
    uint64_t ref_timestamp;   // Last clock update
    uint64_t orig_timestamp;  // Client timestamp (T1)
    uint64_t recv_timestamp;  // Server receive time (T2)
    uint64_t xmit_timestamp;  // Server transmit time (T3)
} SNTPPacket_t;
```

## SNTP Request (Client → Server)

```c
// Prepare SNTP request packet
SNTPPacket_t request = {0};

// Set LI (0), Version (4), Mode (3 = client)
request.li_vn_mode = (0 << 6) | (4 << 3) | 3;

// Set transmit timestamp (T1)
request.xmit_timestamp = getCurrentNTPTimestamp();

// Send to server on UDP port 123
sendto(socket, &request, sizeof(request), ...);
```

## SNTP Response Processing

```c
// Receive response from server
SNTPPacket_t response;
recvfrom(socket, &response, sizeof(response), ...);

// Extract timestamps
uint64_t T1 = request.xmit_timestamp;   // Client send time
uint64_t T2 = response.recv_timestamp;  // Server receive time
uint64_t T3 = response.xmit_timestamp;  // Server send time
uint64_t T4 = getCurrentNTPTimestamp(); // Client receive time

// Calculate offset and delay
int64_t offset = ((T2 - T1) + (T3 - T4)) / 2;
int64_t delay = (T4 - T1) - (T3 - T2);

// Apply offset to local clock
adjustClock(offset);
```

## Simplified Time Calculation

For basic SNTP implementations, you can use simplified calculation:

```c
// Simple offset (ignoring network delay)
int64_t offset = T3 - T4;

// Or average of server times
uint64_t server_time = (T2 + T3) / 2;
```

## SNTP Client Implementation Pattern

```c
// SNTP client task
void vSNTPClientTask(void *pvParameters)
{
    TickType_t xLastSyncTime = 0;
    const TickType_t xSyncInterval = pdMS_TO_TICKS(60000); // 60 seconds
    
    while (1) {
        // Wait for sync interval
        vTaskDelayUntil(&xLastSyncTime, xSyncInterval);
        
        // Send SNTP request
        if (sendSNTPRequest(server_address) == SUCCESS) {
            // Wait for response (with timeout)
            if (receiveSNTPResponse(&response, 5000) == SUCCESS) {
                // Calculate offset
                int64_t offset = calculateOffset(&response);
                
                // Update system time
                if (abs(offset) > STEP_THRESHOLD) {
                    // Large offset: step clock
                    stepClock(offset);
                } else {
                    // Small offset: gradual adjustment
                    slewClock(offset);
                }
            }
        }
    }
}
```

## SNTP Client Configuration

```c
typedef struct {
    const char *server_address;   // NTP server hostname or IP
    uint16_t    server_port;      // Usually 123
    uint32_t    poll_interval_sec; // Sync interval (e.g., 60-3600)
    uint32_t    timeout_ms;       // Response timeout (e.g., 5000)
    uint32_t    step_threshold_us; // Threshold for step vs slew
    uint8_t     max_retries;      // Retry attempts
    bool        use_pool;         // Use NTP pool rotation
} SNTPConfig_t;
```

## Validation Checks

SNTP clients should perform basic sanity checks:

```c
bool validateSNTPResponse(SNTPPacket_t *response, SNTPPacket_t *request)
{
    // Check version (should be 3 or 4)
    uint8_t version = (response->li_vn_mode >> 3) & 0x07;
    if (version < 3 || version > 4) return false;
    
    // Check mode (should be 4 = server)
    uint8_t mode = response->li_vn_mode & 0x07;
    if (mode != 4) return false;
    
    // Check stratum (1-15, not 0 or 16)
    if (response->stratum == 0 || response->stratum > 15) return false;
    
    // Check leap indicator (should not be 3 = unsync)
    uint8_t leap = (response->li_vn_mode >> 6) & 0x03;
    if (leap == 3) return false;
    
    // Verify origin timestamp matches our transmit timestamp
    if (response->orig_timestamp != request->xmit_timestamp) return false;
    
    // Check that transmit timestamp is not zero
    if (response->xmit_timestamp == 0) return false;
    
    return true;
}
```

## Clock Adjustment Strategies

### 1. Step (Immediate)
For large offsets (>128ms), immediately set the clock:

```c
void stepClock(int64_t offset_us)
{
    uint32_t seconds = ulSystemSeconds;
    uint32_t microseconds = ulSystemMicroseconds;
    
    // Add offset
    int64_t total_us = (int64_t)microseconds + offset_us;
    seconds += (uint32_t)(total_us / 1000000);
    microseconds = (uint32_t)(total_us % 1000000);
    
    setSystemTime(seconds, microseconds);
}
```

### 2. Slew (Gradual)
For small offsets (<128ms), adjust gradually:

```c
void slewClock(int64_t offset_us)
{
    // Adjust clock over time (e.g., 500 ppm = 0.5 ms/s)
    uint32_t adjustment_rate = 500; // ppm
    setClockAdjustmentRate(offset_us, adjustment_rate);
}
```

### 3. Ignore
For very small offsets (<1ms), may ignore:

```c
if (abs(offset_us) < 1000) {
    // Offset too small to matter
    return;
}
```

## Error Handling

```c
typedef enum {
    SNTP_SUCCESS = 0,
    SNTP_ERROR_NETWORK,      // Network/socket error
    SNTP_ERROR_TIMEOUT,      // No response from server
    SNTP_ERROR_INVALID_PACKET, // Malformed packet
    SNTP_ERROR_STRATUM,      // Invalid stratum
    SNTP_ERROR_LEAP_ALARM,   // Leap indicator = 3
    SNTP_ERROR_ORIGIN,       // Origin timestamp mismatch
    SNTP_ERROR_ZERO_XMIT,    // Zero transmit timestamp
} SNTPError_t;
```

## SNTP for Embedded Systems

### Memory Footprint

Minimal SNTP implementation:
- **Code**: ~2-5 KB
- **RAM**: ~100-200 bytes (packet buffer + state)
- **Stack**: ~1-2 KB per task

### Resource Requirements

```c
// Minimal resource allocation
#define SNTP_TASK_STACK_SIZE    1024  // words
#define SNTP_TASK_PRIORITY      (tskIDLE_PRIORITY + 2)
#define SNTP_PACKET_SIZE        48     // bytes
#define SNTP_TIMEOUT_MS         5000   // milliseconds
#define SNTP_RETRY_COUNT        3      // attempts
```

### Power Optimization

```c
// Power-aware SNTP sync
void vSNTPLowPowerTask(void *pvParameters)
{
    while (1) {
        // Wake up network interface
        wakeNetworkInterface();
        
        // Quick sync
        if (syncSNTPTime() == SUCCESS) {
            logTime("SNTP sync successful");
        }
        
        // Sleep network interface
        sleepNetworkInterface();
        
        // Long sleep interval for battery devices
        vTaskDelay(pdMS_TO_TICKS(3600000)); // 1 hour
    }
}
```

## Common SNTP Servers

```c
// Popular SNTP servers
const char *sntp_servers[] = {
    "time.google.com",       // Google NTP (anycast)
    "time.cloudflare.com",   // Cloudflare NTP
    "pool.ntp.org",          // NTP Pool Project
    "time.nist.gov",         // NIST
    "time.windows.com",      // Microsoft
};
```

## IPv4 vs IPv6

```c
// IPv4 example: 216.239.35.0 (time.google.com)
struct sockaddr_in server_addr = {
    .sin_family = AF_INET,
    .sin_port = htons(123),
    .sin_addr.s_addr = inet_addr("216.239.35.0"),
};

// IPv6 example: 2001:4860:4806::
struct sockaddr_in6 server_addr6 = {
    .sin6_family = AF_INET6,
    .sin6_port = htons(123),
    // .sin6_addr = ... (IPv6 address)
};
```

## Broadcast Mode

SNTP also supports broadcast mode (Mode 5) for one-to-many:

```c
// Broadcast client receives periodic broadcasts
void vSNTPBroadcastClientTask(void *pvParameters)
{
    // Bind to UDP port 123
    bindSocket(socket, 123);
    
    while (1) {
        // Wait for broadcast packet
        if (recvfrom(socket, &packet, ...) > 0) {
            // Validate broadcast packet
            if (validateBroadcastPacket(&packet)) {
                // Apply time update
                updateSystemTime(&packet);
            }
        }
    }
}
```

## Testing SNTP Implementation

```bash
# Test SNTP query with ntpdate
ntpdate -q time.google.com

# Test with custom SNTP client
sntp -d time.google.com

# Capture SNTP packets
tcpdump -i any -n port 123

# Test with chrony
chronyc tracking
chronyc sources -v
```

## Best Practices for SNTP

1. **Server Selection**: Use reliable, nearby servers
2. **Polling Interval**: 60-3600 seconds (respect server load)
3. **Timeout**: 5-10 seconds
4. **Retries**: 2-3 attempts before failover
5. **Validation**: Always validate responses
6. **Multiple Servers**: Use 2-3 for redundancy
7. **Leap Seconds**: Handle gracefully
8. **Boot Time**: Sync immediately after network is ready
9. **Persistence**: Save last good time across reboots
10. **Monitoring**: Log sync success/failure for debugging

## When to Use SNTP vs NTP

### Use SNTP when:
- ✅ Embedded system with limited resources
- ✅ Simple client-only application
- ✅ Accuracy requirement > 10ms
- ✅ Infrequent synchronization (minutes/hours)
- ✅ Code size is critical
- ✅ Single time source is acceptable

### Use Full NTP when:
- ✅ Need high accuracy (< 10ms)
- ✅ Continuous synchronization required
- ✅ Multiple server selection needed
- ✅ Clock discipline is important
- ✅ Frequency drift compensation needed
- ✅ Server functionality required

## Implementation Notes for This Project

This FreeRTOS NTP library provides SNTP mode through:
- Configuration flag: `use_sntp_mode = true`
- Simplified packet handling
- No clock discipline algorithm
- Direct time setting via callbacks
- Minimal memory footprint option
- Single-server or multi-server support

## References

- RFC 4330: https://www.rfc-editor.org/rfc/rfc4330
- RFC 5905: https://www.rfc-editor.org/rfc/rfc5905
- NTP.org: https://www.ntp.org/
- SNTP Wikipedia: https://en.wikipedia.org/wiki/Network_Time_Protocol#SNTP
