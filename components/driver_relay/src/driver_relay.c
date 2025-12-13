#include "driver_relay.h"
#include "driver/gpio.h"

esp_err_t relay_init(relay_config_t *config)
{
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
        return ret;
    }

    // Set the initial state of the relay to OFF
    relay_set_state(config, RELAY_STATE_OFF);

    return ESP_OK;
}

esp_err_t relay_set_state(relay_config_t *config, relay_state_t state)
{
    if (config->type == RELAY_ACTIVE_HIGH) {
        gpio_set_level(config->gpio_pin, (state == RELAY_STATE_ON) ? 1 : 0);
    } else { // RELAY_ACTIVE_LOW
        gpio_set_level(config->gpio_pin, (state == RELAY_STATE_ON) ? 0 : 1);
    }
    return ESP_OK;
}

esp_err_t relay_get_state(relay_config_t *config, relay_state_t *state)
{
    int level = gpio_get_level(config->gpio_pin);
    if (config->type == RELAY_ACTIVE_HIGH) {
        *state = (level == 1) ? RELAY_STATE_ON : RELAY_STATE_OFF;
    } else { // RELAY_ACTIVE_LOW
        *state = (level == 0) ? RELAY_STATE_ON : RELAY_STATE_OFF;
    }
    return ESP_OK;
}

esp_err_t relay_toggle(relay_config_t *config)
{
    relay_state_t current_state;
    relay_get_state(config, &current_state);

    relay_state_t new_state = (current_state == RELAY_STATE_ON) ? RELAY_STATE_OFF : RELAY_STATE_ON;
    return relay_set_state(config, new_state);
}

uint8_t relay_get_pin_number(relay_config_t *config)
{
    return config->gpio_pin;
}