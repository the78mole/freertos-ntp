# Network Time Security (NTS) - Skill Context

## Overview

Network Time Security (NTS) is a security extension for NTP that provides cryptographic authentication and encryption for time synchronization. NTS protects against various attacks including Man-in-the-Middle (MITM), packet forgery, and replay attacks, ensuring that time information comes from authenticated sources.

## Key Specifications

- **RFC 8915**: Network Time Security for the Network Time Protocol (June 2020)
- **RFC 5705**: Keying Material Exporters for Transport Layer Security (TLS)
- **RFC 8446**: The Transport Layer Security (TLS) Protocol Version 1.3

## NTS Architecture

NTS consists of two main components:

### 1. NTS Key Exchange (NTS-KE)
- Runs over TLS (TCP port 4460)
- Establishes shared secrets and parameters
- Exchanges cookie keys between client and server
- One-time operation (or infrequent renewal)

### 2. NTS Extension Fields for NTP
- Authenticates NTP packets using NTS cookies
- Uses AEAD (Authenticated Encryption with Associated Data)
- Runs over UDP (port 123, same as NTP)
- Fast, low-overhead per-packet authentication

## NTS-KE Protocol Flow

```
Client                                Server
  |                                      |
  |---- TLS Handshake -----------------→|
  |←--- TLS Handshake ------------------| (Mutual TLS 1.3)
  |                                      |
  |---- NTS-KE Request ----------------→|
  |     (Client Records)                 |
  |                                      |
  |←--- NTS-KE Response ----------------| 
  |     (Server Records + Cookies)       |
  |                                      |
  |---- TLS Close ----------------------|
```

### NTS-KE Records

NTS-KE exchanges records in a Type-Length-Value (TLV) format:

```c
typedef struct {
    uint16_t record_type;    // Record type
    uint16_t body_length;    // Length of record body
    uint8_t  body[];         // Record body (variable length)
} NTSKERecord_t;
```

### NTS-KE Record Types

```c
// Critical record types (must be understood)
#define NTS_KE_END_OF_MESSAGE           0  // End of records
#define NTS_KE_NEXT_PROTOCOL            1  // NTPv4, etc.
#define NTS_KE_ERROR                    2  // Error indication
#define NTS_KE_WARNING                  3  // Warning
#define NTS_KE_AEAD_ALGORITHM           4  // AEAD algorithm

// Non-critical record types
#define NTS_KE_NEW_COOKIE               5  // NTS cookie
#define NTS_KE_SERVER                   6  // NTP server name/address
#define NTS_KE_PORT                     7  // NTP server port

// Error codes
#define NTS_KE_ERROR_UNRECOGNIZED_CRITICAL_RECORD  0
#define NTS_KE_ERROR_BAD_REQUEST                   1
#define NTS_KE_ERROR_INTERNAL_SERVER_ERROR         2
```

## NTS-KE Client Implementation

```c
// NTS-KE session establishment
typedef struct {
    int tls_socket;                    // TLS socket
    uint16_t next_protocol;            // NTPv4 = 0
    uint16_t aead_algorithm;           // AEAD_AES_SIV_CMAC_256 = 15
    uint8_t  c2s_key[32];             // Client-to-server key
    uint8_t  s2c_key[32];             // Server-to-client key
    uint8_t  cookies[8][256];         // NTS cookies (up to 8)
    uint8_t  cookie_count;            // Number of cookies received
    char     ntp_server[256];          // NTP server address
    uint16_t ntp_port;                 // NTP server port (default 123)
} NTSKESession_t;

// Establish NTS-KE session
int establishNTSKE(const char *server, uint16_t port, NTSKESession_t *session)
{
    // 1. Establish TLS 1.3 connection to server:4460
    session->tls_socket = establishTLS13(server, port);
    
    // 2. Export keying material using RFC 5705
    exportKeyingMaterial(session->tls_socket, 
                        "EXPORTER-network-time-security",
                        session->c2s_key, session->s2c_key);
    
    // 3. Send NTS-KE request records
    sendNTSKERecord(NTS_KE_NEXT_PROTOCOL, &ntpv4);
    sendNTSKERecord(NTS_KE_AEAD_ALGORITHM, &aes_siv_cmac_256);
    sendNTSKERecord(NTS_KE_END_OF_MESSAGE, NULL);
    
    // 4. Receive NTS-KE response records
    while (receiveNTSKERecord(&record)) {
        switch (record.type) {
            case NTS_KE_NEXT_PROTOCOL:
                session->next_protocol = *((uint16_t*)record.body);
                break;
            case NTS_KE_AEAD_ALGORITHM:
                session->aead_algorithm = *((uint16_t*)record.body);
                break;
            case NTS_KE_NEW_COOKIE:
                memcpy(session->cookies[session->cookie_count++],
                       record.body, record.length);
                break;
            case NTS_KE_SERVER:
                strncpy(session->ntp_server, record.body, record.length);
                break;
            case NTS_KE_PORT:
                session->ntp_port = *((uint16_t*)record.body);
                break;
            case NTS_KE_END_OF_MESSAGE:
                goto done;
        }
    }
    
done:
    // 5. Close TLS connection
    closeTLS(session->tls_socket);
    
    return 0;
}
```

