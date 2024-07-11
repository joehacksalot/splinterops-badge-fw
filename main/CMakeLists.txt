idf_component_register( SRC_DIRS "src" "src/songs"
                        INCLUDE_DIRS "inc"
                        REQUIRES esp_wifi esp_event esp_netif esp_eth esp_phy lwip vfs efuse esp_timer driver fatfs console_cmds led_strip json esp_http_client log freertos esp-tls esp_https_ota app_update esp_adc esp_netif bt)

# add_definitions(-DTRON_BADGE)
# add_definitions(-DREACTOR_BADGE)
add_definitions(-DCREST_BADGE)