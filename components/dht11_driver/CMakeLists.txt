idf_component_register(
    SRCS "src/dht11_driver.c"
    INCLUDE_DIRS "include"
    REQUIRES "driver"
    PRIV_REQUIRES "esp_timer"
)