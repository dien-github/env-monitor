idf_component_register(
    SRCS "main.c"
    REQUIRES 
        "dht11_driver"
        "nvs_flash"
        "service_wifi"
        "service_mqtt"
        "esp_timer"
        "driver_relay"
    PRIV_REQUIRES
        "json"
    INCLUDE_DIRS "."
)