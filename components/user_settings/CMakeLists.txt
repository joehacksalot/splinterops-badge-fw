idf_component_register(
    SRCS "UserSettings.c"
    INCLUDE_DIRS "."
    REQUIRES
        battery_sensor         # uses BatterySensor handle for disk writes
        disk_utilities         # WriteFileToDisk()
        utilities              # common helpers/macros
        freertos               # tasks, semaphores
        log                    # ESP logging
        json                   # cJSON library
        mbedtls                # base64 encoding
        esp_hw_support         # sha_parallel_engine.h, esp_mac.h
        wifi_client            # WifiSettings struct
        game_types             # GameTypes.h
)
