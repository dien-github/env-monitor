#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_err.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "cJSON.h"

#include "app_config.h"
#include "app_controller.h"
#include "dht11_driver.h"
#include "driver_relay.h"
#include "service_wifi.h"
#include "service_mqtt.h"
#include "driver_sht3x.h"

#define CONFIG_DHT11_PIN    4
#define CONFIG_DHT11_CONNECTION_TIMEOUT 5

#define CONFIG_HUMID_PIN    16
#define CONFIG_HUMID_TYPE   RELAY_ACTIVE_HIGH

#define CONFIG_FAN_PIN      18
#define CONFIG_FAN_TYPE     RELAY_ACTIVE_HIGH

#define CONFIG_SHT3X_PERIOD_MS 2000
#define CONFIG_SDA_PIN      21
#define CONFIG_SCL_PIN      22

#define TOPIC_SENSOR_PUB            "room_01/sensors" // {"temperature": xx.x, "humidity": yy.y }
#define TOPIC_COMMAND_SUB           "room_01/commands" // {"type": "fan", "state": "on"/"off"} / {"type": "humidifier", "state": "on"/"off"}
#define TOPIC_STATUS_SYSTEM_PUB     "room_01/status/system"
#define TOPIC_STATUS_CONNECTION_PUB "room_01/status/connection"
#define TOPIC_STATUS_DEVICE_PUB     "room_01/status/devices"
#define TOPIC_ERROR_PUB             "room_01/errors"

#define APP_TASK_STACK_SIZE     4096
#define APP_TASK_PRIORITY       5
#define SENSOR_TASK_STACK_SIZE  2048
#define SENSOR_TASK_PRIORITY    4
#define TASK_SHT3X_STACK_SIZE   3072    /* 3 bytes */

static TaskHandle_t humid_task_handle = NULL;
static TaskHandle_t fan_task_handle = NULL;
static const char *TAG = "MAIN";

void publish_sensor_data(float temp, float hum)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        ESP_LOGE("MQTT", "Failed to create JSON object");
        return;
    }
    
    cJSON_AddNumberToObject(root, "temperature", temp);
    cJSON_AddNumberToObject(root, "humidity", hum);
    cJSON_AddNumberToObject(root, "timestamp", (double)(esp_timer_get_time() / 1000000));

    char *json_string = cJSON_PrintUnformatted(root);
    if (json_string != NULL) {
        mqtt_service_publish(TOPIC_SENSOR_PUB, json_string, 1);
        free(json_string);
    } else {
        ESP_LOGE("MQTT", "Failed to print JSON");
    }
    
    cJSON_Delete(root);
}

/**
 * @brief Publish device status to MQTT
 * - topic: room_01/status/devices
 * - goal: Report the actual state of actuators after receiving a command
 * - payload: {"device": "fan", "state": "on", "timestamp": 1234}
 * - qos: 1
 * - retain: FALSE
 * - trigger: Immediately after relay_set_level successfully executes the command in each task |
 */
void publish_device_status(const char *device, const char *status)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        ESP_LOGE("MQTT", "Failed to create JSON object for status message");
        return;
    }

    cJSON_AddStringToObject(root, "device", device);
    cJSON_AddStringToObject(root, "state", status);
    cJSON_AddNumberToObject(root, "timestamp", (double)(esp_timer_get_time() / 1000000));

    char *json_string = cJSON_PrintUnformatted(root);
    if (json_string != NULL) {
        mqtt_service_publish(TOPIC_STATUS_DEVICE_PUB, json_string, 1);
        free(json_string);
    } else {
        ESP_LOGE("MQTT", "Failed to print JSON for status message");
    }

    cJSON_Delete(root);
}

/* dht11 task
void dht11_task(void *pvParameters)
{
    dht11_data_t data;
    data.pin = CONFIG_DHT11_PIN;

    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(CONFIG_DHT11_CONNECTION_TIMEOUT * 1000);

    dht11_init(data.pin);
    
    while (1) {
        vTaskDelayUntil(&last_wake_time, period);

        esp_err_t err = dht11_read(&data);
        if (err == ESP_OK) {
            ESP_LOGI("DHT11", "Temperature: %.2f C, Humidity: %.2f %%", data.temperature, data.humidity);
        } else {
            ESP_LOGE("DHT11", "Failed to read data from DHT11 sensor: %s", esp_err_to_name(err));
        }

        publish_sensor_data(data.temperature, data.humidity);
    }
}
*/

void sht3x_task(void *pvParameters) {
    float temperature = 0.0;
    float humidity = 0.0;

    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(CONFIG_SHT3X_PERIOD_MS);

    while (1) {
        /* Wait for the next cycle */
        vTaskDelayUntil(&last_wake_time, period);

        // Gọi hàm đo single shot
        esp_err_t res = sht3x_read_data(&temperature, &humidity);

        if (res == ESP_OK) {
            ESP_LOGI(TAG, "Temp: %.2f °C, Hum: %.2f %%", temperature, humidity);
        } else {
            ESP_LOGE(TAG, "SHT3x read error: %s", esp_err_to_name(res));
        }

        publish_sensor_data(temperature, humidity);

        UBaseType_t high_water_mark = uxTaskGetStackHighWaterMark(NULL);
        ESP_LOGI("SENSOR TASK", "Stack high water mark: %u bytes", high_water_mark);
    }
}

