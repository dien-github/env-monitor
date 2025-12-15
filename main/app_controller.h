/**
 * @file app_controller.h
 * 
 */

#ifndef APP_CONTROLLER_H
#define APP_CONTROLLER_H

#include "esp_err.h"
#include <stdbool.h>

esp_err_t app_controller_init(void);
void app_controller_set_relay_task_handle(TaskHandle_t handle);
bool app_controller_send_command(const char *json_str);
void app_controller_task(void *pvParameters);

#endif // APP_CONTROLLER_H