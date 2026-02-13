# Skills Documentation

This directory contains detailed technical documentation about the protocols and technologies used in this FreeRTOS NTP Client implementation. These files serve as knowledge base for GitHub Copilot and as reference documentation for developers.

## Available Skills

### 1. [NTP (Network Time Protocol)](ntp.md)
**Full NTP client implementation with clock discipline algorithms**

- Complete NTP v4 protocol specification
- Packet structure and timestamp formats
- Time calculation algorithms (offset and delay)
- Clock discipline using PLL/FLL
- Stratum levels and server selection
- Multi-server support with quality metrics
- Implementation considerations for FreeRTOS
- Security best practices

**When to use:** High-precision time synchronization, server applications, continuous clock discipline

---

### 2. [SNTP (Simple Network Time Protocol)](sntp.md)
**Simplified NTP for resource-constrained embedded systems**

- SNTP vs NTP comparison
- Minimal packet handling
- Simplified time calculation
- Clock adjustment strategies (step/slew)
- Low memory footprint implementation
- Power optimization for battery-powered devices
- Broadcast mode support
- Best practices for embedded systems

**When to use:** IoT devices, simple clients, limited resources, infrequent synchronization

---

### 3. [NTS (Network Time Security)](nts.md)
**Cryptographic authentication for NTP**

- NTS-KE (Key Exchange) over TLS 1.3
- AEAD encryption (AES-SIV-CMAC-256)
- Cookie management system
- Extension fields in NTP packets
- mbedTLS integration for embedded systems
- Security against MITM, replay, and forgery attacks
- Public NTS server list
- Performance considerations

**When to use:** Security-critical applications, authenticated time sources, protection against time-based attacks

---

## Quick Reference

### Protocol Comparison

| Feature | NTP | SNTP | NTS |
|---------|-----|------|-----|
| **Purpose** | Full sync + discipline | Simple client | Security layer |
| **Code Size** | ~50-100 KB | ~2-5 KB | +10-20 KB |
| **Accuracy** | 1-10 ms | 10-100 ms | Same as base |
| **Complexity** | High | Low | Medium |
| **Resources** | High RAM/CPU | Minimal | Medium |
| **Security** | None | None | Authenticated |
| **Best For** | Servers, precision | Embedded, IoT | Secure apps |

### Port Numbers

- **NTP/SNTP**: UDP 123
- **NTS-KE**: TCP 4460

### RFCs

- **NTP**: RFC 5905 (v4)
- **SNTP**: RFC 4330
- **NTS**: RFC 8915

### Common AEAD Algorithms

- **AES-SIV-CMAC-256** (NTS mandatory)
- **AES-SIV-CMAC-384** (optional)
- **AES-SIV-CMAC-512** (optional)

### Recommended Public Servers

```c
// Standard NTP/SNTP
"pool.ntp.org"
"time.google.com"
"time.cloudflare.com"

// NTS-enabled
"time.cloudflare.com"  // NTS-KE on port 4460
"nts.ntp.se"
"virginia.time.system76.com"
```

## Using These Skills

### For Developers

These documents provide:
- Protocol specifications and wire formats
- Implementation patterns and code examples
- Best practices and security considerations
- Troubleshooting guidance
- Testing procedures

### For GitHub Copilot

These skills help Copilot:
- Understand protocol requirements
- Generate specification-compliant code
- Suggest appropriate error handling
- Recommend security best practices
- Provide context-aware completions

## Implementation in This Project

This FreeRTOS NTP Client library implements:

✅ **Full NTP Client** (Mode 3)
- Complete packet handling
- Offset and delay calculation
- Optional clock discipline
- Multi-server support

✅ **SNTP Mode**
- Configurable via `use_sntp_mode` flag
- Minimal footprint variant
- Simple time setting

✅ **NTS Support**
- NTS-KE client over TLS 1.3
- AEAD authentication
- Cookie jar management
- mbedTLS integration

✅ **Broadcast Client**
- One-to-many synchronization
- Passive listening mode
- LAN optimization

## Contributing

When adding new features or fixing bugs, please:
1. Reference the appropriate skill document
2. Follow the patterns and best practices described
3. Update skills if protocol handling changes
4. Add test cases for security-critical code

## Additional Resources

### Official Documentation
- [NTP.org](https://www.ntp.org/)
- [RFC Editor](https://www.rfc-editor.org/)
- [IETF NTP Working Group](https://datatracker.ietf.org/wg/ntp/)

### Tools
- **ntpdate**: Simple NTP query tool
- **ntpq**: NTP query program
- **chrony/chronyc**: Modern NTP implementation
- **ntpd-rs**: Rust-based NTP daemon with NTS
- **Wireshark**: Packet capture and analysis

### Testing Services
- [NTP Pool Project](https://www.ntppool.org/)
- [Public NTS Servers](https://github.com/jauderho/nts-servers)
- [NTPsec](https://www.ntpsec.org/)

## License

These skill documents are part of the FreeRTOS NTP Client project.

SPDX-License-Identifier: MIT

Copyright (c) 2026 Daniel Glaser
