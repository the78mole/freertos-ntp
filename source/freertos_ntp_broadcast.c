/*
 * FreeRTOS NTP Client - Broadcast Mode Implementation
 * Copyright (c) 2026 Daniel Glaser
 * 
 * SPDX-License-Identifier: MIT
 */

#include "freertos_ntp_broadcast.h"
#include "FreeRTOS.h"
#include "task.h"
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include <string.h>

/* Broadcast receiver task stack size */
#define NTP_BROADCAST_TASK_STACK_SIZE  2048

/* Broadcast receiver task priority */
#define NTP_BROADCAST_TASK_PRIORITY    (tskIDLE_PRIORITY + 2)

/* Helper function to get current time */
static uint32_t prvGetCurrentTimestamp(void)
{
    TickType_t ticks = xTaskGetTickCount();
    return (uint32_t)(ticks / configTICK_RATE_HZ);
}

/* Helper function to find or create server entry */
static NTPBroadcastServer_t* prvFindOrCreateServer(NTPBroadcastContext_t *context,
                                                    uint32_t source_ip)
{
    /* First, try to find existing server */
    for (uint32_t i = 0; i < context->num_servers; i++) {
        if (context->servers[i].server_ip == source_ip) {
            return &context->servers[i];
        }
    }
    
    /* If not found and we have room, create new entry */
    if (context->num_servers < NTP_BROADCAST_MAX_SERVERS) {
        NTPBroadcastServer_t *server = &context->servers[context->num_servers];
        memset(server, 0, sizeof(NTPBroadcastServer_t));
        server->server_ip = source_ip;
        context->num_servers++;
        return server;
    }
    
    /* No room for new server */
    return NULL;
}

