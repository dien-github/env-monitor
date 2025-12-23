#ifndef SERVICE_MQTT_H
#define SERVICE_MQTT_H

typedef void (*mqtt_data_callback_t)(const char* topic, int topic_len, const char* data, int data_len);

/**
 * @brief Initializes the MQTT client with URI, Port (8883), and TLS credentials. 
 *        Registers a global event handler and starts the MQTT client task.
 * @param callback Function pointer for handling incoming MQTT messages.
 * @return None
 */
void mqtt_service_start(mqtt_data_callback_t callback);

/**
 * @brief Publishes a message to a specific topic. 
 *        Checks if the client is initialized before calling
 *        the underlying ESP-MQTT publish function.
 * @param topic The MQTT topic to publish to.
 * @param payload The message payload to publish.
 * @param qos The Quality of Service level for the message.
 * @return None
 */
void mqtt_service_publish(const char* topic, const char* payload, int qos);

/**
 * @brief Subscribes the client to a specific topic to receive incoming
 *        messages. Status is logged upon successful subscription.
 * 
 * @param topic The MQTT topic to subscribe to.
 * @param qos The Quality of Service level for the subscription.
 */
void mqtt_service_subscribe(const char* topic, int qos);

#endif // SERVICE_MQTT_H