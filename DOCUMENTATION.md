# Badge Application Documentation Guide

## Overview

This document provides comprehensive documentation for the 2024 Badge Application codebase. It serves as a developer's guide to understanding the architecture, coding standards, and implementation details.

## Code Documentation Standards

### File Headers
All source files should include a comprehensive header with:
- Brief description of the module's purpose
- Key functionality overview
- Author information
- Date information

Example:
```c
/**
 * @file ModuleName.h
 * @brief Brief description of module functionality
 * 
 * Detailed description of what this module does, its key features,
 * and how it fits into the overall system architecture.
 * 
 * @author Badge Development Team
 * @date 2024
 */
```

### Function Documentation
All public functions should be documented using Doxygen-style comments:

```c
/**
 * @brief Brief function description
 * 
 * Detailed description of what the function does, its behavior,
 * and any important implementation notes.
 * 
 * @param[in] param1 Description of input parameter
 * @param[out] param2 Description of output parameter
 * @param[in,out] param3 Description of input/output parameter
 * 
 * @return Description of return value
 * @retval SPECIFIC_VALUE Description of when this value is returned
 * @retval ERROR_CODE Description of error condition
 * 
 * @note Important implementation notes
 * @warning Critical warnings for users
 * @see Related functions or modules
 */
```

### Structure Documentation
All structures should be documented with field descriptions:

```c
/**
 * @brief Brief structure description
 * 
 * Detailed description of the structure's purpose and usage.
 */
typedef struct ExampleStruct_t
{
    int field1;          /**< Description of field1 */
    bool field2;         /**< Description of field2 */
    char *field3;        /**< Description of field3 */
} ExampleStruct;
```

## Architecture Documentation

### System Components

#### SystemState Module
- **Purpose**: Central state machine and coordination hub
- **Key Files**: `SystemState.h`, `SystemState.c`
- **Responsibilities**:
  - System initialization and shutdown
  - Event coordination between subsystems
  - Timer management
  - State transitions

#### BLE Control System
- **Purpose**: Bluetooth Low Energy communication
- **Key Files**: `BleControl*.h`, `BleControl*.c`
- **Responsibilities**:
  - BLE stack management
  - Service and characteristic definitions
  - Peer discovery and connection
  - Data transfer protocols

#### Touch System
- **Purpose**: Touch sensor input processing
- **Key Files**: `TouchSensor.h`, `TouchActions.h`
- **Responsibilities**:
  - Touch sensor calibration
  - Gesture recognition
  - Touch event generation
  - Action mapping

#### LED Control System
- **Purpose**: LED pattern and sequence management
- **Key Files**: `LedControl.h`, `LedModing.h`, `LedSequences.h`
- **Responsibilities**:
  - LED hardware abstraction
  - Pattern generation and playback
  - Brightness control
  - Sequence timing

#### Audio System
- **Purpose**: Musical note synthesis and playback
- **Key Files**: `Song.h`, `Songs.c`, `SynthMode.h`, `Ocarina.h`
- **Responsibilities**:
  - Note frequency generation
  - Song library management
  - Tempo and timing control
  - Audio output control

### Event System Architecture

The badge uses ESP-IDF's event system for loose coupling between components:

```
┌─────────────────┐    Events    ┌─────────────────┐
│   Touch System  │─────────────→│  SystemState    │
└─────────────────┘              └─────────────────┘
                                          │
┌─────────────────┐    Events            │    State Changes
│   BLE System    │─────────────→        │
└─────────────────┘                      ▼
                                 ┌─────────────────┐
┌─────────────────┐    Events    │   LED System    │
│   Game System   │─────────────→└─────────────────┘
└─────────────────┘
```

### State Machine Overview

The badge operates in several primary states:
- **IDLE**: Low-power mode with basic LED patterns
- **TOUCH_ACTIVE**: Processing user touch input
- **GAME_MODE**: Interactive multi-badge games
- **SYNTH_MODE**: Musical synthesis and playback
- **MENU_MODE**: Configuration and settings
- **BLE_ACTIVE**: Peer communication mode

## Development Guidelines

### Adding New Features

#### New Songs
1. Create song definition file in `main/src/songs/`
2. Define `SongNotes` structure with tempo and note array
3. Add extern declaration in `Songs.c`
4. Add song to `pSongs` array
5. Update `Song_e` enum in `Song.h`

#### New LED Patterns
1. Define pattern data structure
2. Implement pattern generation function
3. Add pattern to sequence manager
4. Update pattern selection logic

#### New BLE Services
1. Define service UUID and characteristics
2. Implement service handlers
3. Register service with BLE stack
4. Add notification handlers

### Testing Guidelines

#### Hardware Testing
- Test on actual badge hardware when possible
- Verify power consumption in different modes
- Test BLE range and connectivity
- Validate touch sensor responsiveness

#### Software Testing
- Use ESP-IDF logging for debugging
- Implement unit tests for critical functions
- Test error handling paths
- Validate memory usage

### Performance Considerations

#### Memory Management
- Monitor heap usage during development
- Use stack allocation for temporary data
- Free allocated memory promptly
- Consider memory fragmentation

#### Real-Time Constraints
- Keep interrupt handlers minimal
- Use appropriate task priorities
- Minimize blocking operations
- Consider watchdog timer requirements

#### Power Optimization
- Use deep sleep when appropriate
- Optimize LED brightness levels
- Manage BLE advertising intervals
- Monitor battery consumption

## Debugging and Troubleshooting

### Common Debug Techniques
- Use ESP_LOG macros for structured logging
- Monitor task stack usage
- Check for memory leaks
- Validate timer behavior

### Performance Monitoring
- Track CPU usage per task
- Monitor memory allocation patterns
- Measure response times
- Profile critical code paths

### Hardware Debug
- Use oscilloscope for timing analysis
- Monitor power consumption
- Check signal integrity
- Validate hardware connections

## Code Style Guidelines

### Naming Conventions
- Functions: `ModuleName_FunctionName()`
- Variables: `camelCase` or `snake_case`
- Constants: `UPPER_CASE`
- Types: `PascalCase` with `_t` suffix

### File Organization
- Header files in `main/inc/`
- Implementation files in `main/src/`
- Component-specific files in `components/`
- Configuration files in project root

### Comment Guidelines
- Use `//` for single-line comments
- Use `/* */` for multi-line comments
- Use `/** */` for documentation comments
- Keep comments concise and relevant

## Build System Documentation

### CMake Configuration
- Main build file: `CMakeLists.txt`
- Component definitions in `components/`
- Dependency management via `dependencies.lock`

### Configuration Options
- Badge type selection via preprocessor defines
- Feature enable/disable flags
- Hardware-specific configurations
- Performance tuning parameters

## Deployment and Maintenance

### Firmware Updates
- OTA update system for remote updates
- Version management and rollback
- Update validation and verification
- Staged deployment strategies

### Configuration Management
- User settings persistence
- Factory reset capabilities
- Configuration backup and restore
- Remote configuration updates

This documentation should be updated as the codebase evolves to maintain accuracy and usefulness for developers.
