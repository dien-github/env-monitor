/**
 * @file app_config.h
 * 
 */

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stdint.h>

/**
 * Command types for controlling devices via MQTT
 */
typedef enum {
    CMD_TYPE_UNKNOWN,
    CMD_TYPE_FAN,
    CMD_TYPE_HUMIDIFIER,
} cmd_type_t;

/**
 * Relay command types
 */
typedef enum {
    RELAY_CMD_OFF = 0,
    RELAY_CMD_ON = 1,
    RELAY_CMD_TOGGLE = 2
} relay_cmd_t;

/**
 * Application command structure
 */
typedef struct {
    cmd_type_t type;
    union {
        int fan_speed;
        char humidifier_state[10];
    } value;
} app_cmd_t;

#endif // APP_CONFIG_H