void humid_task(void *pvParameters)
{
    relay_config_t relay_cfg;
    relay_cfg.gpio_pin = CONFIG_HUMID_PIN;
    relay_cfg.type = CONFIG_HUMID_TYPE;

    esp_err_t err = relay_init(&relay_cfg);
    if (err != ESP_OK) {
        ESP_LOGE("RELAY_TASK", "Failed to initialize relay: %s", esp_err_to_name(err));
        vTaskDelete(NULL);
    }

    uint32_t received_state = 0;
    relay_state_t last_state = RELAY_STATE_OFF;  // Track actual relay state

    while (1) {
        BaseType_t res = xTaskNotifyWait(0, ULONG_MAX, &received_state, portMAX_DELAY);

        if (res == pdTRUE) {
            ESP_LOGI("HUMID_TASK", "Received command set state: %lu", received_state);
            
            esp_err_t ret = relay_set_level(&relay_cfg, received_state);
            if (ret != ESP_OK) {
                ESP_LOGE("HUMID_TASK", "Failed to set relay: %s", esp_err_to_name(ret));
            } else {
                // Get actual relay state
                relay_state_t current_state;
                ret = relay_get_state(&relay_cfg, &current_state);
                if (ret == ESP_OK) {
                    // Only publish if state changed
                    if (current_state != last_state) {
                        last_state = current_state;
                        publish_device_status("humidifier", current_state == RELAY_STATE_ON ? "on" : "off");
                        ESP_LOGI("HUMID_TASK", "Relay state updated to: %s", current_state == RELAY_STATE_ON ? "ON" : "OFF");
                    }
                } else {
                    ESP_LOGE("HUMID_TASK", "Failed to get relay state: %s", esp_err_to_name(ret));
                }
            }
        }

        UBaseType_t high_water_mark = uxTaskGetStackHighWaterMark(NULL);
        ESP_LOGI("HUMID_TASK", "Stack high water mark: %u bytes", high_water_mark);
    }
}

void fan_task(void *pvParameters)
{
    relay_config_t relay_cfg;
    relay_cfg.gpio_pin = CONFIG_FAN_PIN;
    relay_cfg.type = CONFIG_FAN_TYPE;

    esp_err_t err = relay_init(&relay_cfg);
    if (err != ESP_OK) {
        ESP_LOGE("FAN_TASK", "Failed to initialize relay: %s", esp_err_to_name(err));
        vTaskDelete(NULL);
    }

    uint32_t received_state = 0;
    relay_state_t last_state = RELAY_STATE_OFF;  // Track actual relay state

    while (1) {
        BaseType_t res = xTaskNotifyWait(0, ULONG_MAX, &received_state, portMAX_DELAY);

        if (res == pdTRUE) {
            ESP_LOGI("FAN_TASK", "Received command set state: %lu", received_state);
            
            esp_err_t ret = relay_set_level(&relay_cfg, received_state);
            if (ret != ESP_OK) {
                ESP_LOGE("FAN_TASK", "Failed to set relay: %s", esp_err_to_name(ret));
            } else {
                // Get actual relay state
                relay_state_t current_state;
                ret = relay_get_state(&relay_cfg, &current_state);
                if (ret == ESP_OK) {
                    // Only publish if state changed
                    if (current_state != last_state) {
                        last_state = current_state;
                        publish_device_status("fan", current_state == RELAY_STATE_ON ? "on" : "off");
                        ESP_LOGI("FAN_TASK", "Relay state updated to: %s", current_state == RELAY_STATE_ON ? "ON" : "OFF");
                    }
                } else {
                    ESP_LOGE("FAN_TASK", "Failed to get relay state: %s", esp_err_to_name(ret));
                }
            }
        } else {
            ESP_LOGW("FAN_TASK", "xTaskNotifyWait failed");
        }

        UBaseType_t high_water_mark = uxTaskGetStackHighWaterMark(NULL);
        ESP_LOGI("FAN_TASK", "Stack high water mark: %u bytes", high_water_mark);
    }
}

void on_mqtt_data_received(const char *topic, int topic_len, const char *payload, int payload_len)
{
    ESP_LOGI("MQTT_DEBUG", "Received data on topic %.*s: %.*s", topic_len, topic, payload_len, payload);

    // 1. Kiểm tra Topic trước để tiết kiệm bộ nhớ (không alloc nếu sai topic)
    if (topic_len == strlen(TOPIC_COMMAND_SUB) && strncmp(topic, TOPIC_COMMAND_SUB, topic_len) == 0)
    {
        // 2. Chuyển payload thành chuỗi null-terminated an toàn
        char *json_str = strndup(payload, payload_len);
        if (json_str == NULL) {
            ESP_LOGE("MQTT", "Failed to allocate memory for JSON string");
            return;
        }
        bool result = app_controller_send_command(json_str);
        if (result) {
            ESP_LOGI("MQTT", "Command processed successfully");
        } else {
            ESP_LOGW("MQTT", "Failed to process command");
        }
        free(json_str);
    }
}

