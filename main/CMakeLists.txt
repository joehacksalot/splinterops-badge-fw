idf_component_register(
    SRC_DIRS "src"
    INCLUDE_DIRS "inc"
    REQUIRES
        # ESP-IDF core/networking
        esp_wifi
        esp_event
        esp_netif
        esp_eth
        esp_phy
        lwip
        vfs
        efuse
        esp_timer
        driver
        log
        freertos
        esp-tls
        esp_http_client
        esp_https_ota
        app_update
        esp_adc
        bt

        # SplinterOps components
        notification_dispatcher
        circular_buffer
        utilities
        battery_sensor
        disk_utilities
        badge_metrics
        json_utils
        hashmap
        piezo_music
        gpio_control
        mutex
        time_utils
        touch_sensor
        touch_actions
        badge_hw_profile
        wifi_client
        console_cmds
        led_strip
        ota
        user_settings
        game_types
)

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