/*
 * FreeRTOS NTP Client Implementation
 * Copyright (c) 2026 Daniel Glaser
 * 
 * SPDX-License-Identifier: MIT
 */

#include "freertos_ntp.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

/* Internal NTP client context */
typedef struct {
    NTPConfig_t config;
    NTPServerStats_t servers[NTP_MAX_SERVERS];
    uint32_t num_servers;
    TaskHandle_t task_handle;
    SemaphoreHandle_t mutex;
    bool running;
    NTPTimeSetCallback_t time_callback;
    NTPSkewSetCallback_t skew_callback;
    Socket_t socket;
} NTPContext_t;

/* Helper function to convert NTP timestamp to Unix timestamp */
static void prvNTPTimestampToUnix(uint32_t ntp_sec, uint32_t ntp_frac, 
                                  uint32_t *unix_sec, uint32_t *unix_us)
{
    *unix_sec = ntp_sec - NTP_TIMESTAMP_DELTA;
    *unix_us = (uint32_t)(((uint64_t)ntp_frac * 1000000ULL) >> 32);
}

/* Helper function to convert Unix timestamp to NTP timestamp */
static void prvUnixToNTPTimestamp(uint32_t unix_sec, uint32_t unix_us,
                                  uint32_t *ntp_sec, uint32_t *ntp_frac)
{
    *ntp_sec = unix_sec + NTP_TIMESTAMP_DELTA;
    *ntp_frac = (uint32_t)(((uint64_t)unix_us << 32) / 1000000ULL);
}

/* Get current time in Unix format */
static void prvGetCurrentTime(uint32_t *seconds, uint32_t *microseconds)
{
    TickType_t ticks = xTaskGetTickCount();
    uint64_t us = ((uint64_t)ticks * 1000000ULL) / configTICK_RATE_HZ;
    *seconds = (uint32_t)(us / 1000000ULL);
    *microseconds = (uint32_t)(us % 1000000ULL);
}

/* Calculate time difference in microseconds */
static int64_t prvTimeDiffUs(uint32_t sec1, uint32_t us1, uint32_t sec2, uint32_t us2)
{
    int64_t diff_sec = (int64_t)sec1 - (int64_t)sec2;
    int64_t diff_us = (int64_t)us1 - (int64_t)us2;
    return (diff_sec * 1000000LL) + diff_us;
}

/* Create and bind NTP socket */
static Socket_t prvCreateNTPSocket(uint32_t timeout_ms)
{
    Socket_t sock;
    struct freertos_sockaddr bind_addr;
    TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);
    
    sock = FreeRTOS_socket(FREERTOS_AF_INET, FREERTOS_SOCK_DGRAM, FREERTOS_IPPROTO_UDP);
    if (sock == FREERTOS_INVALID_SOCKET) {
        return FREERTOS_INVALID_SOCKET;
    }
    
    /* Set receive timeout */
    FreeRTOS_setsockopt(sock, 0, FREERTOS_SO_RCVTIMEO, &timeout_ticks, sizeof(timeout_ticks));
    
    /* Bind to any local address */
    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin_family = FREERTOS_AF_INET;
    bind_addr.sin_addr = FreeRTOS_htonl(FREERTOS_INADDR_ANY);
    bind_addr.sin_port = FreeRTOS_htons(0); /* Any port */
    
    if (FreeRTOS_bind(sock, &bind_addr, sizeof(bind_addr)) != 0) {
        FreeRTOS_closesocket(sock);
        return FREERTOS_INVALID_SOCKET;
    }
    
    return sock;
}

