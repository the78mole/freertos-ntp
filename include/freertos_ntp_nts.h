/*
 * FreeRTOS NTP Client - NTS (Network Time Security) Support
 * Copyright (c) 2026 Daniel Glaser
 * 
 * SPDX-License-Identifier: MIT
 * 
 * This module implements NTS (RFC 8915) for secure NTP communication.
 */

#ifndef FREERTOS_NTP_NTS_H
#define FREERTOS_NTP_NTS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* NTS-KE (Key Exchange) Protocol Constants */
#define NTS_KE_DEFAULT_PORT          4460
#define NTS_KE_ALPN_STRING           "ntske/1"
#define NTS_MAX_COOKIES              8
#define NTS_COOKIE_MAX_SIZE          256
#define NTS_KEY_SIZE                 32    /* 256 bits */
#define NTS_NONCE_SIZE               16

/* NTS Record Types (RFC 8915) */
#define NTS_RECORD_END_OF_MESSAGE    0
#define NTS_RECORD_NTS_NEXT_PROTO    1
#define NTS_RECORD_ERROR             2
#define NTS_RECORD_WARNING           3
#define NTS_RECORD_AEAD_ALGORITHM    4
#define NTS_RECORD_NEW_COOKIE        5
#define NTS_RECORD_SERVER            6
#define NTS_RECORD_PORT              7

/* NTS Next Protocol Values */
#define NTS_PROTOCOL_NTP             0

/* NTS AEAD Algorithms */
#define NTS_AEAD_AES_SIV_CMAC_256    15  /* RFC 5297 */

/* NTS Error Codes */
#define NTS_ERROR_UNRECOGNIZED_CRITICAL_RECORD  0
#define NTS_ERROR_BAD_REQUEST                   1
#define NTS_ERROR_INTERNAL_SERVER_ERROR         2

/* NTP Extension Field Types for NTS */
#define NTP_EXT_UNIQUE_IDENTIFIER               0x0104
#define NTP_EXT_NTS_COOKIE                      0x0204
#define NTP_EXT_NTS_COOKIE_PLACEHOLDER          0x0304
#define NTP_EXT_NTS_AUTHENTICATOR               0x0404

/* NTS Cookie Storage */
typedef struct {
    uint8_t  data[NTS_COOKIE_MAX_SIZE];
    uint16_t length;
    bool     valid;
} NTSCookie_t;

/* NTS Key Material */
typedef struct {
    uint8_t  c2s[NTS_KEY_SIZE];  /* Client-to-Server key */
    uint8_t  s2c[NTS_KEY_SIZE];  /* Server-to-Client key */
    bool     valid;
} NTSKeys_t;

/* NTS Session State */
typedef struct {
    char     server_name[64];     /* NTP server name from NTS-KE */
    uint16_t server_port;         /* NTP server port */
    uint32_t server_ip;           /* Resolved NTP server IP */
    NTSKeys_t keys;               /* Session keys */
    NTSCookie_t cookies[NTS_MAX_COOKIES];  /* Cookie storage */
    uint32_t num_cookies;         /* Number of valid cookies */
    uint16_t aead_algorithm;      /* Negotiated AEAD algorithm */
    uint32_t last_refresh;        /* Last key refresh timestamp */
    bool     initialized;         /* Session initialized */
} NTSSession_t;

/* NTP Extension Field Header */
typedef struct {
    uint16_t field_type;
    uint16_t length;
} __attribute__((packed)) NTPExtField_t;

/* NTS-KE Record Header */
typedef struct {
    uint16_t critical_type;  /* Critical bit + Type */
    uint16_t length;
} __attribute__((packed)) NTSKERecord_t;

/**
 * @brief Initialize NTS session with NTS-KE server
 * 
 * @param session Pointer to NTS session structure
 * @param nts_ke_server NTS-KE server hostname or IP
 * @param nts_ke_port NTS-KE server port (usually 4460)
 * @return true if NTS-KE succeeded, false otherwise
 */
bool xNTSInitSession(NTSSession_t *session, const char *nts_ke_server, uint16_t nts_ke_port);

/**
 * @brief Add NTS authentication to NTP packet
 * 
 * @param session Pointer to NTS session
 * @param packet Pointer to NTP packet buffer
 * @param packet_size Current packet size
 * @param max_size Maximum packet buffer size
 * @param new_size Pointer to store new packet size
 * @return true if authentication added successfully, false otherwise
 */
bool xNTSAddAuthentication(NTSSession_t *session, uint8_t *packet, 
                           uint32_t packet_size, uint32_t max_size, 
                           uint32_t *new_size);

/**
 * @brief Verify NTS authentication on received NTP packet
 * 
 * @param session Pointer to NTS session
 * @param packet Pointer to received NTP packet
 * @param packet_size Size of received packet
 * @return true if authentication valid, false otherwise
 */
bool xNTSVerifyAuthentication(NTSSession_t *session, const uint8_t *packet, 
                               uint32_t packet_size);

/**
 * @brief Extract cookies from NTP response
 * 
 * @param session Pointer to NTS session
 * @param packet Pointer to NTP packet
 * @param packet_size Size of packet
 * @return Number of cookies extracted
 */
uint32_t ulNTSExtractCookies(NTSSession_t *session, const uint8_t *packet, 
                              uint32_t packet_size);

/**
 * @brief Check if NTS session needs refresh
 * 
 * @param session Pointer to NTS session
 * @return true if session should be refreshed, false otherwise
 */
bool xNTSNeedsRefresh(const NTSSession_t *session);

/**
 * @brief Clean up NTS session resources
 * 
 * @param session Pointer to NTS session
 */
void vNTSCleanupSession(NTSSession_t *session);

#ifdef __cplusplus
}
#endif

#endif /* FREERTOS_NTP_NTS_H */
