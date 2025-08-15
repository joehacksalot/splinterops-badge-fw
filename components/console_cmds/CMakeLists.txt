idf_component_register(
    SRC_DIRS "system" "badge"
    INCLUDE_DIRS "system" "badge" "../../main/inc/"
    REQUIRES console nvs_flash spi_flash utilities
)