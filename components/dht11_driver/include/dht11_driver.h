#ifndef DHT11_DRIVER_H
#define DHT11_DRIVER_H

#include <stdint.h>
#include "esp_err.h"

typedef struct
{
    uint8_t pin;
    float temperature;
    float humidity;
    uint64_t timestamp;
} dht11_data_t;

/**
 * @brief Configures the specified GPIO pin as Input/Output Open-Drain and initializes the internal driver context.
 * @param [in] pin GPIO number to which the DHT11 data line is connected.
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t dht11_init(uint8_t pin);

/**
 * @brief Executes the 1-Wire protocol (Start signal -> Response -> Data transmission) using precise microsecond delays to read 40 bits of data.
 * @param [out] data Pointer to a dht11_data_t structure to store the read temperature, humidity, and timestamp.
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t dht11_read(dht11_data_t *data);

#endif // DHT11_DRIVER_H