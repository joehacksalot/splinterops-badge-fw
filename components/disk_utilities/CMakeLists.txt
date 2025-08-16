idf_component_register(
    SRCS "DiskUtilities.c"
    INCLUDE_DIRS "."
    REQUIRES battery_sensor nvs_flash fatfs
)
