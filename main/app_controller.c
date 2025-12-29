/**
 * @file app_controller.c
 * 
 * @brief Application controller module
 * 
 */
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "cJSON.h"
#include "app_config.h"
#include "app_controller.h"

#define TASK_APPCTRL_STACK_SIZE     2048

static QueueHandle_t cmd_queue = NULL;
static TaskHandle_t humid_task_handle = NULL;
static TaskHandle_t fan_task_handle = NULL;
static const char *TAG = "APP_CTRL";

/**
 * Initialize queue and application controller task
 */
esp_err_t app_controller_init(void)
{
    // Queue chứa 10 app_cmd_t objects (by value)
    cmd_queue = xQueueCreate(10, sizeof(app_cmd_t));
    if (cmd_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create command queue");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Command queue created successfully");

    // Create application controller task
    esp_err_t ret = xTaskCreate(app_controller_task, "APP CTRL TASK", TASK_APPCTRL_STACK_SIZE, NULL, 4, NULL);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create application controller task");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Application controller task created");

    return ESP_OK;
}

void app_controller_set_humid_task_handle(TaskHandle_t handle)
{
    humid_task_handle = handle;
    ESP_LOGI(TAG, "Humidifier task handle set");
}

void app_controller_set_fan_task_handle(TaskHandle_t handle)
{
    fan_task_handle = handle;
    ESP_LOGI(TAG, "Fan task handle set");
}

bool app_controller_send_command(const char *json_str)
{
    app_cmd_t cmd = {0};
    bool result = false;

    cJSON *json = cJSON_Parse(json_str);
    if (json == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        return false;
    }

    cJSON *type = cJSON_GetObjectItem(json, "type");

    if (type && type->valuestring)
    {
        // --- CASE 1: FAN (state là number) ---
        if (strcasecmp(type->valuestring, "fan") == 0)
        {
            cmd.type = CMD_TYPE_FAN;
            cmd.value.fan_state[0] = '\0';
            cJSON *state = cJSON_GetObjectItem(json, "state");
            if (state && state->valuestring) {
                strncpy(cmd.value.fan_state, state->valuestring,
                    sizeof(cmd.value.fan_state) - 1);
                
                if (xQueueSend(cmd_queue, &cmd, 10) == pdTRUE) {
                    result = true;
                } else {
                    ESP_LOGE(TAG, "Failed to send FAN command to queue");
                }
            } else {
                ESP_LOGE(TAG, "Invalid or missing 'state' field for fan");
            }
        }
        // --- CASE 2: HUMIDIFIER (state là string "on"/"off") ---
        else if (strcasecmp(type->valuestring, "humidifier") == 0)
        {
            cmd.type = CMD_TYPE_HUMIDIFIER;
            cmd.value.humidifier_state[0] = '\0';
            cJSON *state = cJSON_GetObjectItem(json, "state");
            if (state && state->valuestring) {
                strncpy(cmd.value.humidifier_state, state->valuestring,
                    sizeof(cmd.value.humidifier_state) - 1);
                
                if (xQueueSend(cmd_queue, &cmd, 10) == pdTRUE) {
                    result = true;
                } else {
                    ESP_LOGE(TAG, "Failed to send HUMIDIFIER command to queue");
                }
            } else {
                ESP_LOGE(TAG, "Invalid or missing 'state' field for humidifier");
            }
        } else {
            ESP_LOGW(TAG, "Unknown device type: %s", type->valuestring);
        }
    } else {
        ESP_LOGE(TAG, "Missing or invalid 'type' field in JSON");
    }
    
    cJSON_Delete(json);
    return result;
}

void app_controller_task(void *pvParameters)
{
    app_cmd_t received_cmd;
    while (1)
    {
        if (xQueueReceive(cmd_queue, &received_cmd, portMAX_DELAY) == pdTRUE) {
            // Process received_cmd here
            ESP_LOGI(TAG, "Processing command type: %d", received_cmd.type);

            switch (received_cmd.type) {
                case CMD_TYPE_FAN:
                    ESP_LOGI(TAG, "Set fan state to %s", received_cmd.value.fan_state);
                    if (fan_task_handle != NULL)
                    {
                        uint32_t target_state = 0;

                        if (strcasecmp(received_cmd.value.fan_state, "on") == 0) {
                            target_state = 1;
                        } else {
                            target_state = 0;
                        }

                        xTaskNotify(fan_task_handle, target_state, eSetValueWithOverwrite);
                        ESP_LOGI(TAG, "Sent notification to fan task");
                    } else {
                        ESP_LOGW(TAG, "Fan task handle not set");
                    }
                    break;
                case CMD_TYPE_HUMIDIFIER:
                    ESP_LOGI(TAG, "Set humidifier state to %s", received_cmd.value.humidifier_state);
                    // Gửi notification đến relay task để toggle relay
                    if (humid_task_handle != NULL) {
                        uint32_t target_state = 0;

                        if (strcasecmp(received_cmd.value.humidifier_state, "on") == 0) {
                            target_state = 1;
                        } else {
                            target_state = 0;
                        }

                        xTaskNotify(humid_task_handle, target_state, eSetValueWithOverwrite);
                        ESP_LOGI(TAG, "Sent notification to relay task");
                    } else {
                        ESP_LOGW(TAG, "Relay task handle not set");
                    }
                    break;
                default:
                    ESP_LOGW(TAG, "Unknown command type received");
                    break;
            }
        }

        UBaseType_t high_water_mark = uxTaskGetStackHighWaterMark(NULL);
        ESP_LOGI(TAG, "Stack còn trống: %u bytes", high_water_mark);
    }
}