void sensor_cycle_task(void *pvParameters)
{
    float fake_temp = 20.0, fake_hum = 60.0;
    while (1) {
        // Logic đọc DHT11 thật sẽ đặt ở đây
        fake_temp += 0.5;
        if (fake_temp > 35.0) fake_temp = 20.0;
        
        publish_sensor_data(fake_temp, fake_hum);
        
        // Dùng vTaskDelay thay vì busy wait
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void print_all_tasks(void *pvParameters) {
    while (1) {
        char buffer[1024];
        vTaskList(buffer);  // Liệt kê tất cả task + stack usage
        ESP_LOGI(TAG, "Danh sách task:\n%s", buffer);
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

void publish_health_check_params(health_check_params_t *params)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        ESP_LOGE("HEALTH_CHECK_MQTT", "Failed to create JSON object for health check");
        return;
    }

    cJSON_AddNumberToObject(root, "uptime_ms", params->uptime_ms);
    cJSON_AddNumberToObject(root, "free_heap", params->free_heap_bytes);
    cJSON_AddNumberToObject(root, "wifi_rssi", params->wifi_rssi);

    char *json_string = cJSON_PrintUnformatted(root);
    if (json_string != NULL) {
        mqtt_service_publish(TOPIC_STATUS_SYSTEM_PUB, json_string, 1);
        free(json_string);
    } else {
        ESP_LOGE("HEALTH_CHECK_MQTT", "Failed to print JSON for health check");
    }

    cJSON_Delete(root);
}

void health_check_task(void *pvParameters)
{
    health_check_params_t params;

    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(CONFIG_HEALTH_CHECK_PERIOD_MS);

    while (1)
    {
        vTaskDelayUntil(&last_wake_time, period);

        ESP_LOGI("HEALTH_CHECK", "Performing health check...");

        /* Get uptime */
        params.uptime_ms = esp_timer_get_time() / 1000;
        ESP_LOGI("HEALTH_CHECK", "Uptime (ms): %u", params.uptime_ms);

        /* Get free_heap*/
        params.free_heap_bytes = esp_get_free_heap_size();
        ESP_LOGI("HEALTH_CHECK", "Free heap (bytes): %u", params.free_heap_bytes);
        
        /* Get rssi */
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) != ESP_OK) {
            ESP_LOGE("HEALTH_CHECK", "Failed to get AP info for RSSI");
            params.wifi_rssi = -100; // Default bad RSSI
        } else {
            params.wifi_rssi = ap_info.rssi;
            ESP_LOGI("HEALTH_CHECK", "WiFi RSSI (dBm): %d", params.wifi_rssi);
        }

        /* Publishing to topic */
        publish_health_check_params(&params);

        UBaseType_t high_water_mark = uxTaskGetStackHighWaterMark(NULL);
        ESP_LOGI("FAN_TASK", "Stack high water mark: %u bytes", high_water_mark);
    }
}

void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Start Wi-Fi Service
    wifi_service_start();
    
    // Wait for Wi-Fi connection to establish
    // TODO Change waiting method
    // => NEED TO CHANGE INTO 
    // if not connect => change to another state just sensor and save data to NVS 
    // send data when have wifi
    // vTaskDelay(pdMS_TO_TICKS(10000));
    wifi_service_wait_for_connect();
    
    // Initialize App Controller
    app_controller_init();

    // Start MQTT Service
    mqtt_service_start(on_mqtt_data_received);

    vTaskDelay(pdMS_TO_TICKS(5000)); 
    mqtt_service_subscribe(TOPIC_COMMAND_SUB, 1);
    
    // Create Relay - HUMID Task
    xTaskCreate(humid_task, "HUMID TASK", 2048, NULL, 5, &humid_task_handle);
    // Set relay task handle in app_controller
    app_controller_set_humid_task_handle(humid_task_handle);
    
    // Create Relay - FAN Task
    xTaskCreate(fan_task, "FAN TASK", 2048, NULL, 5, &fan_task_handle);
    // Set fan task handle in app_controller
    app_controller_set_fan_task_handle(fan_task_handle);

    // Create Sensor Cycle Task
    // xTaskCreate(sensor_cycle_task, "SENSOR", SENSOR_TASK_STACK_SIZE, NULL, SENSOR_TASK_PRIORITY, NULL);
    
    // Create DHT11 Task
    // xTaskCreate(dht11_task, "DHT11 TASK", 2048, NULL, 5, NULL);

    ESP_ERROR_CHECK(sht3x_init_i2c(CONFIG_SDA_PIN, CONFIG_SCL_PIN));
    ESP_LOGI(TAG, "I2C Initialized");
    xTaskCreate(sht3x_task, "SHT3X TASK", TASK_SHT3X_STACK_SIZE, NULL, 5, NULL);

    // xTaskCreate(print_all_tasks, "TASK_LIST", 2048, NULL, 1, NULL);
    xTaskCreate(health_check_task, "HEALTH_CHECK_TASK", 4096, NULL, 6, NULL);
}