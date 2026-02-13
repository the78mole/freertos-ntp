# Network Time Protocol (NTP) - Skill Context

## Overview

Network Time Protocol (NTP) is a networking protocol for clock synchronization between computer systems over packet-switched, variable-latency data networks. NTP is one of the oldest Internet protocols still in use.

## Key Specifications

- **RFC 5905**: Network Time Protocol Version 4: Protocol and Algorithms Specification
- **RFC 5906**: Network Time Protocol Version 4: Autokey Specification
- **RFC 5907**: Definitions of Managed Objects for Network Time Protocol Version 4
- **RFC 5908**: Network Time Protocol (NTP) Server Option for DHCPv6

## Protocol Fundamentals

### NTP Packet Structure

NTP uses a 48-byte packet structure:

```c
typedef struct {
    uint8_t  li_vn_mode;      // Leap Indicator, Version, Mode
    uint8_t  stratum;         // Stratum level (0-15)
    uint8_t  poll;            // Poll interval (log2 seconds)
    int8_t   precision;       // Precision (log2 seconds)
    uint32_t root_delay;      // Root delay (NTP short format)
    uint32_t root_dispersion; // Root dispersion (NTP short format)
    uint32_t ref_id;          // Reference identifier
    uint64_t ref_timestamp;   // Reference timestamp
    uint64_t orig_timestamp;  // Origin timestamp (T1)
    uint64_t recv_timestamp;  // Receive timestamp (T2)
    uint64_t xmit_timestamp;  // Transmit timestamp (T3)
} NTPPacket_t;
```

### NTP Timestamp Format

NTP uses a 64-bit timestamp:
- **Upper 32 bits**: Seconds since January 1, 1900 00:00:00 UTC (NTP epoch)
- **Lower 32 bits**: Fractional seconds (resolution ~233 picoseconds)

```c
// Convert NTP timestamp to seconds and microseconds
uint32_t seconds = (uint32_t)(ntp_timestamp >> 32);
uint32_t fraction = (uint32_t)(ntp_timestamp & 0xFFFFFFFF);
uint32_t microseconds = (uint32_t)((fraction * 1000000ULL) >> 32);
```

### Time Calculation Algorithm

NTP uses a four-timestamp exchange to calculate offset and delay:

```
Client→Server: T1 (origin timestamp)
Server←Client: T2 (receive timestamp)
Server→Client: T3 (transmit timestamp)
Client←Server: T4 (destination timestamp)

Offset = ((T2 - T1) + (T3 - T4)) / 2
Delay = (T4 - T1) - (T3 - T2)
```

## Operating Modes

### 1. Client Mode (Mode 3)
Client sends request to server and waits for response.

### 2. Server Mode (Mode 4)
Server responds to client requests with time information.

### 3. Symmetric Active (Mode 1)
Peer initiates time synchronization with another peer.

### 4. Symmetric Passive (Mode 2)
Peer responds to symmetric active requests.

### 5. Broadcast Mode (Mode 5)
Server broadcasts time to all clients on subnet.

### 6. Broadcast Client Mode (Mode 6)
Client receives broadcast time updates.

## Stratum Levels

- **Stratum 0**: Reference clocks (GPS, atomic clocks)
- **Stratum 1**: Primary servers directly connected to Stratum 0
- **Stratum 2**: Secondary servers synchronized to Stratum 1
- **Stratum 3-15**: Higher-level servers
- **Stratum 16**: Unsynchronized

## Poll Intervals

NTP uses logarithmic poll intervals (2^n seconds):
- **Minimum**: 2^4 = 16 seconds (typical minimum)
- **Maximum**: 2^17 = 131072 seconds (~36 hours)
- **Default**: 2^6 = 64 seconds

## Leap Second Handling

Leap indicators in NTP packet:
- **0**: No warning
- **1**: Last minute of day has 61 seconds
- **2**: Last minute of day has 59 seconds
- **3**: Clock not synchronized

## Clock Discipline Algorithm

NTP uses a complex clock discipline algorithm:

1. **Clock Filter**: Selects best samples from multiple measurements
2. **Clock Select**: Chooses best servers from multiple sources
3. **Clock Combine**: Combines multiple server offsets
4. **Clock Adjust**: Applies correction using PLL/FLL

### Phase-Locked Loop (PLL)

For small frequency errors and stable conditions:
```
frequency_adjustment = offset × gain_factor
```

### Frequency-Locked Loop (FLL)

