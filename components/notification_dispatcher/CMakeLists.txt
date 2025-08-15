idf_component_register(
    SRCS "NotificationDispatcher.c"
    INCLUDE_DIRS "."
    REQUIRES esp_event freertos log
)
