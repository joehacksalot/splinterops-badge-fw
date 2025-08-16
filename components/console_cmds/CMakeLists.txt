idf_component_register(
    SRC_DIRS "system_cmds" "badge_cmds" "console"
    INCLUDE_DIRS "system_cmds" "badge_cmds" "console"
    REQUIRES console nvs_flash spi_flash utilities esp_driver_uart driver fatfs vfs disk_utilities
)