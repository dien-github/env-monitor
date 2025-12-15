/**
 * @file driver_relay.c
 * @brief Driver for controlling a relay module.
 */

#include "driver_relay.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define RELAY_GPIO_MAX 39  // Maximum GPIO pin number for ESP32

static const char *TAG = "RELAY_DRIVER";

esp_err_t relay_init(relay_config_t *config)
{
    // Validate input
    if (config == NULL) {
        ESP_LOGE(TAG, "Config pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    // Validate GPIO pin number
    if (config->gpio_pin > RELAY_GPIO_MAX) {
        ESP_LOGE(TAG, "Invalid GPIO pin: %d (max: %d)", config->gpio_pin, RELAY_GPIO_MAX);
        return ESP_ERR_INVALID_ARG;
    }

    // Configure the GPIO pin for the relay
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << config->gpio_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    // Apply the GPIO configuration
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO %d: %s", config->gpio_pin, esp_err_to_name(ret));
        return ret;
    }

    // Set the initial state of the relay to OFF
    ret = relay_set_state(config, RELAY_STATE_OFF);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set initial state");
        return ret;
    }

    ESP_LOGI(TAG, "Relay initialized on GPIO %d (Type: %s)", 
             config->gpio_pin, 
             config->type == RELAY_ACTIVE_HIGH ? "Active-High" : "Active-Low");

    return ESP_OK;
}

esp_err_t relay_set_level(relay_config_t *config, uint32_t level) {
    if (config == NULL) return ESP_ERR_INVALID_ARG;
    
    // Xử lý Active High/Low
    uint32_t physical_level = level;
    if (config->type == RELAY_ACTIVE_LOW) {
        physical_level = !level;
    }

    return gpio_set_level(config->gpio_pin, physical_level);
}

esp_err_t relay_set_state(relay_config_t *config, relay_state_t state)
{
    // Validate input
    if (config == NULL) {
        ESP_LOGE(TAG, "Config pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    // Determine GPIO level based on relay type and desired state
    int level;
    if (config->type == RELAY_ACTIVE_HIGH) {
        level = (state == RELAY_STATE_ON) ? 1 : 0;
    } else { // RELAY_ACTIVE_LOW
        level = (state == RELAY_STATE_ON) ? 0 : 1;
    }

    // Set GPIO level and check return value
    esp_err_t ret = gpio_set_level(config->gpio_pin, level);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set GPIO %d level: %s", config->gpio_pin, esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

esp_err_t relay_get_state(relay_config_t *config, relay_state_t *state)
{
    // Validate input
    if (config == NULL || state == NULL) {
        ESP_LOGE(TAG, "Invalid parameter (NULL pointer)");
        return ESP_ERR_INVALID_ARG;
    }

    // Read GPIO level
    int level = gpio_get_level(config->gpio_pin);
    
    // Interpret level based on relay type
    if (config->type == RELAY_ACTIVE_HIGH) {
        *state = (level == 1) ? RELAY_STATE_ON : RELAY_STATE_OFF;
    } else { // RELAY_ACTIVE_LOW
        *state = (level == 0) ? RELAY_STATE_ON : RELAY_STATE_OFF;
    }
    
    return ESP_OK;
}

esp_err_t relay_toggle(relay_config_t *config)
{
    // Validate input
    if (config == NULL) {
        ESP_LOGE(TAG, "Config pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    // Get current state
    relay_state_t current_state;
    esp_err_t ret = relay_get_state(config, &current_state);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get current state");
        return ret;
    }

    // Toggle to opposite state
    relay_state_t new_state = (current_state == RELAY_STATE_ON) ? RELAY_STATE_OFF : RELAY_STATE_ON;
    
    return relay_set_state(config, new_state);
}

uint8_t relay_get_pin_number(relay_config_t *config)
{
    return config->gpio_pin;
}