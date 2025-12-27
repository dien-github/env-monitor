#include "freertos/FreeRTOS.h"
#include "service_wifi.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"

#define MAX_RETRY      10

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

wifi_config_t wifi_cfg = {
    .sta = {
        .ssid = CONFIG_WIFI_SSID,
        .password = CONFIG_WIFI_PASSWORD,
        .threshold.authmode = WIFI_AUTH_WPA2_PSK,
    }
};

static const char *TAG = "WIFI_SERVICE";
static int retry_num = 0;
static bool ever_connected = false;  // Track if WiFi has ever connected successfully
static EventGroupHandle_t s_wifi_event_group;

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    // Start station 
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    }
    // Retry connecting to AP
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        // If we've connected before, retry indefinitely (for reconnection after WiFi loss)
        if (ever_connected) {
            esp_wifi_connect();
            ESP_LOGI(TAG, "WiFi disconnected, retrying to reconnect...");
        }
        // If never connected, use limited retries (for initial connection)
        else if (retry_num < MAX_RETRY) {
            esp_wifi_connect();
            retry_num++;
            ESP_LOGI(TAG, "Retrying to connect to the AP (attempt %d/%d)", retry_num, MAX_RETRY);
        } else {
            ESP_LOGI(TAG, "Failed to connect to the AP after %d attempts", MAX_RETRY);
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    }
    // Successfully got IP
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        retry_num = 0;
        ever_connected = true;  // Mark that we've successfully connected
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_service_wait_for_connect(void)
{
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT,
            pdFALSE,
            pdTRUE,
            portMAX_DELAY);
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to AP");
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}

void wifi_service_start(void)
{
    // Initialize the Event Group
    s_wifi_event_group = xEventGroupCreate();

    // 1. Wi-Fi/LwIP Init Phase
    // Initialize TCP/IP network interface
    ESP_ERROR_CHECK(esp_netif_init());
    // Create the default event loop needed by the Wi-Fi driver
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    // Create default Wi-Fi station
    esp_netif_create_default_wifi_sta();
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
    // Set Wi-Fi mode to Station
    esp_wifi_set_mode(WIFI_MODE_STA);
    // Set the Wi-Fi configuration
    esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);

    // 3. Wi-Fi Start Phase
    // Start the Wi-Fi driver
    esp_wifi_start();

    ESP_LOGI(TAG, "WiFi Service Started.");
}