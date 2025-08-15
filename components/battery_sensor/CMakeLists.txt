idf_component_register(
    SRCS "BatterySensor.c"
    INCLUDE_DIRS "."
    REQUIRES esp_adc freertos log driver notification_dispatcher utilities
)
