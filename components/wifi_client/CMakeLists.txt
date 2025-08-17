idf_component_register(
    SRCS "WifiClient.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_netif esp_event freertos log notification_dispatcher time_utils utilities
)