/* Send NTP request and receive response */
static bool prvQueryNTPServer(Socket_t sock, NTPServerStats_t *server, 
                              uint32_t port, NTPPacket_t *response,
                              uint32_t *t1_sec, uint32_t *t1_us,
                              uint32_t *t4_sec, uint32_t *t4_us)
{
    NTPPacket_t request;
    struct freertos_sockaddr server_addr;
    int32_t bytes_sent, bytes_received;
    
    /* Get transmit timestamp (t1) */
    prvGetCurrentTime(t1_sec, t1_us);
    
    /* Prepare NTP request packet */
    memset(&request, 0, sizeof(request));
    request.li_vn_mode = (NTP_VERSION << 3) | NTP_MODE_CLIENT;
    
    /* Convert t1 to NTP format and set as transmit timestamp */
    prvUnixToNTPTimestamp(*t1_sec, *t1_us, &request.tx_timestamp_sec, &request.tx_timestamp_frac);
    request.tx_timestamp_sec = FreeRTOS_htonl(request.tx_timestamp_sec);
    request.tx_timestamp_frac = FreeRTOS_htonl(request.tx_timestamp_frac);
    
    /* Set up server address */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = FREERTOS_AF_INET;
    server_addr.sin_addr = server->ip_address;
    server_addr.sin_port = FreeRTOS_htons(port);
    
    /* Send request */
    bytes_sent = FreeRTOS_sendto(sock, &request, sizeof(request), 0,
                                 &server_addr, sizeof(server_addr));
    if (bytes_sent != sizeof(request)) {
        return false;
    }
    
    /* Receive response */
    socklen_t addr_len = sizeof(server_addr);
    bytes_received = FreeRTOS_recvfrom(sock, response, sizeof(NTPPacket_t), 0,
                                       &server_addr, &addr_len);
    
    /* Get receive timestamp (t4) */
    prvGetCurrentTime(t4_sec, t4_us);
    
    if (bytes_received != sizeof(NTPPacket_t)) {
        return false;
    }
    
    /* Convert network byte order to host byte order */
    response->root_delay = FreeRTOS_ntohl(response->root_delay);
    response->root_dispersion = FreeRTOS_ntohl(response->root_dispersion);
    response->ref_id = FreeRTOS_ntohl(response->ref_id);
    response->ref_timestamp_sec = FreeRTOS_ntohl(response->ref_timestamp_sec);
    response->ref_timestamp_frac = FreeRTOS_ntohl(response->ref_timestamp_frac);
    response->orig_timestamp_sec = FreeRTOS_ntohl(response->orig_timestamp_sec);
    response->orig_timestamp_frac = FreeRTOS_ntohl(response->orig_timestamp_frac);
    response->rx_timestamp_sec = FreeRTOS_ntohl(response->rx_timestamp_sec);
    response->rx_timestamp_frac = FreeRTOS_ntohl(response->rx_timestamp_frac);
    response->tx_timestamp_sec = FreeRTOS_ntohl(response->tx_timestamp_sec);
    response->tx_timestamp_frac = FreeRTOS_ntohl(response->tx_timestamp_frac);
    
    return true;
}

/* Calculate NTP metrics (offset, delay, jitter) */
static void prvCalculateNTPMetrics(NTPServerStats_t *server, const NTPPacket_t *response,
                                   uint32_t t1_sec, uint32_t t1_us,
                                   uint32_t t4_sec, uint32_t t4_us)
{
    uint32_t t2_sec, t2_us, t3_sec, t3_us;
    int64_t t1, t2, t3, t4;
    int64_t offset, delay;
    int32_t old_offset;
    
    /* Convert NTP timestamps to Unix format */
    prvNTPTimestampToUnix(response->rx_timestamp_sec, response->rx_timestamp_frac, &t2_sec, &t2_us);
    prvNTPTimestampToUnix(response->tx_timestamp_sec, response->tx_timestamp_frac, &t3_sec, &t3_us);
    
    /* Convert all timestamps to microseconds for calculation */
    t1 = (int64_t)t1_sec * 1000000LL + (int64_t)t1_us;
    t2 = (int64_t)t2_sec * 1000000LL + (int64_t)t2_us;
    t3 = (int64_t)t3_sec * 1000000LL + (int64_t)t3_us;
    t4 = (int64_t)t4_sec * 1000000LL + (int64_t)t4_us;
    
    /* Calculate offset: ((t2 - t1) + (t3 - t4)) / 2 */
    offset = ((t2 - t1) + (t3 - t4)) / 2;
    
    /* Calculate delay: (t4 - t1) - (t3 - t2) */
    delay = (t4 - t1) - (t3 - t2);
    
    /* Store old offset for jitter calculation */
    old_offset = server->offset_us;
    
    /* Update server statistics */
    server->offset_us = (int32_t)offset;
    server->delay_us = (int32_t)delay;
    server->stratum = response->stratum;
    server->ref_id = response->ref_id;
    
    /* Calculate jitter as absolute difference from previous offset */
    if (server->flags & NTP_SERVER_FLAG_VALID) {
        int32_t offset_diff = server->offset_us - old_offset;
        if (offset_diff < 0) offset_diff = -offset_diff;
        /* Simple exponential moving average for jitter */
        server->jitter_us = (server->jitter_us * 7 + offset_diff) / 8;
    } else {
        server->jitter_us = 0;
    }
    
    /* Update reachability register (shift left and set bit 0) */
    server->reach = (server->reach << 1) | 1;
    
    /* Mark server as valid and reachable */
    server->flags |= NTP_SERVER_FLAG_VALID | NTP_SERVER_FLAG_REACHABLE;
    server->last_update = t4_sec;
}

