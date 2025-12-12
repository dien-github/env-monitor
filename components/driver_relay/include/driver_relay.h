#ifndef DRIVER_RELAY_H
#define DRIVER_RELAY_H

#include "esp_err.h"

typedef enum {
    RELAY_STATE_OFF,
    RELAY_STATE_ON
} relay_state_t;

typedef enum {
    RELAY_ACTIVE_HIGH,
    RELAY_ACTIVE_LOW
} relay_type_t;

typedef struct {
    uint8_t gpio_pin;
    relay_type_t type;
} relay_config_t;

/**
 * @brief Initialize the relay with the given configuration.
 * @param config Pointer to the relay configuration structure.
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t relay_init(relay_config_t *config);

/**
 * @brief Set the state of the relay.
 * @param config Pointer to the relay configuration structure.
 * @param state Desired state of the relay (RELAY_STATE_ON or RELAY_STATE_OFF).
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t relay_set_state(relay_config_t *config, relay_state_t state);

/**
 * @brief Get the current state of the relay.
 * @param config Pointer to the relay configuration structure.
 * @param state Pointer to store the current state of the relay.
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t relay_get_state(relay_config_t *config, relay_state_t *state);

/**
 * @brief Toggle the state of the relay.
 * @param config Pointer to the relay configuration structure.
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t relay_toggle(relay_config_t *config);

/**
 * @brief Get the GPIO pin number associated with the relay.
 * @param config Pointer to the relay configuration structure.
 * @return GPIO pin number.
 */
uint8_t relay_get_pin_number(relay_config_t *config);

#endif // DRIVER_RELAY_H