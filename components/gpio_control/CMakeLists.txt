idf_component_register(
    SRCS "GpioControl.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_timer log
)