/* NTP client task */
static void prvNTPTask(void *pvParameters)
{
    NTPContext_t *ctx = (NTPContext_t *)pvParameters;
    uint32_t poll_count = 0;
    
    while (ctx->running) {
        /* Wait for network to be ready */
        if (FreeRTOS_IsNetworkUp() == pdFALSE) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        
        /* Create socket if not already created */
        if (ctx->socket == FREERTOS_INVALID_SOCKET) {
            ctx->socket = prvCreateNTPSocket(ctx->config.timeout_ms);
            if (ctx->socket == FREERTOS_INVALID_SOCKET) {
                vTaskDelay(pdMS_TO_TICKS(1000));
                continue;
            }
        }
        
        xSemaphoreTake(ctx->mutex, portMAX_DELAY);
        
        /* Query each active server */
        uint32_t active_count = 0;
        int32_t best_offset = 0;
        bool found_best = false;
        
        for (uint32_t i = 0; i < ctx->num_servers && active_count < NTP_MAX_ACTIVE_SERVERS; i++) {
            if (!(ctx->servers[i].flags & NTP_SERVER_FLAG_ACTIVE)) {
                continue;
            }
            
            active_count++;
            
            NTPPacket_t response;
            uint32_t t1_sec, t1_us, t4_sec, t4_us;
            
            if (prvQueryNTPServer(ctx->socket, &ctx->servers[i], ctx->config.port,
                                 &response, &t1_sec, &t1_us, &t4_sec, &t4_us)) {
                
                if (ctx->config.use_sntp_mode) {
                    /* SNTP mode: just update time from server timestamp */
                    uint32_t server_sec, server_us;
                    prvNTPTimestampToUnix(response.tx_timestamp_sec, response.tx_timestamp_frac,
                                        &server_sec, &server_us);
                    
                    if (ctx->time_callback) {
                        ctx->time_callback(server_sec, server_us);
                    }
                    
                    ctx->servers[i].reach = (ctx->servers[i].reach << 1) | 1;
                    ctx->servers[i].flags |= NTP_SERVER_FLAG_REACHABLE;
                    ctx->servers[i].last_update = t4_sec;
                } else {
                    /* Full NTP mode: calculate metrics */
                    prvCalculateNTPMetrics(&ctx->servers[i], &response, t1_sec, t1_us, t4_sec, t4_us);
                    
                    /* Select best server (lowest jitter and valid) */
                    if (!found_best || 
                        (ctx->servers[i].jitter_us < best_offset && 
                         (ctx->servers[i].flags & NTP_SERVER_FLAG_VALID))) {
                        best_offset = ctx->servers[i].offset_us;
                        found_best = true;
                        ctx->servers[i].flags |= NTP_SERVER_FLAG_SELECTED;
                    } else {
                        ctx->servers[i].flags &= ~NTP_SERVER_FLAG_SELECTED;
                    }
                }
            } else {
                /* Query failed - update reachability */
                ctx->servers[i].reach <<= 1;
                if (ctx->servers[i].reach == 0) {
                    ctx->servers[i].flags &= ~NTP_SERVER_FLAG_REACHABLE;
                }
            }
        }
        
        /* Apply time adjustment if we have a best server in NTP mode */
        if (!ctx->config.use_sntp_mode && found_best) {
            if (ctx->skew_callback && poll_count > 0) {
                /* After initial sync, use skew adjustment */
                ctx->skew_callback(best_offset);
            } else if (ctx->time_callback && poll_count == 0) {
                /* First sync: set time directly */
                uint32_t curr_sec, curr_us;
                prvGetCurrentTime(&curr_sec, &curr_us);
                int64_t adjusted_us = (int64_t)curr_us + (int64_t)best_offset;
                uint32_t adj_sec = curr_sec + (uint32_t)(adjusted_us / 1000000LL);
                uint32_t adj_us = (uint32_t)(adjusted_us % 1000000LL);
                ctx->time_callback(adj_sec, adj_us);
            }
        }
        
        xSemaphoreGive(ctx->mutex);
        
        poll_count++;
        
        /* Wait for next poll interval */
        vTaskDelay(pdMS_TO_TICKS(ctx->config.poll_interval * 1000));
    }
    
    /* Clean up */
    if (ctx->socket != FREERTOS_INVALID_SOCKET) {
        FreeRTOS_closesocket(ctx->socket);
        ctx->socket = FREERTOS_INVALID_SOCKET;
    }
    
    vTaskDelete(NULL);
}

