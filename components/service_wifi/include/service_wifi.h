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
 * @brief Start the Wi-Fi service.
 */
void wifi_service_start(void);

/**
 * @brief Wait until the device is connected to Wi-Fi.
 */
void wifi_service_wait_for_connect(void);

#endif // SERVICE_WIFI_H