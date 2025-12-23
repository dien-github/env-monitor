#ifndef SHT3X_H
#define SHT3X_H

#include "esp_err.h"
#include "driver/i2c.h"

#define I2C_MASTER_NUM          I2C_NUM_0
#define SHT3X_ADDR              0x44
#define SHT3X_CMD_MEASURE_HIGH  0x2C06

typedef struct {
    float temperature;
    float humidity;
} sht3x_data_t;

esp_err_t sht3x_init_i2c(int sda_pin, int scl_pin);

esp_err_t sht3x_read_data(float *temp, float *hum);

#endif /* SHT3X_H */