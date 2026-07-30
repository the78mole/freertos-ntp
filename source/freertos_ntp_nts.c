/*
 * FreeRTOS NTP Client - NTS Implementation
 * Copyright (c) 2026 Daniel Glaser
 * 
 * SPDX-License-Identifier: MIT
 */

#include "freertos_ntp_nts.h"
#include "FreeRTOS.h"
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_TLS.h"
#include <string.h>
#include <stdlib.h>

/* NTS-KE timeout in milliseconds */
#define NTS_KE_TIMEOUT_MS    10000

/* Session refresh interval (24 hours) */
#define NTS_SESSION_REFRESH_INTERVAL  (24 * 60 * 60)

/* Helper function to write NTS-KE record */
static uint32_t prvWriteNTSKERecord(uint8_t *buffer, uint16_t type, 
                                    bool critical, const uint8_t *data, 
                                    uint16_t data_len)
{
    NTSKERecord_t *record = (NTSKERecord_t *)buffer;
    uint16_t critical_bit = critical ? 0x8000 : 0x0000;
    
    record->critical_type = FreeRTOS_htons(critical_bit | type);
    record->length = FreeRTOS_htons(data_len);
    
    if (data && data_len > 0) {
        memcpy(buffer + sizeof(NTSKERecord_t), data, data_len);
    }
    
    return sizeof(NTSKERecord_t) + data_len;
}

/* Helper function to read NTS-KE record */
static bool prvReadNTSKERecord(const uint8_t *buffer, uint32_t buffer_len,
                               uint32_t *offset, uint16_t *type, bool *critical,
                               const uint8_t **data, uint16_t *data_len)
{
    if (*offset + sizeof(NTSKERecord_t) > buffer_len) {
        return false;
    }
    
    const NTSKERecord_t *record = (const NTSKERecord_t *)(buffer + *offset);
    uint16_t critical_type = FreeRTOS_ntohs(record->critical_type);
    
    *critical = (critical_type & 0x8000) != 0;
    *type = critical_type & 0x7FFF;
    *data_len = FreeRTOS_ntohs(record->length);
    
    *offset += sizeof(NTSKERecord_t);
    
    if (*offset + *data_len > buffer_len) {
        return false;
    }
    
    *data = buffer + *offset;
    *offset += *data_len;
    
    return true;
}