/* Broadcast receiver task */
static void prvBroadcastReceiverTask(void *pvParameters)
{
    NTPBroadcastContext_t *context = (NTPBroadcastContext_t *)pvParameters;
    uint8_t packet_buffer[NTP_PACKET_SIZE + 512];  /* Extra space for extensions */
    struct freertos_sockaddr source_addr;
    socklen_t addr_len;
    
    while (context->running) {
        addr_len = sizeof(source_addr);
        
        /* Receive broadcast packet */
        int32_t bytes_received = FreeRTOS_recvfrom(context->socket, packet_buffer,
                                                   sizeof(packet_buffer), 0,
                                                   &source_addr, &addr_len);
        
        if (bytes_received > 0) {
            /* Process the broadcast packet */
            xNTPBroadcastProcessPacket(context, packet_buffer,
                                      (uint32_t)bytes_received,
                                      source_addr.sin_addr);
        } else if (bytes_received == -pdFREERTOS_ERRNO_EWOULDBLOCK) {
            /* Timeout - check for stale servers */
            uint32_t current_time = prvGetCurrentTimestamp();
            
            for (uint32_t i = 0; i < context->num_servers; i++) {
                NTPBroadcastServer_t *server = &context->servers[i];
                if (server->valid) {
                    uint32_t time_since_last = current_time - server->last_broadcast;
                    
                    /* If no broadcast for 3x expected interval, mark unreachable */
                    if (time_since_last > (server->broadcast_interval * 3)) {
                        server->reach <<= 1;
                        if (server->reach == 0) {
                            server->flags &= ~NTP_SERVER_FLAG_REACHABLE;
                        }
                    }
                }
            }
            
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    
    vTaskDelete(NULL);
}

/* Initialize broadcast receiver */
NTPBroadcastContext_t* xNTPBroadcastInit(uint32_t multicast_addr, uint16_t port)
{
    NTPBroadcastContext_t *context;
    
    /* Allocate context */
    context = (NTPBroadcastContext_t *)pvPortMalloc(sizeof(NTPBroadcastContext_t));
    if (context == NULL) {
        return NULL;
    }
    
    memset(context, 0, sizeof(NTPBroadcastContext_t));
    
    /* Set default multicast address if not specified */
    context->multicast_addr = (multicast_addr != 0) ? multicast_addr : NTP_BROADCAST_ADDR;
    context->port = (port != 0) ? port : NTP_BROADCAST_PORT;
    context->socket = FREERTOS_INVALID_SOCKET;
    context->require_auth = false;
    context->running = false;
    
    return context;
}

/* Start broadcast receiver */
bool xNTPBroadcastStart(NTPBroadcastContext_t *context)
{
    struct freertos_sockaddr bind_addr;
    struct freertos_ip_mreq mreq;
    BaseType_t result;
    TickType_t timeout;
    
    if (context == NULL || context->running) {
        return false;
    }
    
    /* Wait for network to be ready */
    while (FreeRTOS_IsNetworkUp() == pdFALSE) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    /* Create UDP socket */
    context->socket = FreeRTOS_socket(FREERTOS_AF_INET, FREERTOS_SOCK_DGRAM, FREERTOS_IPPROTO_UDP);
    if (context->socket == FREERTOS_INVALID_SOCKET) {
        return false;
    }
    
    /* Set socket options for multicast */
    int reuse = 1;
    FreeRTOS_setsockopt(context->socket, 0, FREERTOS_SO_REUSEADDR, &reuse, sizeof(reuse));
    
    /* Set receive timeout */
    timeout = pdMS_TO_TICKS(5000);
    FreeRTOS_setsockopt(context->socket, 0, FREERTOS_SO_RCVTIMEO, &timeout, sizeof(timeout));
    
    /* Bind to multicast port */
    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin_family = FREERTOS_AF_INET;
    bind_addr.sin_addr = FreeRTOS_htonl(FREERTOS_INADDR_ANY);
    bind_addr.sin_port = FreeRTOS_htons(context->port);
    
    if (FreeRTOS_bind(context->socket, &bind_addr, sizeof(bind_addr)) != 0) {
        FreeRTOS_closesocket(context->socket);
        context->socket = FREERTOS_INVALID_SOCKET;
        return false;
    }
    
    /* Join multicast group */
    memset(&mreq, 0, sizeof(mreq));
    mreq.imr_multiaddr = context->multicast_addr;
    mreq.imr_interface = FreeRTOS_htonl(FREERTOS_INADDR_ANY);
    
    if (FreeRTOS_setsockopt(context->socket, 0, FREERTOS_SO_IP_MULTICAST_IF,
                           &mreq, sizeof(mreq)) != 0) {
        FreeRTOS_closesocket(context->socket);
        context->socket = FREERTOS_INVALID_SOCKET;
        return false;
    }
    
    /* Create receiver task */
    context->running = true;
    result = xTaskCreate(prvBroadcastReceiverTask, "NTPBcast",
                        NTP_BROADCAST_TASK_STACK_SIZE, context,
                        NTP_BROADCAST_TASK_PRIORITY, NULL);
    
    if (result != pdPASS) {
        context->running = false;
        FreeRTOS_closesocket(context->socket);
        context->socket = FREERTOS_INVALID_SOCKET;
        return false;
    }
    
    return true;
}

/* Stop broadcast receiver */
void vNTPBroadcastStop(NTPBroadcastContext_t *context)
{
    if (context == NULL || !context->running) {
        return;
    }
    
    context->running = false;
    
    /* Wait for task to stop */
    vTaskDelay(pdMS_TO_TICKS(100));
    
    /* Close socket */
    if (context->socket != FREERTOS_INVALID_SOCKET) {
        FreeRTOS_closesocket(context->socket);
        context->socket = FREERTOS_INVALID_SOCKET;
    }
}

/* Process broadcast packet */
bool xNTPBroadcastProcessPacket(NTPBroadcastContext_t *context,
                                const uint8_t *packet, uint32_t packet_size,
                                uint32_t source_ip)
{
    const NTPPacket_t *ntp_pkt;
    NTPBroadcastServer_t *server;
    uint8_t mode;
    uint32_t current_time;
    
    if (context == NULL || packet == NULL || packet_size < sizeof(NTPPacket_t)) {
        return false;
    }
    
    ntp_pkt = (const NTPPacket_t *)packet;
    
    /* Verify this is a broadcast packet */
    mode = ntp_pkt->li_vn_mode & 0x07;
    if (mode != NTP_MODE_BROADCAST) {
        return false;
    }
    
    /* Check if authentication is required */
    if (context->require_auth) {
        /* Check for authentication extension fields */
        if (packet_size <= sizeof(NTPPacket_t)) {
            return false;  /* No extensions, reject */
        }
        
        /* In a real implementation, verify authentication here */
        /* For now, just check that extensions exist */
    }
    
    /* Find or create server entry */
    server = prvFindOrCreateServer(context, source_ip);
    if (server == NULL) {
        return false;  /* No room for new servers */
    }
    
    current_time = prvGetCurrentTimestamp();
    
    /* Update server information */
    server->stratum = ntp_pkt->stratum;
    server->ref_id = FreeRTOS_ntohl(ntp_pkt->ref_id);
    
    /* Calculate broadcast interval if we have previous timestamp */
    if (server->last_broadcast > 0) {
        uint32_t interval = current_time - server->last_broadcast;
        if (server->broadcast_interval == 0) {
            server->broadcast_interval = interval;
        } else {
            /* Exponential moving average */
            server->broadcast_interval = (server->broadcast_interval * 7 + interval) / 8;
        }
    }
    
    server->last_broadcast = current_time;
    
    /* Update reachability */
    server->reach = (server->reach << 1) | 1;
    server->flags |= NTP_SERVER_FLAG_REACHABLE | NTP_SERVER_FLAG_VALID;
    server->valid = true;
    
    /* Calculate offset from broadcast timestamp */
    /* In broadcast mode, client doesn't know the propagation delay, */
    /* so it must either estimate it or accept lower accuracy */
    uint32_t tx_sec = FreeRTOS_ntohl(ntp_pkt->tx_timestamp_sec);
    uint32_t tx_frac = FreeRTOS_ntohl(ntp_pkt->tx_timestamp_frac);
    
    /* Convert NTP timestamp to Unix timestamp */
    uint32_t server_time = tx_sec - NTP_TIMESTAMP_DELTA;
    uint32_t server_us = (uint32_t)(((uint64_t)tx_frac * 1000000ULL) >> 32);
    
    /* Calculate offset (simplified, assumes negligible propagation delay) */
    int64_t local_us = (int64_t)current_time * 1000000LL;
    int64_t server_us_total = (int64_t)server_time * 1000000LL + (int64_t)server_us;
    server->offset_us = (int32_t)(server_us_total - local_us);
    
    return true;
}

/* Get broadcast server statistics */
uint32_t ulNTPBroadcastGetServers(NTPBroadcastContext_t *context,
                                   NTPBroadcastServer_t *servers,
                                   uint32_t max_servers)
{
    uint32_t count;
    
    if (context == NULL || servers == NULL || max_servers == 0) {
        return 0;
    }
    
    count = (context->num_servers < max_servers) ? context->num_servers : max_servers;
    
    for (uint32_t i = 0; i < count; i++) {
        memcpy(&servers[i], &context->servers[i], sizeof(NTPBroadcastServer_t));
    }
    
    return count;
}

/* Set authentication requirement */
void vNTPBroadcastSetAuthRequired(NTPBroadcastContext_t *context, bool require)
{
    if (context != NULL) {
        context->require_auth = require;
    }
}

/* Clean up broadcast receiver */
void vNTPBroadcastCleanup(NTPBroadcastContext_t *context)
{
    if (context == NULL) {
        return;
    }
    
    vNTPBroadcastStop(context);
    vPortFree(context);
}