/* Public API Implementation */

void vNTPGetDefaultConfig(NTPConfig_t *config)
{
    configASSERT(config != NULL);
    
    config->use_sntp_mode = false;
    config->port = NTP_DEFAULT_PORT;
    config->poll_interval = 64;  /* 64 seconds default */
    config->timeout_ms = 5000;   /* 5 second timeout */
    config->task_priority = tskIDLE_PRIORITY + 2;
    config->task_stack_size = 2048;
}

NTPTaskHandle_t xNTPClientInit(const NTPConfig_t *config)
{
    NTPContext_t *ctx;
    
    configASSERT(config != NULL);
    
    /* Allocate context */
    ctx = (NTPContext_t *)pvPortMalloc(sizeof(NTPContext_t));
    if (ctx == NULL) {
        return NULL;
    }
    
    memset(ctx, 0, sizeof(NTPContext_t));
    memcpy(&ctx->config, config, sizeof(NTPConfig_t));
    
    /* Create mutex */
    ctx->mutex = xSemaphoreCreateMutex();
    if (ctx->mutex == NULL) {
        vPortFree(ctx);
        return NULL;
    }
    
    ctx->socket = FREERTOS_INVALID_SOCKET;
    ctx->running = false;
    
    return (NTPTaskHandle_t)ctx;
}

bool xNTPAddServer(NTPTaskHandle_t handle, const char *hostname)
{
    NTPContext_t *ctx = (NTPContext_t *)handle;
    uint32_t ip_addr;
    
    if (ctx == NULL || hostname == NULL) {
        return false;
    }
    
    xSemaphoreTake(ctx->mutex, portMAX_DELAY);
    
    if (ctx->num_servers >= NTP_MAX_SERVERS) {
        xSemaphoreGive(ctx->mutex);
        return false;
    }
    
    /* Resolve hostname to IP address */
    ip_addr = FreeRTOS_gethostbyname(hostname);
    if (ip_addr == 0) {
        xSemaphoreGive(ctx->mutex);
        return false;
    }
    
    /* Add server to list */
    NTPServerStats_t *server = &ctx->servers[ctx->num_servers];
    memset(server, 0, sizeof(NTPServerStats_t));
    strncpy(server->hostname, hostname, sizeof(server->hostname) - 1);
    server->hostname[sizeof(server->hostname) - 1] = '\0';
    server->ip_address = ip_addr;
    server->flags = NTP_SERVER_FLAG_ACTIVE;
    server->poll_interval = ctx->config.poll_interval;
    
    ctx->num_servers++;
    
    xSemaphoreGive(ctx->mutex);
    
    return true;
}

bool xNTPRemoveServer(NTPTaskHandle_t handle, const char *hostname)
{
    NTPContext_t *ctx = (NTPContext_t *)handle;
    
    if (ctx == NULL || hostname == NULL) {
        return false;
    }
    
    xSemaphoreTake(ctx->mutex, portMAX_DELAY);
    
    /* Find and remove server */
    for (uint32_t i = 0; i < ctx->num_servers; i++) {
        if (strcmp(ctx->servers[i].hostname, hostname) == 0) {
            /* Shift remaining servers down */
            for (uint32_t j = i; j < ctx->num_servers - 1; j++) {
                memcpy(&ctx->servers[j], &ctx->servers[j + 1], sizeof(NTPServerStats_t));
            }
            ctx->num_servers--;
            xSemaphoreGive(ctx->mutex);
            return true;
        }
    }
    
    xSemaphoreGive(ctx->mutex);
    return false;
}

