if(CONFIG_BADGE_QEMU_MODE)
    # QEMU mode: use stubs for unsupported peripherals (touch, LED, battery, WiFi)
    # but keep REAL BLE application code with a custom HCI transport shim.
    # The BleControl_Stub.c is no longer used — real BLE code runs against
    # the NimBLE host stack, with HCI packets bridged to an external virtual
    # controller (e.g. Google Bumble) via UART/TCP.
    # QEMU mode: real LedControl.c runs — led_strip_qemu.c provides the
    # hardware abstraction.  TouchSensor_QemuViz.c replaces the old no-op
    # touch stub with one that receives events from the visualization UI.
    set(QEMU_EXCLUDED_SRCS
        "src/TouchSensor.c"
        "src/BatterySensor.c"
        "src/BleSpec.c"
        "src/WifiClient.c"
        "src/stubs/WifiClient_Stub.c"
        "src/stubs/BleControl_Stub.c"
        "src/stubs/LedControl_Stub.c"
        "src/stubs/TouchSensor_Stub.c"
        "src/stubs/SynthMode_Stub.c"
    )
    # LedControl.c is NO LONGER excluded — real LED logic runs.
    # led_strip_qemu.c overrides the led_strip component functions.
    # qemu_viz_transport.c provides shared UART2 transport.
    # TouchSensor_QemuViz.c receives touch events from the viz UI.
    idf_component_register( SRC_DIRS "src" "src/songs" "src/stubs"
                            EXCLUDE_SRCS ${QEMU_EXCLUDED_SRCS}
                            INCLUDE_DIRS "inc"
                            REQUIRES esp_wifi esp_event esp_netif esp_eth esp_phy lwip vfs efuse esp_timer driver fatfs console_cmds led_strip json esp_http_client log freertos esp-tls esp_https_ota app_update esp_adc esp_netif bt)
else()
    # Normal build: use real hardware implementations
    idf_component_register( SRC_DIRS "src" "src/songs"
                            INCLUDE_DIRS "inc"
                            REQUIRES esp_wifi esp_event esp_netif esp_eth esp_phy lwip vfs efuse esp_timer driver fatfs console_cmds led_strip json esp_http_client log freertos esp-tls esp_https_ota app_update esp_adc esp_netif bt)
endif()

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