#ifndef SERVICE_MQTT_H
#define SERVICE_MQTT_H

/**
 * @brief Start the MQTT service.
 * 
 * This function initializes and starts the MQTT client,
 * connecting to the configured MQTT broker.
 */
void mqtt_service_start(void);

/**
 * @brief Publish a message to a specified MQTT topic.
 * 
 * @param topic The MQTT topic to publish to.
 * @param payload The message payload to publish.
 */
void mqtt_service_publish(const char* topic, const char* payload);

#endif // SERVICE_MQTT_H