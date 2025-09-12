idf_component_register(
    SRCS "HTTPRequest.c"
    INCLUDE_DIRS "."
    REQUIRES freertos wifi_client vector
    PRIV_REQUIRES esp_http_client esp-tls app_update esp_https_ota json log time_utils utilities
)