void vNTPRegisterTimeSetCallback(NTPTaskHandle_t handle, NTPTimeSetCallback_t callback)
{
    NTPContext_t *ctx = (NTPContext_t *)handle;
    
    if (ctx == NULL) {
        return;
    }
    
    xSemaphoreTake(ctx->mutex, portMAX_DELAY);
    ctx->time_callback = callback;
    xSemaphoreGive(ctx->mutex);
}

void vNTPRegisterSkewSetCallback(NTPTaskHandle_t handle, NTPSkewSetCallback_t callback)
{
    NTPContext_t *ctx = (NTPContext_t *)handle;
    
    if (ctx == NULL) {
        return;
    }
    
    xSemaphoreTake(ctx->mutex, portMAX_DELAY);
    ctx->skew_callback = callback;
    xSemaphoreGive(ctx->mutex);
}

uint32_t ulNTPGetServerStats(NTPTaskHandle_t handle, NTPServerStats_t *stats, uint32_t max_servers)
{
    NTPContext_t *ctx = (NTPContext_t *)handle;
    uint32_t count;
    
    if (ctx == NULL || stats == NULL || max_servers == 0) {
        return 0;
    }
    
    xSemaphoreTake(ctx->mutex, portMAX_DELAY);
    
    count = ctx->num_servers < max_servers ? ctx->num_servers : max_servers;
    memcpy(stats, ctx->servers, count * sizeof(NTPServerStats_t));
    
    xSemaphoreGive(ctx->mutex);
    
    return count;
}

uint32_t ulNTPGetStatusString(NTPTaskHandle_t handle, char *buffer, uint32_t buffer_size)
{
    NTPContext_t *ctx = (NTPContext_t *)handle;
    uint32_t offset = 0;
    
    if (ctx == NULL || buffer == NULL || buffer_size < 100) {
        return 0;
    }
    
    xSemaphoreTake(ctx->mutex, portMAX_DELAY);
    
    /* Print header similar to ntpq -p */
    offset += snprintf(buffer + offset, buffer_size - offset,
                      "     remote           refid      st t when poll reach   delay   offset  jitter\n");
    offset += snprintf(buffer + offset, buffer_size - offset,
                      "==============================================================================\n");
    
    for (uint32_t i = 0; i < ctx->num_servers && offset < buffer_size - 80; i++) {
        NTPServerStats_t *s = &ctx->servers[i];
        char marker = ' ';
        
        if (s->flags & NTP_SERVER_FLAG_SELECTED) {
            marker = '*';
        } else if (s->flags & NTP_SERVER_FLAG_REACHABLE) {
            marker = '+';
        } else {
            marker = '-';
        }
        
        uint32_t when = 0;
        if (s->last_update > 0) {
            uint32_t curr_sec, curr_us;
            prvGetCurrentTime(&curr_sec, &curr_us);
            when = curr_sec - s->last_update;
        }
        
        char ref_str[16];
        snprintf(ref_str, sizeof(ref_str), "%u.%u.%u.%u",
                (s->ref_id >> 24) & 0xFF, (s->ref_id >> 16) & 0xFF,
                (s->ref_id >> 8) & 0xFF, s->ref_id & 0xFF);
        
        offset += snprintf(buffer + offset, buffer_size - offset,
                          "%c%-15s %-15s %2u u %4u %4u  %3o  %6d  %6d  %6d\n",
                          marker, s->hostname, ref_str, s->stratum,
                          when, s->poll_interval, s->reach,
                          s->delay_us / 1000, s->offset_us / 1000, s->jitter_us / 1000);
    }
    
    xSemaphoreGive(ctx->mutex);
    
    return offset;
}

bool xNTPStart(NTPTaskHandle_t handle)
{
    NTPContext_t *ctx = (NTPContext_t *)handle;
    BaseType_t result;
    
    if (ctx == NULL || ctx->running) {
        return false;
    }
    
    ctx->running = true;
    
    result = xTaskCreate(prvNTPTask, "NTP", ctx->config.task_stack_size,
                        ctx, ctx->config.task_priority, &ctx->task_handle);
    
    if (result != pdPASS) {
        ctx->running = false;
        return false;
    }
    
    return true;
}

void vNTPStop(NTPTaskHandle_t handle)
{
    NTPContext_t *ctx = (NTPContext_t *)handle;
    
    if (ctx == NULL || !ctx->running) {
        return;
    }
    
    ctx->running = false;
    
    /* Wait for task to terminate */
    vTaskDelay(pdMS_TO_TICKS(100));
}
