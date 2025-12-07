/*
 * tasks_common.h
 *
 * Created on: Oct 17, 2021
 * Author: kjagu
 * Updated for: LM35 & Ultrasonic Project
 */

#ifndef MAIN_TASKS_COMMON_H_
#define MAIN_TASKS_COMMON_H_

// ===================================================
// Core 0 Tasks (WiFi & Network Stack)
// ===================================================

// WiFi application task
#define WIFI_APP_TASK_STACK_SIZE            4096
#define WIFI_APP_TASK_PRIORITY              5
#define WIFI_APP_TASK_CORE_ID               0

// HTTP Server task
// Aumentado para 8192 para garantir espaço para JSON/Headers
#define HTTP_SERVER_TASK_STACK_SIZE         8192
#define HTTP_SERVER_TASK_PRIORITY           4
#define HTTP_SERVER_TASK_CORE_ID            0

// HTTP Server Monitor task
#define HTTP_SERVER_MONITOR_STACK_SIZE      4096
#define HTTP_SERVER_MONITOR_PRIORITY        3
#define HTTP_SERVER_MONITOR_CORE_ID         0

// ===================================================
// Core 1 Tasks (Sensors & Application Logic)
// ===================================================

// LM35 Temperature Sensor Task
#define LM35_TASK_STACK_SIZE                4096
#define LM35_TASK_PRIORITY                  5
#define LM35_TASK_CORE_ID                   1

// Ultrasonic Sensor Task
#define ULTRASONIC_TASK_STACK_SIZE          4096
#define ULTRASONIC_TASK_PRIORITY            5
#define ULTRASONIC_TASK_CORE_ID             1

// SNTP Time Sync task
#define SNTP_TIME_SYNC_TASK_STACK_SIZE      4096
#define SNTP_TIME_SYNC_TASK_PRIORITY        4
#define SNTP_TIME_SYNC_TASK_CORE_ID         1

#define WIFI_RESET_BUTTON_TASK_STACK_SIZE   2048
#define WIFI_RESET_BUTTON_TASK_PRIORITY     6
#define WIFI_RESET_BUTTON_TASK_CORE_ID      0

#endif /* MAIN_TASKS_COMMON_H_ */