## AEAD Algorithms

NTS uses AEAD (Authenticated Encryption with Associated Data):

```c
// AEAD algorithm identifiers (from RFC 5116)
#define NTS_AEAD_AES_SIV_CMAC_256       15  // Mandatory
#define NTS_AEAD_AES_SIV_CMAC_384       16  // Optional
#define NTS_AEAD_AES_SIV_CMAC_512       17  // Optional

// AEAD_AES_SIV_CMAC_256 parameters
#define NTS_KEY_SIZE        32  // bytes
#define NTS_NONCE_SIZE      16  // bytes
#define NTS_TAG_SIZE        16  // bytes
```

## NTS Extension Fields in NTP

NTP packets with NTS include extension fields:

```c
// NTS extension field structure
typedef struct {
    uint16_t field_type;     // Extension field type
    uint16_t length;         // Length (including header)
    uint8_t  value[];        // Field value (variable)
} NTPExtensionField_t;

// NTS extension field types
#define NTS_UNIQUE_IDENTIFIER          0x0104  // Unique identifier
#define NTS_COOKIE                     0x0204  // NTS cookie
#define NTS_COOKIE_PLACEHOLDER         0x0304  // Cookie placeholder
#define NTS_AUTHENTICATOR              0x0404  // AEAD authenticator
```

## NTS NTP Packet Structure

```c
// NTP packet with NTS extensions
struct {
    NTPHeader_t header;                    // 48 bytes - standard NTP
    NTPExtensionField_t unique_id;         // Unique identifier
    NTPExtensionField_t nts_cookie;        // NTS cookie from server
    NTPExtensionField_t cookie_placeholder; // Request new cookies
    NTPExtensionField_t authenticator;     // AEAD authenticator (last)
} NTPwithNTS;
```

## NTS Client Request (NTP with NTS)

```c
// Prepare NTS-authenticated NTP request
void prepareNTSRequest(NTPPacket_t *packet, NTSKESession_t *session)
{
    // 1. Standard NTP header
    memset(packet, 0, sizeof(NTPPacket_t));
    packet->li_vn_mode = (0 << 6) | (4 << 3) | 3; // NTPv4 client
    
    uint8_t *extension = (uint8_t*)(packet + 1);
    
    // 2. Unique Identifier extension
    uint8_t unique_id[32];
    generateRandomBytes(unique_id, 32);
    extension += addExtensionField(extension, NTS_UNIQUE_IDENTIFIER, 
                                   unique_id, 32);
    
    // 3. NTS Cookie extension (from NTS-KE)
    extension += addExtensionField(extension, NTS_COOKIE,
                                   session->cookies[0], 
                                   session->cookie_length);
    
    // 4. Cookie Placeholder (request 7 new cookies)
    for (int i = 0; i < 7; i++) {
        extension += addExtensionField(extension, NTS_COOKIE_PLACEHOLDER,
                                       NULL, 0);
    }
    
    // 5. AEAD Authenticator (must be last)
    uint8_t nonce[16];
    generateNonce(nonce, packet);
    
    uint8_t ciphertext[16];
    aeadEncrypt(session->c2s_key,  // C2S key from NTS-KE
                nonce,              // Nonce
                packet,             // Associated data (NTP + extensions)
                extension - (uint8_t*)packet, // AD length
                NULL, 0,            // No plaintext
                ciphertext);        // Output: authentication tag
    
    extension += addExtensionField(extension, NTS_AUTHENTICATOR,
                                   ciphertext, 16);
}
```