For large frequency errors or unstable conditions:
```
frequency_adjustment = (offset_now - offset_prev) / time_interval
```

## Implementation Considerations for FreeRTOS

### 1. Memory Management
- Use static buffers for NTP packets (48 bytes)
- Minimize dynamic allocations
- Consider memory pool for multiple server tracking

### 2. Task Priority
- NTP task should run at moderate priority
- Time-critical callbacks may need higher priority
- Balance with network stack priority

### 3. Network Stack Integration
- Requires UDP socket support
- Handle network timeouts gracefully
- Implement retry logic with exponential backoff

### 4. Time Representation
- Store time as seconds + microseconds
- Use 64-bit integers to avoid overflow
- Handle NTP epoch (1900) vs Unix epoch (1970) conversion

### 5. Clock Synchronization
- Step clock for large offsets (> 128ms)
- Slew clock for small offsets (< 128ms)
- Protect against time going backwards

## Security Considerations

### Attacks to Prevent

1. **Replay Attacks**: Use timestamps and sequence numbers
2. **Man-in-the-Middle**: Verify server authenticity (use NTS)
3. **Denial of Service**: Rate limiting, source verification
4. **Time Shifting**: Multiple server validation, outlier detection

### Best Practices

- Use multiple independent time sources (3-5 servers)
- Implement sanity checks on received time
- Monitor server reachability and quality
- Use authenticated NTP (Autokey or NTS) when possible
- Validate stratum levels and leap indicators

## NTP vs SNTP

| Feature | NTP | SNTP |
|---------|-----|------|
| Complexity | Full protocol | Simplified subset |
| Clock Discipline | Yes | No |
| Frequency Correction | Yes | No |
| Multiple Servers | Yes | Optional |
| Typical Use | Servers, high accuracy | Embedded devices, clients |
| Code Size | Large (~100KB) | Small (~5KB) |

## Common Public NTP Servers

```c
// NTP Pool Project (load-balanced)
"pool.ntp.org"
"0.pool.ntp.org"
"1.pool.ntp.org"

// Regional pools
"europe.pool.ntp.org"
"north-america.pool.ntp.org"
"asia.pool.ntp.org"

// Public time servers
"time.google.com"      // Google (anycast)
"time.cloudflare.com"  // Cloudflare
"time.nist.gov"        // NIST
"time.windows.com"     // Microsoft
```

## Quality Metrics

### Root Delay
Total round-trip delay to reference clock (in seconds).

### Root Dispersion
Maximum error relative to reference clock (in seconds).

### Jitter
Short-term variability in offset measurements.

### Reachability
8-bit register tracking successful polls (bit-shifted each poll).

## Error Handling

```c
// Common NTP error conditions
#define NTP_ERROR_TIMEOUT        1  // Server did not respond
#define NTP_ERROR_INVALID_PACKET 2  // Malformed packet
#define NTP_ERROR_STRATUM_HIGH   3  // Stratum too high (unsync)
#define NTP_ERROR_LI_ALARM       4  // Leap indicator = 3 (alarm)
#define NTP_ERROR_ORIGIN_MISMATCH 5 // Origin timestamp mismatch
#define NTP_ERROR_ROOT_DELAY_HIGH 6 // Root delay too high
```

## Testing and Validation

### Test Scenarios

1. **Normal Operation**: Regular synchronization with stable servers
2. **Network Loss**: Handle timeout and retry
3. **Server Failure**: Failover to alternate servers
4. **Large Offset**: Step clock when offset > threshold
5. **Small Offset**: Slew clock gradually
6. **Leap Second**: Handle leap second insertion/deletion
7. **Stratum Changes**: React to server stratum changes

### Validation Tools

```bash
# Query NTP server
ntpdate -q [server]

# Debug NTP packet
ntpdate -d [server]

# Continuous monitoring
ntpq -p

# Chrony tracking
chronyc tracking
chronyc sources
```

## References

- RFC 5905: https://www.rfc-editor.org/rfc/rfc5905
- NTP.org: https://www.ntp.org/
- NTP Pool Project: https://www.ntppool.org/
- Mills, D.L.: Computer Network Time Synchronization (book)

## Implementation Notes for This Project

This FreeRTOS NTP implementation provides:
- Full NTP client (Mode 3) support
- SNTP client mode for resource-constrained devices
- NTS (Network Time Security) support for authenticated time
- Broadcast client mode for one-to-many synchronization
- Configurable clock discipline algorithms
- Multiple server support with selection algorithm
- Integration with FreeRTOS tasks and timers
