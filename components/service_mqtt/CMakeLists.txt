idf_component_register(
    SRCS "src/service_mqtt.c"
    INCLUDE_DIRS "include"
    REQUIRES mqtt
    PRIV_REQUIRES mbedtls
)