## NTS Response Verification

```c
// Verify NTS-authenticated NTP response
bool verifyNTSResponse(NTPPacket_t *response, NTSKESession_t *session)
{
    uint8_t *extension = (uint8_t*)(response + 1);
    uint8_t *authenticator = NULL;
    size_t auth_length = 0;
    
    // 1. Parse extension fields
    while (extension < end_of_packet) {
        NTPExtensionField_t *field = (NTPExtensionField_t*)extension;
        uint16_t type = ntohs(field->field_type);
        uint16_t length = ntohs(field->length);
        
        if (type == NTS_COOKIE) {
            // Save new cookie for next request
            saveNewCookie(session, field->value, length - 4);
        }
        else if (type == NTS_AUTHENTICATOR) {
            // Save authenticator (must be last)
            authenticator = field->value;
            auth_length = length - 4;
        }
        
        extension += length;
    }
    
    // 2. Verify authenticator
    if (authenticator == NULL) return false;
    
    uint8_t nonce[16];
    extractNonce(nonce, response);
    
    size_t ad_length = authenticator - (uint8_t*)response;
    
    // Decrypt/verify using AEAD
    return aeadDecrypt(session->s2c_key,  // S2C key from NTS-KE
                      nonce,
                      response,           // Associated data
                      ad_length,
                      authenticator,      // Ciphertext (just auth tag)
                      auth_length,
                      NULL);              // No plaintext output
}
```

## Cookie Management

```c
typedef struct {
    uint8_t  cookies[8][256];  // Up to 8 cookies
    uint16_t lengths[8];       // Cookie lengths
    uint8_t  count;            // Number of valid cookies
    uint8_t  next_index;       // Next cookie to use
} NTSCookieJar_t;

// Get next cookie for request
uint8_t* getNextCookie(NTSCookieJar_t *jar, uint16_t *length)
{
    if (jar->count == 0) return NULL;
    
    uint8_t *cookie = jar->cookies[jar->next_index];
    *length = jar->lengths[jar->next_index];
    
    // Remove used cookie
    jar->count--;
    jar->next_index = (jar->next_index + 1) % 8;
    
    return cookie;
}

// Store new cookie from response
void storeNewCookie(NTSCookieJar_t *jar, uint8_t *cookie, uint16_t length)
{
    if (jar->count >= 8) return; // Jar full
    
    uint8_t index = (jar->next_index + jar->count) % 8;
    memcpy(jar->cookies[index], cookie, length);
    jar->lengths[index] = length;
    jar->count++;
}
```

## Security Considerations

### Protected Against

1. **Man-in-the-Middle (MITM)**
   - TLS 1.3 protects NTS-KE exchange
   - AEAD protects NTP packets

2. **Packet Forgery**
   - Authenticator ensures packet integrity
   - Only server with cookie key can create valid cookies

3. **Replay Attacks**
   - Unique identifiers prevent replay
   - Cookies are single-use (server-enforced)

4. **Denial of Service**
   - TLS connection to NTS-KE server is rate-limited
   - Cookie mechanism protects NTP server from DDoS

### Best Practices

```c
// Security best practices for NTS
const NTSSecurity_t nts_security = {
    .require_tls13 = true,           // Require TLS 1.3
    .verify_server_cert = true,      // Validate server certificate
    .check_hostname = true,          // Check certificate hostname
    .cookie_lifetime = 3600,         // Refresh cookies hourly
    .min_cookie_count = 2,           // Keep at least 2 cookies
    .max_ke_retry = 3,               // Max NTS-KE retries
    .fallback_to_unauth = false,     // Don't fall back to unauthenticated NTP
};
```

## NTS Server List

```c
// Public NTS servers
const char *nts_servers[] = {
    "time.cloudflare.com",           // Cloudflare NTS
    "time.nl.immc.nts.netnod.se",   // Netnod (Sweden)
    "nts.ntp.se",                    // Swedish NTP server
    "ntpmon.dcs1.biz",              // NTP monitoring service
    "virginia.time.system76.com",    // System76
};
```

