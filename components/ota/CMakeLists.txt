idf_component_register(
    SRCS "OtaUpdate.c"
    INCLUDE_DIRS "."
    REQUIRES wifi_client notification_dispatcher utilities badge_hw_profile esp_https_ota esp_http_client esp-tls app_update freertos log
)
