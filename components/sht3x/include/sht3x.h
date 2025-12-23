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

/**
 * @brief Initializes the ESP32 I2C master driver with 100kHz clock speed and internal pull-ups enabled.
 * @param [in] sda_pin GPIO number for SDA line.
 * @param [in] scl_pin GPIO number for SCL line.
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t sht3x_init_i2c(uint8_t sda_pin, uint8_t scl_pin);

/**
 * @brief Sends measurement command (High Repeatability), 
 *        waits 20ms for conversion, reads 6 bytes, 
 *        verifies CRC-8, and calculates physical values.
 * @param [out] temp Pointer to store the measured temperature in Celsius.
 * @param [out] hum Pointer to store the measured relative humidity in percentage.
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t sht3x_read_data(float *temp, float *hum);

#endif /* SHT3X_H */