## Performance Considerations

### NTS-KE Overhead
- **Initial setup**: ~100-500ms (TLS handshake + key exchange)
- **Frequency**: Once per session or hourly
- **Impact**: Minimal (one-time cost)

### NTP with NTS Overhead
- **Packet size**: +100-300 bytes (extensions)
- **Computation**: AEAD encrypt/decrypt (~1ms on modern CPU)
- **Impact**: Low (similar to standard NTP)

## Implementation for Embedded Systems

```c
// NTS configuration for FreeRTOS
typedef struct {
    const char *nts_ke_server;       // NTS-KE server
    uint16_t    nts_ke_port;         // Default: 4460
    const char *ca_cert_path;        // CA certificate for TLS
    uint32_t    cookie_refresh_sec;   // Cookie refresh interval
    bool        verify_certificate;   // Verify server certificate
    mbedtls_ssl_config *tls_config;  // mbedTLS configuration
} NTSConfig_t;

// Memory requirements (estimated)
// Code: ~10-20 KB (with mbedTLS)
// RAM: ~2-4 KB (TLS context + cookies)
// Stack: ~4-8 KB (TLS handshake)
```

## mbedTLS Integration

```c
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"

// Establish TLS 1.3 connection
int establishNTSKETLS(const char *server, uint16_t port)
{
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    
    // Initialize mbedTLS
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    
    // Configure TLS 1.3
    mbedtls_ssl_config_defaults(&conf,
        MBEDTLS_SSL_IS_CLIENT,
        MBEDTLS_SSL_TRANSPORT_STREAM,
        MBEDTLS_SSL_PRESET_DEFAULT);
    
    // Force TLS 1.3
    mbedtls_ssl_conf_min_version(&conf, MBEDTLS_SSL_MAJOR_VERSION_3,
                                       MBEDTLS_SSL_MINOR_VERSION_4);
    
    // Perform handshake
    // ...
    
    return 0;
}
```

## Testing NTS Implementation

```bash
# Test NTS-KE connection
openssl s_client -connect time.cloudflare.com:4460 -tls1_3

# Test NTS with chrony
chronyc sources | grep "^*"
chronyc authdata

# Test NTS with ntpd-rs
ntpd-rs --nts-server time.cloudflare.com

# Verify NTS support
nts-ke-check time.cloudflare.com
```

## Error Handling

```c
typedef enum {
    NTS_SUCCESS = 0,
    NTS_ERROR_TLS_HANDSHAKE,         // TLS handshake failed
    NTS_ERROR_CERTIFICATE,           // Certificate validation failed
    NTS_ERROR_KE_PROTOCOL,           // NTS-KE protocol error
    NTS_ERROR_NO_COOKIES,            // No cookies received
    NTS_ERROR_AEAD,                  // AEAD encryption/decryption error
    NTS_ERROR_INVALID_AUTHENTICATOR, // Invalid authenticator
    NTS_ERROR_NO_ALGORITHMS,         // No common AEAD algorithm
} NTSError_t;
```

## Debugging NTS

```c
// Enable verbose NTS logging
#define NTS_DEBUG_LEVEL_NONE    0
#define NTS_DEBUG_LEVEL_ERROR   1
#define NTS_DEBUG_LEVEL_WARN    2
#define NTS_DEBUG_LEVEL_INFO    3
#define NTS_DEBUG_LEVEL_DEBUG   4

void ntsLog(int level, const char *fmt, ...)
{
    if (level <= nts_current_debug_level) {
        // Log message
    }
}
```

## References

- RFC 8915: https://www.rfc-editor.org/rfc/rfc8915.html
- NTS Project: https://www.nts-project.org/
- Cloudflare NTS: https://www.cloudflare.com/time/
- NTPsec NTS: https://docs.ntpsec.org/latest/NTS-QuickStart.html

## Implementation Notes for This Project

This FreeRTOS NTP implementation provides NTS support through:
- mbedTLS integration for TLS 1.3
- NTS-KE client implementation
- AEAD_AES_SIV_CMAC_256 support
- Cookie jar management
- Automatic cookie refresh
- Certificate verification
- Graceful fallback on NTS-KE failure
- Configuration options for NTS enable/disable
