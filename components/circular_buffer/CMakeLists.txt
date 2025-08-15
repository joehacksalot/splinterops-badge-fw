idf_component_register(
    SRCS "CircularBuffer.c"
    INCLUDE_DIRS "."
    REQUIRES esp_common freertos log
)
