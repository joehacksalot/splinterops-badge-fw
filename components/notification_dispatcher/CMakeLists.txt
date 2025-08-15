idf_component_register(
    SRCS "NotificationDispatcher.c"
    INCLUDE_DIRS "." "../../main/inc"
    REQUIRES esp_event freertos log
)