/* Perform NTS-KE handshake */
bool xNTSInitSession(NTSSession_t *session, const char *nts_ke_server, uint16_t nts_ke_port)
{
    Socket_t sock;
    uint8_t request_buffer[512];
    uint8_t response_buffer[2048];
    uint32_t request_len = 0;
    int32_t bytes_sent, bytes_received;
    struct freertos_sockaddr server_addr;
    uint32_t ip_addr;
    BaseType_t tls_result;
    
    if (session == NULL || nts_ke_server == NULL) {
        return false;
    }
    
    /* Initialize session structure */
    memset(session, 0, sizeof(NTSSession_t));
    
    /* Resolve NTS-KE server */
    ip_addr = FreeRTOS_gethostbyname(nts_ke_server);
    if (ip_addr == 0) {
        return false;
    }
    
    /* Create TCP socket for NTS-KE */
    sock = FreeRTOS_socket(FREERTOS_AF_INET, FREERTOS_SOCK_STREAM, FREERTOS_IPPROTO_TCP);
    if (sock == FREERTOS_INVALID_SOCKET) {
        return false;
    }
    
    /* Set socket timeout */
    TickType_t timeout = pdMS_TO_TICKS(NTS_KE_TIMEOUT_MS);
    FreeRTOS_setsockopt(sock, 0, FREERTOS_SO_RCVTIMEO, &timeout, sizeof(timeout));
    FreeRTOS_setsockopt(sock, 0, FREERTOS_SO_SNDTIMEO, &timeout, sizeof(timeout));
    
    /* Set up server address */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = FREERTOS_AF_INET;
    server_addr.sin_addr = ip_addr;
    server_addr.sin_port = FreeRTOS_htons(nts_ke_port);
    
    /* Connect to NTS-KE server */
    if (FreeRTOS_connect(sock, &server_addr, sizeof(server_addr)) != 0) {
        FreeRTOS_closesocket(sock);
        return false;
    }
    
    /* Upgrade to TLS */
    tls_result = FreeRTOS_setsockopt(sock, 0, FREERTOS_SO_REQUIRE_TLS, NULL, 0);
    if (tls_result != pdPASS) {
        FreeRTOS_closesocket(sock);
        return false;
    }
    
    /* Set ALPN for NTS-KE */
    FreeRTOS_setsockopt(sock, 0, FREERTOS_SO_ALPN_PROTOCOLS, 
                       (void *)NTS_KE_ALPN_STRING, strlen(NTS_KE_ALPN_STRING));
    
    /* Build NTS-KE request */
    /* Record 1: NTS Next Protocol */
    uint16_t next_proto = FreeRTOS_htons(NTS_PROTOCOL_NTP);
    request_len += prvWriteNTSKERecord(request_buffer + request_len, 
                                      NTS_RECORD_NTS_NEXT_PROTO, true,
                                      (uint8_t *)&next_proto, sizeof(next_proto));
    
    /* Record 2: AEAD Algorithm */
    uint16_t aead = FreeRTOS_htons(NTS_AEAD_AES_SIV_CMAC_256);
    request_len += prvWriteNTSKERecord(request_buffer + request_len,
                                      NTS_RECORD_AEAD_ALGORITHM, true,
                                      (uint8_t *)&aead, sizeof(aead));
    
    /* Record 3: End of Message */
    request_len += prvWriteNTSKERecord(request_buffer + request_len,
                                      NTS_RECORD_END_OF_MESSAGE, true,
                                      NULL, 0);
    
    /* Send NTS-KE request */
    bytes_sent = FreeRTOS_send(sock, request_buffer, request_len, 0);
    if (bytes_sent != (int32_t)request_len) {
        FreeRTOS_closesocket(sock);
        return false;
    }
    
    /* Receive NTS-KE response */
    bytes_received = FreeRTOS_recv(sock, response_buffer, sizeof(response_buffer), 0);
    if (bytes_received <= 0) {
        FreeRTOS_closesocket(sock);
        return false;
    }
    
    FreeRTOS_closesocket(sock);
    
    /* Parse NTS-KE response */
    uint32_t offset = 0;
    bool session_valid = false;
    
    while (offset < (uint32_t)bytes_received) {
        uint16_t type;
        bool critical;
        const uint8_t *data;
        uint16_t data_len;
        
        if (!prvReadNTSKERecord(response_buffer, bytes_received, &offset,
                               &type, &critical, &data, &data_len)) {
            break;
        }
        
        switch (type) {
            case NTS_RECORD_END_OF_MESSAGE:
                /* End of message reached */
                session_valid = true;
                goto parse_done;
                
            case NTS_RECORD_NTS_NEXT_PROTO:
                if (data_len == 2) {
                    uint16_t proto = FreeRTOS_ntohs(*(uint16_t *)data);
                    if (proto != NTS_PROTOCOL_NTP) {
                        return false;  /* Unsupported protocol */
                    }
                }
                break;
                
            case NTS_RECORD_AEAD_ALGORITHM:
                if (data_len == 2) {
                    session->aead_algorithm = FreeRTOS_ntohs(*(uint16_t *)data);
                }
                break;
                
            case NTS_RECORD_NEW_COOKIE:
                if (session->num_cookies < NTS_MAX_COOKIES && data_len <= NTS_COOKIE_MAX_SIZE) {
                    memcpy(session->cookies[session->num_cookies].data, data, data_len);
                    session->cookies[session->num_cookies].length = data_len;
                    session->cookies[session->num_cookies].valid = true;
                    session->num_cookies++;
                }
                break;
                
            case NTS_RECORD_SERVER:
                if (data_len > 0 && data_len < sizeof(session->server_name)) {
                    memcpy(session->server_name, data, data_len);
                    session->server_name[data_len] = '\0';
                }
                break;
                
            case NTS_RECORD_PORT:
                if (data_len == 2) {
                    session->server_port = FreeRTOS_ntohs(*(uint16_t *)data);
                }
                break;
                
            case NTS_RECORD_ERROR:
                /* Server returned error */
                return false;
                
            default:
                if (critical) {
                    /* Unrecognized critical record */
                    return false;
                }
                break;
        }
    }
    
parse_done:
    if (!session_valid || session->num_cookies == 0) {
        return false;
    }
    
    /* Derive keys from TLS session (simplified - real implementation would use exporter) */
    /* In a real implementation, use TLS key exporter with label "EXPORTER-network-time-security" */
    /* For this implementation, we'll use a placeholder key derivation */
    memset(session->keys.c2s, 0xAA, NTS_KEY_SIZE);  /* Placeholder */
    memset(session->keys.s2c, 0xBB, NTS_KEY_SIZE);  /* Placeholder */
    session->keys.valid = true;
    
    /* Set default NTP server if not specified */
    if (session->server_name[0] == '\0') {
        strncpy(session->server_name, nts_ke_server, sizeof(session->server_name) - 1);
    }
    
    if (session->server_port == 0) {
        session->server_port = 123;  /* Default NTP port */
    }
    
    /* Resolve NTP server */
    session->server_ip = FreeRTOS_gethostbyname(session->server_name);
    
    session->initialized = true;
    session->last_refresh = xTaskGetTickCount() / configTICK_RATE_HZ;
    
    return true;
}

