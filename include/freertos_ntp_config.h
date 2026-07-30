/*
 * FreeRTOS NTP Client Configuration
 * Copyright (c) 2026 Daniel Glaser
 * 
 * SPDX-License-Identifier: MIT
 * 
 * This file contains user-configurable settings for the NTP client.
 * Copy this file to your project and customize as needed.
 */

#ifndef FREERTOS_NTP_CONFIG_H
#define FREERTOS_NTP_CONFIG_H

/* Maximum number of NTP servers that can be configured (up to 16) */
#ifndef NTP_MAX_SERVERS
#define NTP_MAX_SERVERS 16
#endif

/* Maximum number of NTP servers actively polled (up to 8) */
#ifndef NTP_MAX_ACTIVE_SERVERS
#define NTP_MAX_ACTIVE_SERVERS 8
#endif

/* Default NTP server port */
#ifndef NTP_DEFAULT_PORT
#define NTP_DEFAULT_PORT 123
#endif

/* Default poll interval in seconds */
#ifndef NTP_DEFAULT_POLL_INTERVAL
#define NTP_DEFAULT_POLL_INTERVAL 64
#endif

/* Default response timeout in milliseconds */
#ifndef NTP_DEFAULT_TIMEOUT_MS
#define NTP_DEFAULT_TIMEOUT_MS 5000
#endif

/* Default FreeRTOS task priority for NTP task */
#ifndef NTP_TASK_PRIORITY
#define NTP_TASK_PRIORITY (tskIDLE_PRIORITY + 2)
#endif

/* Default FreeRTOS task stack size for NTP task */
#ifndef NTP_TASK_STACK_SIZE
#define NTP_TASK_STACK_SIZE 2048
#endif

/* Enable debug logging (requires a printf implementation) */
#ifndef NTP_ENABLE_DEBUG
#define NTP_ENABLE_DEBUG 0
#endif

#if NTP_ENABLE_DEBUG
#define NTP_DEBUG_PRINTF(...) printf(__VA_ARGS__)
#else
#define NTP_DEBUG_PRINTF(...) ((void)0)
#endif

#endif /* FREERTOS_NTP_CONFIG_H */
