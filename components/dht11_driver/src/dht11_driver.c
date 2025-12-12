#include "dht11_driver.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_err.h"
#include <string.h>
#include <rom/ets_sys.h>

static const char *TAG = "DHT11_DRIVER";

typedef struct {
    uint8_t pin;
    bool initialized;
    dht11_data_t last_read;
} dht11_context_t;

static dht11_context_t dht11_ctx = {0};

static esp_err_t dht11_wait_for_level(int target_level, uint32_t timeout_us)
{
    int64_t start_time = esp_timer_get_time();
    while ((esp_timer_get_time() - start_time) < timeout_us) {
        if (gpio_get_level(dht11_ctx.pin) == target_level) {
            return ESP_OK;
        }
    }
    return ESP_ERR_TIMEOUT;
}

static esp_err_t dht11_send_start_signal()
{
    // Set pin to output
    gpio_set_direction(dht11_ctx.pin, GPIO_MODE_OUTPUT);
    
    // Pull the pin low for at least 18ms
    gpio_set_level(dht11_ctx.pin, 0);
    ets_delay_us(20000); // 20ms

    // Pull the pin high for 20-40us
    gpio_set_level(dht11_ctx.pin, 1);
    ets_delay_us(40);

    // Set pin to input to read data
    gpio_set_direction(dht11_ctx.pin, GPIO_MODE_INPUT);

    return ESP_OK;
}

static bool dht11_validate_data(const uint8_t *data) {
    uint8_t checksum = data[0] + data[1] + data[2] + data[3];
    return (checksum == data[4]);
}

esp_err_t dht11_init(uint8_t pin)
{
    // Initialize the DHT11 sensor on the specified GPIO pin
    dht11_ctx.pin = pin;
    dht11_ctx.initialized = true;
    gpio_set_direction(pin, GPIO_MODE_INPUT_OUTPUT);
    ESP_LOGI(TAG, "DHT11 Sensor initialized on GPIO %d", pin);
    return ESP_OK;
}

esp_err_t dht11_read(dht11_data_t *data)
{
    uint8_t data_buffer[5] = {0};
    
    // Send start signal
    dht11_send_start_signal();

    // Wait for sensor response
    if (dht11_wait_for_level(0, 1000) != ESP_OK) return ESP_ERR_TIMEOUT;
    if (dht11_wait_for_level(1, 1000) != ESP_OK) return ESP_ERR_TIMEOUT;
    if (dht11_wait_for_level(0, 1000) != ESP_OK) return ESP_ERR_TIMEOUT;

    // Read 40 bits (5 bytes) of data
    for (int i = 0; i < 40; i++) {
        // Wait for bit start
        if (dht11_wait_for_level(1, 1000) != ESP_OK) return ESP_ERR_TIMEOUT;

        // Measure the length of the high pulse
        int64_t start_time = esp_timer_get_time();

        // Wait for bit end
        if (dht11_wait_for_level(0, 1000) != ESP_OK) return ESP_ERR_TIMEOUT;

        // Calculate pulse duration
        int64_t pulse_duration = esp_timer_get_time() - start_time;

        // Determine if bit is 0 or 1
        if (pulse_duration > 50) {
            data_buffer[i / 8] |= (1 << (7 - (i % 8)));
        }
    }

    if (dht11_validate_data(data_buffer)) {
        data->humidity = data_buffer[0] + data_buffer[1] * 0.1f;
        data->temperature = data_buffer[2] + data_buffer[3] * 0.1f;
        data->timestamp = (uint32_t)(esp_timer_get_time() / 1000);
    } else {
        return ESP_ERR_INVALID_CRC;
    }

    // Update context
    memcpy(&dht11_ctx.last_read, data, sizeof(dht11_data_t));
    
    return ESP_OK;
}