/* Add NTS authentication to NTP packet */
bool xNTSAddAuthentication(NTSSession_t *session, uint8_t *packet,
                           uint32_t packet_size, uint32_t max_size,
                           uint32_t *new_size)
{
    uint32_t offset = packet_size;
    
    if (session == NULL || !session->initialized || packet == NULL) {
        return false;
    }
    
    /* Ensure we have at least one cookie */
    if (session->num_cookies == 0) {
        return false;
    }
    
    /* Add Unique Identifier extension field */
    if (offset + sizeof(NTPExtField_t) + 32 > max_size) {
        return false;
    }
    
    NTPExtField_t *uid_field = (NTPExtField_t *)(packet + offset);
    uid_field->field_type = FreeRTOS_htons(NTP_EXT_UNIQUE_IDENTIFIER);
    uid_field->length = FreeRTOS_htons(32);
    offset += sizeof(NTPExtField_t);
    
    /* Generate random unique identifier */
    for (uint32_t i = 0; i < 32; i++) {
        packet[offset++] = (uint8_t)(xTaskGetTickCount() + i);
    }
    
    /* Add Cookie extension field (use first valid cookie) */
    for (uint32_t i = 0; i < session->num_cookies; i++) {
        if (session->cookies[i].valid) {
            if (offset + sizeof(NTPExtField_t) + session->cookies[i].length > max_size) {
                return false;
            }
            
            NTPExtField_t *cookie_field = (NTPExtField_t *)(packet + offset);
            cookie_field->field_type = FreeRTOS_htons(NTP_EXT_NTS_COOKIE);
            cookie_field->length = FreeRTOS_htons(session->cookies[i].length);
            offset += sizeof(NTPExtField_t);
            
            memcpy(packet + offset, session->cookies[i].data, session->cookies[i].length);
            offset += session->cookies[i].length;
            
            /* Mark cookie as used (will be replaced by server) */
            session->cookies[i].valid = false;
            break;
        }
    }
    
    /* Add Cookie Placeholder extension fields for new cookies */
    uint16_t placeholder_len = 128;  /* Request 128-byte cookies */
    for (uint32_t i = 0; i < 7; i++) {  /* Request 7 new cookies */
        if (offset + sizeof(NTPExtField_t) + 2 > max_size) {
            break;
        }
        
        NTPExtField_t *placeholder_field = (NTPExtField_t *)(packet + offset);
        placeholder_field->field_type = FreeRTOS_htons(NTP_EXT_NTS_COOKIE_PLACEHOLDER);
        placeholder_field->length = FreeRTOS_htons(2);
        offset += sizeof(NTPExtField_t);
        
        *(uint16_t *)(packet + offset) = FreeRTOS_htons(placeholder_len);
        offset += 2;
    }
    
    /* Add Authenticator extension field (placeholder for real AEAD encryption) */
    if (offset + sizeof(NTPExtField_t) + 32 > max_size) {
        return false;
    }
    
    NTPExtField_t *auth_field = (NTPExtField_t *)(packet + offset);
    auth_field->field_type = FreeRTOS_htons(NTP_EXT_NTS_AUTHENTICATOR);
    auth_field->length = FreeRTOS_htons(32);
    offset += sizeof(NTPExtField_t);
    
    /* In a real implementation, this would be AES-SIV-CMAC-256 authentication tag */
    /* For now, use a placeholder */
    memset(packet + offset, 0xCC, 32);
    offset += 32;
    
    *new_size = offset;
    return true;
}

