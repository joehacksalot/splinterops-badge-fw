# 2024 Badge Application

An interactive ESP32-based conference badge with BLE connectivity, LED control, touch sensors, and audio synthesis capabilities.

## Table of Contents

- Overview
- Features
- Hardware Requirements
- Project Structure
- Getting Started
- Building and Flashing
- Configuration
- Architecture
- Development
- Troubleshooting

## Overview

This project implements a feature-rich conference badge application built on the ESP-IDF framework. The badge supports interactive games, peer-to-peer communication via BLE, customizable LED patterns, touch-based controls, and audio synthesis with support for various musical compositions.

## Features

### Core Functionality
- **BLE Communication**: Peer-to-peer connectivity and service advertising
- **Touch Interface**: Multi-touch sensor support with gesture recognition
- **LED Control**: Programmable LED sequences and patterns
- **Audio System**: Musical note synthesis and song playback
- **Interactive Games**: Multi-badge gaming capabilities
- **OTA Updates**: Over-the-air firmware update support
- **Battery Management**: Power monitoring and low-battery indicators
- **File Transfer**: BLE-based file transfer capabilities

### Badge Variants
- TRON Badge
- Reactor Badge 
- Crest Badge
- FMAN25 Badge

## Hardware Requirements

- ESP32 microcontroller
- Touch sensors
- LED array/matrix
- Audio output (buzzer/speaker)
- Battery power system
- BLE antenna

## Project Structure

```
fman25/
├── main/                    # Main application code
│   ├── inc/                 # Header files
│   │   ├── SystemState.h    # Main system state management
│   │   ├── Song.h           # Audio/music definitions
│   │   ├── BleControl*.h    # BLE functionality
│   │   └── ...              # Other component headers
│   └── src/                 # Implementation files
│       ├── SystemState.c    # Core system logic
│       ├── Songs.c          # Music/audio implementation
│       └── songs/           # Individual song definitions
├── components/              # Custom ESP-IDF components
│   ├── console_cmds/        # Console command handlers
│   └── ...                  # Other components
├── managed_components/      # ESP Component Registry components
├── CMakeLists.txt          # Build configuration
├── sdkconfig               # ESP-IDF configuration
└── partitions_*.csv        # Flash partition tables
```

## Getting Started

### Prerequisites

1. **ESP-IDF**: Install ESP-IDF v4.0.2 or compatible version
2. **Python**: Python 3.6+ with pip
3. **Git**: For cloning and submodule management

### Initial Setup

1. Clone the repository with submodules:
```bash
git clone --recursive <repository-url>
cd fman25
```

2. If already cloned, initialize submodules:
```bash
git submodule update --init --recursive
```

3. Set up ESP-IDF environment:
```bash
. $IDF_PATH/export.sh
```

## Building and Flashing

### Build Configuration

1. Configure the project:
```bash
idf.py menuconfig
```

2. Build the project:
```bash
idf.py build
```

### Flashing

1. **Erase and flash** (recommended for first-time setup):
```bash
idf.py -p $(ls /dev/cu.usbserial-*) erase-flash
idf.py -p $(ls /dev/cu.usbserial-*) flash
```

2. **Quick flash** (for updates):
```bash
idf.py -p $(ls /dev/cu.usbserial-*) flash
```

3. **All-in-one command**:
```bash
idf.py -p $(ls /dev/cu.usbserial-*) erase-flash && idf.py -p $(ls /dev/cu.usbserial-*) flash
```

### Monitoring

Monitor serial output for debugging:
```bash
idf.py -p $(ls /dev/cu.usbserial-*) monitor
```

Exit monitor with `Ctrl+]`

## Configuration

### Badge Type Selection
The badge type is configured at compile time using preprocessor definitions:
- `TRON_BADGE`
- `REACTOR_BADGE` 
- `CREST_BADGE`
- `FMAN25_BADGE`

### Key Configuration Options
- **Touch sensitivity thresholds**
- **LED brightness and patterns**
- **BLE advertising parameters**
- **Audio volume and synthesis settings**
- **Power management settings**

## Architecture

### System Components

1. **SystemState**: Central state machine managing badge behavior
2. **BleControl**: Bluetooth Low Energy communication stack
3. **TouchSensor**: Multi-touch input handling
4. **LedControl**: LED pattern and sequence management
5. **Audio System**: Note synthesis and song playback
6. **InteractiveGame**: Multi-badge gaming logic
7. **NotificationDispatcher**: Event-driven communication between components

### Event-Driven Architecture
The system uses ESP-IDF's event system for loose coupling between components:
- Touch events trigger system state changes
- BLE events handle peer communication
- Timer events manage LED sequences and timeouts
- Game events coordinate multi-badge interactions

### State Management
The badge operates in multiple modes:
- **Idle**: Low-power standby with basic LED patterns
- **Touch Active**: Responding to user input
- **Game Mode**: Interactive multi-badge games
- **Synth Mode**: Musical note synthesis
- **Menu Mode**: Configuration and settings

## Development

### Adding New Songs
1. Create a new file in `main/src/songs/`
2. Define the song structure using the `SongNotes` format
3. Add the song to the `pSongs` array in `Songs.c`
4. Update the `Song_e` enum in `Song.h`

### Adding New LED Patterns
1. Implement pattern logic in the LED control system
2. Add pattern identifiers to relevant enums
3. Update the pattern selection logic

### BLE Service Extensions
1. Define new characteristics in `BleControl_Service.h`
2. Implement handlers in the appropriate service files
3. Update the service registration code

## Troubleshooting

### Common Issues

**Build Errors:**
- Ensure ESP-IDF environment is properly sourced
- Check that all submodules are initialized
- Verify ESP-IDF version compatibility

**Flash Errors:**
- Check USB connection and port permissions
- Try different baud rates
- Ensure badge is in download mode

**Runtime Issues:**
- Monitor serial output for error messages
- Check power supply stability
- Verify hardware connections

**BLE Connection Problems:**
- Check BLE is enabled in configuration
- Verify peer badge compatibility
- Monitor RSSI levels for range issues

### Debug Features
- Serial console with command interface
- LED status indicators
- BLE service for remote debugging
- Comprehensive logging system

## Contributing

When contributing to this project:
1. Follow the existing code style and naming conventions
2. Add appropriate documentation for new features
3. Test on actual hardware when possible
4. Update this README for significant changes

## License

[Add license information here]