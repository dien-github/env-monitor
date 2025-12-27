idf_component_register(
    SRCS
        "main.c"
        "app_controller.c"
    REQUIRES 
        "dht11_driver"
        "nvs_flash"
        "service_wifi"
        "service_mqtt"
        "esp_timer"
        "esp_wifi"
        "driver_sht3x"
        "driver_relay"
        "esp_driver_gpio"
    PRIV_REQUIRES
        "json"
    INCLUDE_DIRS "."
)