idf_component_register(
    SRCS "src/service_wifi.c"
    INCLUDE_DIRS "include"
    REQUIRES "esp_wifi" "esp_event" "nvs_flash"
    PRIV_REQUIRES
)