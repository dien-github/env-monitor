#include "service_mqtt.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "esp_crt_bundle.h"

#define MQTT_BROKER_URI "mqtts://6753deb1228e4cc3a9e2847294ddefda.s1.eu.hivemq.cloud"
#define MQTT_BROKER_PORT 8883
#define MQTT_BROKER_USERNAME "env-monitor"
#define MQTT_BROKER_PASSWORD "abcABC@123"

static const char *TAG = "MQTT_SERVICE";
static esp_mqtt_client_handle_t client = NULL;
static mqtt_data_callback_t data_callback = NULL;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, 
    int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            ESP_LOGI(TAG, "TOPIC=%.*s", event->topic_len, event->topic);
            ESP_LOGI(TAG, "DATA=%.*s", event->data_len, event->data);
            if (data_callback) {
                data_callback(event->topic, event->topic_len, event->data, event->data_len);
            }
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event_id);
            break;
    }
}

void mqtt_service_start(mqtt_data_callback_t callback)
{
    data_callback = callback;
    // 1. MQTT Client Initialization Phase
    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
        .broker.address.port = MQTT_BROKER_PORT,
        .broker.verification.crt_bundle_attach = esp_crt_bundle_attach,
        .credentials.username = MQTT_BROKER_USERNAME,
        .credentials.authentication.password = MQTT_BROKER_PASSWORD,
    };

    client = esp_mqtt_client_init(&mqtt_cfg);

    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID,
        mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

void mqtt_service_publish(const char* topic, const char* payload, int qos)
{
    if (client != NULL) {
        int msg_id = esp_mqtt_client_publish(client, topic, payload, 0, qos, 0);
        ESP_LOGI(TAG, "Published message with msg_id=%d", msg_id);
    } else {
        ESP_LOGW(TAG, "MQTT client not initialized");
    }
}

void mqtt_service_subscribe(const char* topic, int qos)
{
    if (client != NULL) {
        int msg_id = esp_mqtt_client_subscribe(client, topic, qos);
        ESP_LOGI(TAG, "Subscribed to topic %s with msg_id=%d", topic, msg_id);
    } else {
        ESP_LOGW(TAG, "MQTT client not initialized");
    }
}