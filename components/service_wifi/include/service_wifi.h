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

void wifi_service_start(void);

#endif // SERVICE_WIFI_H