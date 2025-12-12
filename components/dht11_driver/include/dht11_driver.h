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

esp_err_t dht11_init(uint8_t pin);

esp_err_t dht11_read(dht11_data_t *data);

#endif // DHT11_DRIVER_H