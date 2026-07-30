/*
 * FreeRTOS NTP Client - x86 Example
 * Copyright (c) 2026 Daniel Glaser
 * 
 * SPDX-License-Identifier: MIT
 * 
 * This example demonstrates the FreeRTOS NTP client on x86 platform.
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "FreeRTOS.h"
#include "task.h"
#include "freertos_ntp.h"

/* Global NTP handle */
static NTPTaskHandle_t xNTPHandle = NULL;

/* System time variables */
static uint32_t ulSystemTimeSec = 0;
static uint32_t ulSystemTimeUs = 0;

/* Time set callback - called when NTP wants to set system time */
static void prvTimeSetCallback(uint32_t seconds, uint32_t microseconds)
{
    ulSystemTimeSec = seconds;
    ulSystemTimeUs = microseconds;
    
    /* Convert to human-readable format */
    time_t t = (time_t)seconds;
    struct tm *tm_info = gmtime(&t);
    char buffer[26];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
    
    printf("[NTP] Time set to: %s.%06u UTC\n", buffer, microseconds);
}

/* Skew adjustment callback - called for small time corrections */
static void prvSkewSetCallback(int32_t skew_us)
{
    /* Adjust system time by skew amount */
    int64_t total_us = (int64_t)ulSystemTimeUs + (int64_t)skew_us;
    
    if (total_us < 0) {
        ulSystemTimeSec--;
        ulSystemTimeUs = (uint32_t)(1000000 + total_us);
    } else if (total_us >= 1000000) {
        ulSystemTimeSec++;
        ulSystemTimeUs = (uint32_t)(total_us - 1000000);
    } else {
        ulSystemTimeUs = (uint32_t)total_us;
    }
    
    printf("[NTP] Time adjusted by %d microseconds\n", skew_us);
}

/* Status display task */
static void prvStatusTask(void *pvParameters)
{
    char status_buffer[2048];
    
    /* Wait for NTP to initialize */
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    while (1) {
        /* Get and display NTP status */
        uint32_t len = ulNTPGetStatusString(xNTPHandle, status_buffer, sizeof(status_buffer));
        if (len > 0) {
            printf("\n=== NTP Status ===\n");
            printf("%s", status_buffer);
            printf("\n");
        }
        
        /* Display current system time */
        time_t t = (time_t)ulSystemTimeSec;
        struct tm *tm_info = gmtime(&t);
        char time_buffer[26];
        strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", tm_info);
        printf("System time: %s.%06u UTC\n\n", time_buffer, ulSystemTimeUs);
        
        /* Wait 30 seconds before next update */
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}

/* Main application entry point */
int main(void)
{
    NTPConfig_t xNTPConfig;
    
    printf("FreeRTOS NTP Client - x86 Example\n");
    printf("==================================\n\n");
    
    /* Initialize system time to a reasonable value (2026-01-01) */
    ulSystemTimeSec = 1735689600;  /* 2026-01-01 00:00:00 UTC */
    ulSystemTimeUs = 0;
    
    /* Get default NTP configuration */
    vNTPGetDefaultConfig(&xNTPConfig);
    
    /* Customize configuration */
    xNTPConfig.use_sntp_mode = false;  /* Use full NTP mode with metrics */
    xNTPConfig.poll_interval = 16;     /* Poll every 16 seconds for demo */
    xNTPConfig.timeout_ms = 5000;
    xNTPConfig.task_priority = tskIDLE_PRIORITY + 2;
    xNTPConfig.task_stack_size = 4096;
    
    /* Initialize NTP client */
    xNTPHandle = xNTPClientInit(&xNTPConfig);
    if (xNTPHandle == NULL) {
        printf("ERROR: Failed to initialize NTP client\n");
        return 1;
    }
    
    printf("NTP client initialized successfully\n");
    
    /* Register callbacks */
    vNTPRegisterTimeSetCallback(xNTPHandle, prvTimeSetCallback);
    vNTPRegisterSkewSetCallback(xNTPHandle, prvSkewSetCallback);
    
    printf("Callbacks registered\n");
    
    /* Add NTP servers */
    /* Note: In a real x86 environment, these would work. For this example, */
    /* they demonstrate the API usage. */
    if (xNTPAddServer(xNTPHandle, "pool.ntp.org")) {
        printf("Added server: pool.ntp.org\n");
    }
    if (xNTPAddServer(xNTPHandle, "time.google.com")) {
        printf("Added server: time.google.com\n");
    }
    if (xNTPAddServer(xNTPHandle, "time.cloudflare.com")) {
        printf("Added server: time.cloudflare.com\n");
    }
    if (xNTPAddServer(xNTPHandle, "time.nist.gov")) {
        printf("Added server: time.nist.gov\n");
    }
    
    printf("\n");
    
    /* Create status display task */
    xTaskCreate(prvStatusTask, "Status", 4096, NULL, tskIDLE_PRIORITY + 1, NULL);
    
    /* Start NTP synchronization */
    if (xNTPStart(xNTPHandle)) {
        printf("NTP synchronization started\n\n");
    } else {
        printf("ERROR: Failed to start NTP synchronization\n");
        return 1;
    }
    
    /* Start FreeRTOS scheduler */
    vTaskStartScheduler();
    
    /* Should never reach here */
    printf("ERROR: Scheduler exited unexpectedly\n");
    return 1;
}

/* FreeRTOS hook functions */
void vApplicationMallocFailedHook(void)
{
    printf("ERROR: Memory allocation failed\n");
    for (;;);
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    printf("ERROR: Stack overflow in task: %s\n", pcTaskName);
    for (;;);
}

void vApplicationIdleHook(void)
{
    /* Called when idle */
}

void vApplicationTickHook(void)
{
    /* Called on each tick */
}
