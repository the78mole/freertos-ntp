/*
 * FreeRTOS NTP Client
 * Copyright (c) 2026 Daniel Glaser
 * 
 * SPDX-License-Identifier: MIT
 */

#ifndef FREERTOS_NTP_H
#define FREERTOS_NTP_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* NTP Configuration */
#define NTP_MAX_SERVERS          16    /* Maximum number of configured servers */
#define NTP_MAX_ACTIVE_SERVERS   8     /* Maximum number of actively used servers */
#define NTP_DEFAULT_PORT         123
#define NTP_PACKET_SIZE          48

/* NTP Protocol Constants */
#define NTP_VERSION              4
#define NTP_MODE_CLIENT          3
#define NTP_MODE_SERVER          4

/* NTP Timestamp Epoch Offset (1900 to 1970) */
#define NTP_TIMESTAMP_DELTA      2208988800ULL

/* Server Status Flags */
#define NTP_SERVER_FLAG_ACTIVE   (1 << 0)
#define NTP_SERVER_FLAG_VALID    (1 << 1)
#define NTP_SERVER_FLAG_REACHABLE (1 << 2)
#define NTP_SERVER_FLAG_SELECTED (1 << 3)

/* NTP Packet Structure (48 bytes) */
typedef struct {
    uint8_t  li_vn_mode;      /* Leap Indicator, Version Number, Mode */
    uint8_t  stratum;         /* Stratum level */
    uint8_t  poll;            /* Poll interval */
    int8_t   precision;       /* Precision */
    uint32_t root_delay;      /* Root delay */
    uint32_t root_dispersion; /* Root dispersion */
    uint32_t ref_id;          /* Reference ID */
    uint32_t ref_timestamp_sec;  /* Reference timestamp seconds */
    uint32_t ref_timestamp_frac; /* Reference timestamp fraction */
    uint32_t orig_timestamp_sec;  /* Originate timestamp seconds */
    uint32_t orig_timestamp_frac; /* Originate timestamp fraction */
    uint32_t rx_timestamp_sec;    /* Receive timestamp seconds */
    uint32_t rx_timestamp_frac;   /* Receive timestamp fraction */
    uint32_t tx_timestamp_sec;    /* Transmit timestamp seconds */
    uint32_t tx_timestamp_frac;   /* Transmit timestamp fraction */
} NTPPacket_t;

/* NTP Server Statistics */
typedef struct {
    char     hostname[64];    /* Server hostname or IP */
    uint32_t ip_address;      /* Resolved IP address */
    uint8_t  stratum;         /* Server stratum */
    uint8_t  reach;           /* Reachability register (8-bit shift register) */
    uint16_t flags;           /* Status flags */
    int32_t  offset_us;       /* Clock offset in microseconds */
    int32_t  delay_us;        /* Round-trip delay in microseconds */
    int32_t  jitter_us;       /* Jitter in microseconds */
    uint32_t poll_interval;   /* Polling interval in seconds */
    uint32_t last_update;     /* Last successful update timestamp */
    uint32_t ref_id;          /* Reference identifier */
} NTPServerStats_t;

/* NTP Client Configuration */
typedef struct {
    bool     use_sntp_mode;   /* Use simple SNTP mode (no metrics) */
    uint16_t port;            /* NTP server port (default 123) */
    uint32_t poll_interval;   /* Poll interval in seconds */
    uint32_t timeout_ms;      /* Response timeout in milliseconds */
    uint16_t task_priority;   /* FreeRTOS task priority */
    uint32_t task_stack_size; /* FreeRTOS task stack size */
} NTPConfig_t;

/* Time adjustment callback function types */
typedef void (*NTPTimeSetCallback_t)(uint32_t seconds, uint32_t microseconds);
typedef void (*NTPSkewSetCallback_t)(int32_t skew_us);

/* NTP Task Handle */
typedef void* NTPTaskHandle_t;

/**
 * @brief Initialize NTP client with configuration
 * 
 * @param config Pointer to NTP configuration structure
 * @return NTPTaskHandle_t Handle to NTP task, or NULL on failure
 */
NTPTaskHandle_t xNTPClientInit(const NTPConfig_t *config);

/**
 * @brief Add NTP server to configuration
 * 
 * @param handle NTP task handle
 * @param hostname Server hostname or IP address
 * @return true if server added successfully, false otherwise
 */
bool xNTPAddServer(NTPTaskHandle_t handle, const char *hostname);

/**
 * @brief Remove NTP server from configuration
 * 
 * @param handle NTP task handle
 * @param hostname Server hostname or IP address
 * @return true if server removed successfully, false otherwise
 */
bool xNTPRemoveServer(NTPTaskHandle_t handle, const char *hostname);

/**
 * @brief Register callback for setting system time
 * 
 * @param handle NTP task handle
 * @param callback Function to call when time needs to be set
 */
void vNTPRegisterTimeSetCallback(NTPTaskHandle_t handle, NTPTimeSetCallback_t callback);

/**
 * @brief Register callback for adjusting time skew
 * 
 * @param handle NTP task handle
 * @param callback Function to call for time skew adjustments
 */
void vNTPRegisterSkewSetCallback(NTPTaskHandle_t handle, NTPSkewSetCallback_t callback);

/**
 * @brief Get status of all configured servers
 * 
 * @param handle NTP task handle
 * @param stats Array to store server statistics
 * @param max_servers Maximum number of servers to return
 * @return Number of servers returned
 */
uint32_t ulNTPGetServerStats(NTPTaskHandle_t handle, NTPServerStats_t *stats, uint32_t max_servers);

/**
 * @brief Get formatted status string (similar to ntpq -p)
 * 
 * @param handle NTP task handle
 * @param buffer Buffer to store formatted string
 * @param buffer_size Size of the buffer
 * @return Number of characters written
 */
uint32_t ulNTPGetStatusString(NTPTaskHandle_t handle, char *buffer, uint32_t buffer_size);

/**
 * @brief Start NTP synchronization
 * 
 * @param handle NTP task handle
 * @return true if started successfully, false otherwise
 */
bool xNTPStart(NTPTaskHandle_t handle);

/**
 * @brief Stop NTP synchronization
 * 
 * @param handle NTP task handle
 */
void vNTPStop(NTPTaskHandle_t handle);

/**
 * @brief Get default NTP configuration
 * 
 * @param config Pointer to configuration structure to fill
 */
void vNTPGetDefaultConfig(NTPConfig_t *config);

#ifdef __cplusplus
}
#endif

#endif /* FREERTOS_NTP_H */