/* Verify NTS authentication */
bool xNTSVerifyAuthentication(NTSSession_t *session, const uint8_t *packet,
                               uint32_t packet_size)
{
    if (session == NULL || !session->initialized || packet == NULL) {
        return false;
    }
    
    /* In a real implementation, this would verify the AEAD authenticator */
    /* For this implementation, we'll do basic validation */
    
    if (packet_size <= 48) {
        return false;  /* No extension fields */
    }
    
    /* Look for authenticator extension field */
    uint32_t offset = 48;  /* Skip NTP header */
    bool found_authenticator = false;
    
    while (offset + sizeof(NTPExtField_t) <= packet_size) {
        const NTPExtField_t *field = (const NTPExtField_t *)(packet + offset);
        uint16_t field_type = FreeRTOS_ntohs(field->field_type);
        uint16_t field_len = FreeRTOS_ntohs(field->length);
        
        offset += sizeof(NTPExtField_t);
        
        if (offset + field_len > packet_size) {
            break;
        }
        
        if (field_type == NTP_EXT_NTS_AUTHENTICATOR) {
            found_authenticator = true;
            /* Real implementation would verify AEAD tag here */
            break;
        }
        
        offset += field_len;
    }
    
    return found_authenticator;
}

/* Extract cookies from NTP response */
uint32_t ulNTSExtractCookies(NTSSession_t *session, const uint8_t *packet,
                              uint32_t packet_size)
{
    uint32_t cookies_extracted = 0;
    
    if (session == NULL || packet == NULL || packet_size <= 48) {
        return 0;
    }
    
    /* Parse extension fields looking for cookies */
    uint32_t offset = 48;  /* Skip NTP header */
    
    /* First, compact existing valid cookies */
    uint32_t valid_count = 0;
    for (uint32_t i = 0; i < session->num_cookies; i++) {
        if (session->cookies[i].valid && valid_count != i) {
            memcpy(&session->cookies[valid_count], &session->cookies[i], sizeof(NTSCookie_t));
            valid_count++;
        } else if (session->cookies[i].valid) {
            valid_count++;
        }
    }
    session->num_cookies = valid_count;
    
    /* Extract new cookies */
    while (offset + sizeof(NTPExtField_t) <= packet_size) {
        const NTPExtField_t *field = (const NTPExtField_t *)(packet + offset);
        uint16_t field_type = FreeRTOS_ntohs(field->field_type);
        uint16_t field_len = FreeRTOS_ntohs(field->length);
        
        offset += sizeof(NTPExtField_t);
        
        if (offset + field_len > packet_size) {
            break;
        }
        
        if (field_type == NTP_EXT_NTS_COOKIE && 
            session->num_cookies < NTS_MAX_COOKIES &&
            field_len <= NTS_COOKIE_MAX_SIZE) {
            
            memcpy(session->cookies[session->num_cookies].data, packet + offset, field_len);
            session->cookies[session->num_cookies].length = field_len;
            session->cookies[session->num_cookies].valid = true;
            session->num_cookies++;
            cookies_extracted++;
        }
        
        offset += field_len;
    }
    
    return cookies_extracted;
}

/* Check if session needs refresh */
bool xNTSNeedsRefresh(const NTSSession_t *session)
{
    if (session == NULL || !session->initialized) {
        return true;
    }
    
    /* Check if we're out of cookies */
    uint32_t valid_cookies = 0;
    for (uint32_t i = 0; i < session->num_cookies; i++) {
        if (session->cookies[i].valid) {
            valid_cookies++;
        }
    }
    
    if (valid_cookies == 0) {
        return true;
    }
    
    /* Check if session is too old */
    uint32_t current_time = xTaskGetTickCount() / configTICK_RATE_HZ;
    if ((current_time - session->last_refresh) > NTS_SESSION_REFRESH_INTERVAL) {
        return true;
    }
    
    return false;
}

/* Clean up NTS session */
void vNTSCleanupSession(NTSSession_t *session)
{
    if (session != NULL) {
        memset(session, 0, sizeof(NTSSession_t));
    }
}
