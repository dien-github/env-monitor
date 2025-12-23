/**
 * @file service_wifi.h
 * @brief Header file for WiFi service component.
 * 
 * This module responsible for handle WiFi connectivity.
 * Auto reconnect when disconnected.
 */

#ifndef SERVICE_WIFI_H
#define SERVICE_WIFI_H

#include "esp_err.h"

/**
 * @brief Initializes TCP/IP stack, creates default event loop, 
 *        and configures WiFi in Station mode using credentials from Kconfig.
 *        Registers event handlers for connection and IP acquisition.
 * @param None
 * @return None
 */
void wifi_service_start(void);

/**
 * @brief Blocks the calling task (typically Main Task) 
 *        using FreeRTOS Event Groups until the WiFi connection 
 *        is successfully established and an IP address is obtained.
 * @param None
 * @return None
 */
void wifi_service_wait_for_connect(void);

#endif // SERVICE_WIFI_H