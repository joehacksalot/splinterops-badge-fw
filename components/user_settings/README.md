# User Settings Component

Manages persistent user-configurable settings (sounds, vibrations, WiFi credentials, pairing ID, selected index) and exposes a thread-safe API. Periodically flushes updates to disk and initializes unique per-badge identifiers.

## Overview

`user_settings` provides:
- A `UserSettings` struct with an in-memory copy of user settings
- A background task that writes settings to disk periodically or when updated
- Helpers to update settings from JSON payloads

Settings are stored at `CONFIG_MOUNT_PATH "/settings"` as a binary blob of `UserSettingsFile`.

## Features

- **Thread-safe updates** using a FreeRTOS mutex
- **Periodic persistence** to filesystem (every 60 seconds)
- **JSON ingestion** via cJSON
- **Badge identity** and key generation with salted SHA-256 and Base64

## API

### `esp_err_t UserSettings_Init(UserSettings *this, BatterySensor * pBatterySensor, int userSettingsPriority)`
Initializes the component, loads settings from disk (or creates defaults), and starts the background task pinned to the app CPU.

### `esp_err_t UserSettings_UpdateFromJson(UserSettings *this, uint8_t * settingsJson)`
Parses a JSON buffer and updates settings (vibrations, sounds, ssid, pass). Triggers persistence.

### `esp_err_t UserSettings_SetPairId(UserSettings *this, uint8_t * pairId)`
Sets or clears the pairing ID.

### `esp_err_t UserSettings_SetSelectedIndex(UserSettings *this, uint32_t selectedIndex)`
Updates the selected index.

## Configuration

- `CONFIG_MOUNT_PATH`: Filesystem mount path used for the settings file
- Uses SHA engine and CRTs provided by ESP-IDF/mbedTLS

## Dependencies

```mermaid
graph TD
    A["user_settings<br/><small>splinterops</small>"] --> B["battery_sensor<br/><small>splinterops</small>"]
    A --> C["disk_utilities<br/><small>splinterops</small>"]
    A --> D["utilities<br/><small>splinterops</small>"]
    A --> E["freertos<br/><small>freertos</small>"]
    A --> F["log<br/><small>esp-idf</small>"]
    A --> G["cjson<br/><small>esp-idf</small>"]
    A --> H["mbedtls<br/><small>esp-idf</small>"]
    A --> I["esp_hw_support<br/><small>esp-idf</small>"]
    A --> J["driver<br/><small>esp-idf</small>"]
    A --> K["esp_system<br/><small>esp-idf</small>"]

    %% Styling
    style A fill:#e1f5fe,color:#000000
    style B fill:#e1f5fe,color:#000000
    style C fill:#e1f5fe,color:#000000
    style D fill:#e1f5fe,color:#000000
    style E fill:#f8cecc,color:#000000
    style F fill:#fff2cc,color:#000000
    style G fill:#fff2cc,color:#000000
    style H fill:#fff2cc,color:#000000
    style I fill:#fff2cc,color:#000000
    style J fill:#fff2cc,color:#000000
    style K fill:#fff2cc,color:#000000
```

## SplinterOps Dependency Tree

```mermaid
graph TD
    %% Root component
    A["user_settings<br/><small>splinterops</small>"]

    %% Direct deps
    A --> B["battery_sensor<br/><small>splinterops</small>"]
    A --> C["disk_utilities<br/><small>splinterops</small>"]
    A --> D["utilities<br/><small>splinterops</small>"]

    %% Recursive deps from battery_sensor
    B --> D
    B --> E["notification_dispatcher<br/><small>splinterops</small>"]

    %% Recursive deps from disk_utilities
    C --> B

    %% Styles (SplinterOps components)
    style A fill:#e1f5fe,color:#000000
    style B fill:#e1f5fe,color:#000000
    style C fill:#e1f5fe,color:#000000
    style D fill:#e1f5fe,color:#000000
    style E fill:#e1f5fe,color:#000000
```

## Integration

Add to your main component CMake:

```cmake
idf_component_register(
    # ... your sources
    REQUIRES user_settings
)
```

Initialize during bring-up (example):

```c
ESP_ERROR_CHECK(UserSettings_Init(&this->userSettings, &this->batterySensor, USER_SETTINGS_TASK_PRIORITY));
```

## Component Structure

```
components/user_settings/
├── CMakeLists.txt
├── UserSettings.c
├── UserSettings.h
└── README.md
```
