/*
 * FreeRTOS NTP Client - Broadcast Mode Support
 * Copyright (c) 2026 Daniel Glaser
 * 
 * SPDX-License-Identifier: MIT
 * 
 * This module implements NTP broadcast mode (RFC 5905 Section 3).
 */

#ifndef FREERTOS_NTP_BROADCAST_H
#define FREERTOS_NTP_BROADCAST_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos_ntp.h"

#ifdef __cplusplus
extern "C" {
#endif

/* NTP Broadcast Mode Constants */
#define NTP_MODE_BROADCAST           5    /* Broadcast mode */
#define NTP_BROADCAST_PORT           123
#define NTP_BROADCAST_ADDR           0xE0000101  /* 224.0.1.1 */
#define NTP_BROADCAST_MAX_SERVERS    8

/* Broadcast Authentication Types */
#define NTP_AUTH_NONE                0
#define NTP_AUTH_SYMMETRIC_KEY       1
#define NTP_AUTH_AUTOKEY             2
#define NTP_AUTH_NTS                 3

/* Broadcast Server Configuration */
typedef struct {
    uint32_t server_ip;           /* Broadcast server IP */
    uint8_t  stratum;             /* Server stratum */
    uint32_t ref_id;              /* Reference ID */
    uint32_t last_broadcast;      /* Last broadcast received timestamp */
    uint32_t broadcast_interval;  /* Expected broadcast interval */
    uint8_t  reach;               /* Reachability register */
    uint16_t flags;               /* Status flags */
    uint8_t  auth_type;           /* Authentication type */
    int32_t  offset_us;           /* Last calculated offset */
    bool     valid;               /* Server is valid */
} NTPBroadcastServer_t;

/* Broadcast Receiver Context */
typedef struct {
    Socket_t socket;              /* Multicast socket */
    NTPBroadcastServer_t servers[NTP_BROADCAST_MAX_SERVERS];
    uint32_t num_servers;
    uint32_t multicast_addr;      /* Multicast address to listen on */
    uint16_t port;                /* Port to listen on */
    bool     require_auth;        /* Require authenticated broadcasts */
    bool     running;             /* Receiver is running */
} NTPBroadcastContext_t;

/**
 * @brief Initialize NTP broadcast receiver
 * 
 * @param multicast_addr Multicast address to join (0 for default 224.0.1.1)
 * @param port Port to listen on (0 for default 123)
 * @return Pointer to broadcast context, or NULL on failure
 */
NTPBroadcastContext_t* xNTPBroadcastInit(uint32_t multicast_addr, uint16_t port);

/**
 * @brief Start broadcast receiver
 * 
 * @param context Pointer to broadcast context
 * @return true if started successfully, false otherwise
 */
bool xNTPBroadcastStart(NTPBroadcastContext_t *context);

/**
 * @brief Stop broadcast receiver
 * 
 * @param context Pointer to broadcast context
 */
void vNTPBroadcastStop(NTPBroadcastContext_t *context);

/**
 * @brief Process received broadcast packet
 * 
 * @param context Pointer to broadcast context
 * @param packet Pointer to received NTP packet
 * @param packet_size Size of packet
 * @param source_ip Source IP address
 * @return true if packet processed successfully, false otherwise
 */
bool xNTPBroadcastProcessPacket(NTPBroadcastContext_t *context,
                                const uint8_t *packet, uint32_t packet_size,
                                uint32_t source_ip);

/**
 * @brief Get broadcast server statistics
 * 
 * @param context Pointer to broadcast context
 * @param servers Array to store server statistics
 * @param max_servers Maximum number of servers to return
 * @return Number of servers returned
 */
uint32_t ulNTPBroadcastGetServers(NTPBroadcastContext_t *context,
                                   NTPBroadcastServer_t *servers,
                                   uint32_t max_servers);

/**
 * @brief Set authentication requirement
 * 
 * @param context Pointer to broadcast context
 * @param require true to require authentication, false to accept any
 */
void vNTPBroadcastSetAuthRequired(NTPBroadcastContext_t *context, bool require);

/**
 * @brief Clean up broadcast receiver
 * 
 * @param context Pointer to broadcast context
 */
void vNTPBroadcastCleanup(NTPBroadcastContext_t *context);

#ifdef __cplusplus
}
#endif

#endif /* FREERTOS_NTP_BROADCAST_H */
