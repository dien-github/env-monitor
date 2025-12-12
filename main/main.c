#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "dht11_driver.h"
#include "service_wifi.h"
#include "service_mqtt.h"

#define CONFIG_DHT11_PIN 4
#define CONFIG_DHT11_CONNECTION_TIMEOUT 5

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
    mqtt_service_start();
    
    // 4. Create DHT11 Task
    // xTaskCreate(dht11_task, "DHT11 TASK", 2048, NULL, 5, NULL);
    char data_buffer[50];
    float fake_temp, fake_hum;

    while (1) {
        // Simulate reading from DHT11
        fake_temp += 0.5;
        fake_hum += 1.0;
        if (fake_temp > 30.0) fake_temp = 20.0;
        if (fake_hum > 90.0) fake_hum = 40.0;

        snprintf(data_buffer, sizeof(data_buffer), "{\"temperature\": %.2f, \"humidity\": %.2f}", fake_temp, fake_hum);
        mqtt_service_publish("env_monitor/dht11", data_buffer);
        ESP_LOGI("MQTT", "Published: %s", data_buffer);

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}