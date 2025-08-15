idf_component_register( SRC_DIRS "src" "src/songs"
                        INCLUDE_DIRS "inc"
                        REQUIRES esp_wifi esp_event esp_netif esp_eth esp_phy lwip vfs efuse esp_timer driver fatfs console_cmds led_strip json esp_http_client log freertos esp-tls esp_https_ota app_update esp_adc esp_netif bt notification_dispatcher circular_buffer)

if(CONFIG_BADGE_TYPE_TRON)
    add_definitions(-DTRON_BADGE)
elseif(CONFIG_BADGE_TYPE_REACTOR)
    add_definitions(-DREACTOR_BADGE)
elseif(CONFIG_BADGE_TYPE_CREST)
    add_definitions(-DCREST_BADGE)
elseif(CONFIG_BADGE_TYPE_FMAN25)
    add_definitions(-DFMAN25_BADGE)
else()
    add_definitions(-DFMAN25_BADGE)
endif()