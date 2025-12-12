#include "service_wifi.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"

#define WIFI_SSID      "Nha N"
#define WIFI_PASS      "NhaN123@"
#define MAX_RETRY      5

static const char *TAG = "WIFI_SERVICE";
static int retry_num = 0;

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    // Start station 
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } 
    // Retry connecting to AP
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (retry_num < MAX_RETRY) {
            esp_wifi_connect();
            retry_num++;
            ESP_LOGI(TAG, "Retrying to connect to the AP");
        } else {
            ESP_LOGI(TAG, "Failed to connect to the AP after %d attempts", MAX_RETRY);
        }
    }
    // Successfully got IP
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        retry_num = 0;
    }
}

void wifi_service_start(void) {
    // 1. Wi-Fi/LwIP Init Phase
    // Initialize TCP/IP network interface
    ESP_ERROR_CHECK(esp_netif_init());
    // Create the default event loop needed by the Wi-Fi driver
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    // Create default Wi-Fi station
    ESP_ERROR_CHECK(esp_netif_create_default_wifi_sta());
    // Initialize the Wi-Fi driver with default configurations
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 2. Wi-Fi Configuration Phase
    // Register event handler for Wi-Fi events
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                        &wifi_event_handler, NULL,
                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                        &wifi_event_handler, NULL,
                                        &instance_got_ip));
    // Configure Wi-Fi connection and authentication settings
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        }
    };
    // Set Wi-Fi mode to Station
    esp_wifi_set_mode(WIFI_MODE_STA);
    // Set the Wi-Fi configuration
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);

    // 3. Wi-Fi Start Phase
    // Start the Wi-Fi driver
    esp_wifi_start();

    ESP_LOGI(TAG, "WiFi Service Started.");
}