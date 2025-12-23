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
        "driver_relay"
        "esp_driver_gpio"
        "sht3x"
    PRIV_REQUIRES
        "json"
    INCLUDE_DIRS "."
)