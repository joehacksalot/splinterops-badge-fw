idf_component_register( SRC_DIRS "src"
                        INCLUDE_DIRS "inc"
                        REQUIRES driver fatfs console_cmds led_strip json esp_http_client log freertos esp-tls esp_https_ota app_update esp_adc_cal esp_netif bt)

#add_definitions(-DTRON_BADGE)
#add_definitions(-DREACTOR_BADGE)
add_definitions(-DCREST_BADGE)