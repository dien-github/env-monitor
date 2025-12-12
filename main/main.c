#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "cJSON.h"

#include "dht11_driver.h"
#include "service_wifi.h"
#include "service_mqtt.h"

#define CONFIG_DHT11_PIN 4
#define CONFIG_DHT11_CONNECTION_TIMEOUT 5

#define TOPIC_SENSOR_PUB "room_01/sensors" // {"temperature": xx.x, "humidity": yy.y }
#define TOPIC_COMMAND_SUB "room_01/commands" // {"type": "fan", "state": xx} / {"type": "humidifier", "state": "on"/"off"}
#define TOPIC_STATUS_PUB "room_01/status"
#define TOPIC_ERROR_PUB "room_01/errors"


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

        // 3. Parse JSON
        cJSON *json = cJSON_Parse(json_str);
        if (json == NULL) {
            ESP_LOGE("MQTT", "Failed to parse JSON payload");
            free(json_str); // Quan trọng: Free chuỗi trước khi return
            return;
        }

        // 4. Lấy field "type" và "state"
        cJSON *type = cJSON_GetObjectItem(json, "type");
        cJSON *state = cJSON_GetObjectItem(json, "state");

        if (cJSON_IsString(type) && (type->valuestring != NULL))
        {
            // --- CASE 1: FAN (state là number) ---
            if (strcmp(type->valuestring, "fan") == 0)
            {
                if (cJSON_IsNumber(state)) {
                    int fan_speed = state->valueint;
                    ESP_LOGI("COMMAND", "Set FAN state to %d", fan_speed);
                    // TODO: fan_control(fan_speed);
                } else {
                    ESP_LOGE("COMMAND", "Invalid state for FAN (expected Number)");
                }
            }
            // --- CASE 2: HUMIDIFIER (state là string "on"/"off") ---
            else if (strcmp(type->valuestring, "humidifier") == 0)
            {
                if (cJSON_IsString(state) && (state->valuestring != NULL)) {
                    char *humid_state = state->valuestring;
                    ESP_LOGI("COMMAND", "Set HUMIDIFIER state to %s", humid_state);
                    // TODO: humidifier_control(humid_state);
                } else {
                    ESP_LOGE("COMMAND", "Invalid state for HUMIDIFIER (expected String)");
                }
            }
            // --- CASE: UNKNOWN TYPE ---
            else {
                ESP_LOGW("COMMAND", "Unknown device type: %s", type->valuestring);
            }
        }
        else {
            ESP_LOGE("COMMAND", "JSON missing 'type' field or type is not string");
        }

        // 5. Dọn dẹp bộ nhớ (Bắt buộc)
        cJSON_Delete(json); // Xóa object JSON
        free(json_str);     // Xóa chuỗi copy
    }
    else 
    {
        // Xử lý các topic khác nếu cần
        ESP_LOGD("MQTT", "Data received on ignored topic");
    }
}

void publish_sensor_data(float temp, float hum)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "temp", temp);
    cJSON_AddNumberToObject(root, "hum", hum);
    cJSON_AddNumberToObject(root, "ts", (double)(esp_timer_get_time() / 1000000));

    char *json_string = cJSON_PrintUnformatted(root);
    mqtt_service_publish(TOPIC_SENSOR_PUB, json_string, 1);
    
    free(json_string);
    cJSON_Delete(root);
}

void app_main(void)
{
    // 1. Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    // 2. Start Wi-Fi Service
    wifi_service_start();
    
    // Wait for Wi-Fi connection to establish
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    // 3. Start MQTT Service
    mqtt_service_start(on_mqtt_data_received);

    vTaskDelay(pdMS_TO_TICKS(5000)); 
    mqtt_service_subscribe(TOPIC_COMMAND_SUB, 1);
    
    // 4. Create DHT11 Task
    // xTaskCreate(dht11_task, "DHT11 TASK", 2048, NULL, 5, NULL);
    char data_buffer[50];
    float fake_temp = 0, fake_hum = 0;

    while (1) {
        // Simulate reading from DHT11
        fake_temp += 0.5;
        fake_hum += 1.0;
        if (fake_temp > 30.0) fake_temp = 20.0;
        if (fake_hum > 90.0) fake_hum = 40.0;
        publish_sensor_data(fake_temp, fake_hum);

        // snprintf(data_buffer, sizeof(data_buffer), "{\"temperature\": %.2f, \"humidity\": %.2f}", fake_temp, fake_hum);
        // mqtt_service_publish("env_monitor/dht11", data_buffer);
        // ESP_LOGI("MQTT", "Published: %s", data_buffer);

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}