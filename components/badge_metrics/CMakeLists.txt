idf_component_register(
    SRCS "BadgeMetrics.c"
    INCLUDE_DIRS "."
    REQUIRES freertos log battery_sensor disk_utilities utilities
)
