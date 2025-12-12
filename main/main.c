#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "dht11_driver.h"

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
    xTaskCreate(
        dht11_task,
        "DHT11 TASK",
        2048,
        NULL,
        5,
        NULL
    );
}