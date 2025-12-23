#include "sht3x.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "SHT3X";

// Hàm tính CRC-8 (Polynomial: 0x31, Init: 0xFF)
static uint8_t sht3x_calc_crc(uint8_t *data, size_t len) {
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x31;
            } else {
                crc = crc << 1;
            }
        }
    }
    return crc;
}

esp_err_t sht3x_init_i2c(int sda_pin, int scl_pin)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda_pin,
        .scl_io_num = scl_pin,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };

    esp_err_t err = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (err != ESP_OK) {
        return err;
    }

    return i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

esp_err_t sht3x_read_data(float *temp, float *hum)
{
    uint8_t cmd[2] = {
        (SHT3X_CMD_MEASURE_HIGH >> 8) & 0xFF,
        SHT3X_CMD_MEASURE_HIGH & 0xFF
    };
    uint8_t data[6];

    // 1. Send Write command
    i2c_cmd_handle_t link = i2c_cmd_link_create();
    i2c_master_start(link);
    i2c_master_write_byte(link, (SHT3X_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(link, cmd, 2, true);
    i2c_master_stop(link);
    esp_err_t err = i2c_master_cmd_begin(I2C_MASTER_NUM, link, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(link);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C Write Failed");
        return err;
    }

    // Wait for measurement
    vTaskDelay(pdMS_TO_TICKS(20));

    // Read data
    link = i2c_cmd_link_create();
    i2c_master_start(link);
    i2c_master_write_byte(link, (SHT3X_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read(link, data, 6, I2C_MASTER_LAST_NACK);
    i2c_master_stop(link);
    err = i2c_master_cmd_begin(I2C_MASTER_NUM, link, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(link);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C Read Failed");
        return err;
    }

    // Check CRC
    if (data[2] != sht3x_calc_crc(data, 2) || data[5] != sht3x_calc_crc(&data[3], 2)) {
        ESP_LOGE(TAG, "CRC Check Failed");
        return ESP_ERR_INVALID_CRC;
    }

    // Change raw data to temperature and humidity
    uint16_t raw_temp = (data[0] << 8) | data[1];
    uint16_t raw_hum = (data[3] << 8) | data[4];

    *temp = -45.0f + (175.0f * (float)raw_temp / 65535.0f);
    *hum = 100.0f * ((float)raw_hum / 65535.0f);

    return ESP_OK;
}