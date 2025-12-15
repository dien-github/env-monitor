#ifndef SERVICE_MQTT_H
#define SERVICE_MQTT_H

typedef void (*mqtt_data_callback_t)(const char* topic, int topic_len, const char* data, int data_len);

/**
 * @brief Start the MQTT service.
 * 
 * This function initializes and starts the MQTT client,
 * connecting to the configured MQTT broker.
 * @param callback Function pointer for handling incoming MQTT messages.
 */
void mqtt_service_start(mqtt_data_callback_t callback);

/**
 * @brief Publish a message to a specified MQTT topic.
 * 
 * @param topic The MQTT topic to publish to.
 * @param payload The message payload to publish.
 * @param qos The Quality of Service level for the message.
 */
void mqtt_service_publish(const char* topic, const char* payload, int qos);

/**
 * @brief Subscribe to a specified MQTT topic.
 * 
 * @param topic The MQTT topic to subscribe to.
 * @param qos The Quality of Service level for the subscription.
 */
void mqtt_service_subscribe(const char* topic, int qos);

#endif // SERVICE_MQTT_H