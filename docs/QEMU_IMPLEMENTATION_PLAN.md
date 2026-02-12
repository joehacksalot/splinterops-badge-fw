# Espressif QEMU Implementation Plan for SplinterOps Badge Firmware

## Executive Summary

This document outlines a plan to integrate Espressif's QEMU emulator into the SplinterOps badge firmware project to enable automated, hardware-free testing of baseline image changes. The ESP32 QEMU fork supports CPU, memory, SPI flash, UART, GPIO, timers, NVS, FAT filesystem, eFuses, hardware crypto (AES/SHA/RSA), Ethernet (OpenCores), and SD/MMC — but **does not** emulate WiFi, Bluetooth/BLE, touch sensors, ADC/DAC, RMT, or SPI-driven LED peripherals.

---

## 1. Project Context

| Property          | Value                              |
|-------------------|------------------------------------|
| **Target chip**   | ESP32 (dual-core Xtensa LX6)      |
| **IDF version**   | 5.4.2                             |
| **Flash layout**  | NVS + OTA data + PHY + 9MB FAT + 2x 3MB OTA partitions |
| **Badge variants**| TRON, REACTOR, CREST, FMAN25       |
| **Existing CI**   | None                               |

### Peripheral Usage in Firmware

| Subsystem            | Peripheral(s) Used               | QEMU Support |
|----------------------|----------------------------------|:------------:|
| NVS storage          | NVS flash                        | ✅           |
| FAT filesystem       | SPI flash wear-leveling           | ✅           |
| Console / UART       | UART0                            | ✅           |
| GPIO (eyes, vibration)| GPIO output                     | ✅ (basic)   |
| Timers (esp_timer, FreeRTOS) | Software timers            | ✅           |
| FreeRTOS tasks       | Dual-core SMP                    | ✅           |
| Hardware crypto      | AES, SHA, RSA (via TLS)          | ✅           |
| LED strip (WS2812)   | SPI + RMT                       | ❌           |
| Touch sensor          | Capacitive touch pads           | ❌           |
| BLE (NimBLE)          | Bluetooth controller            | ❌           |
| WiFi (STA mode)       | WiFi radio                      | ❌           |
| ADC (battery sensor)  | ADC oneshot                     | ❌           |
| PWM audio (buzzer)    | DAC/PWM                         | ❌           |
| OTA update            | HTTPS + OTA partition mgmt      | ⚠️ Partial   |
| HTTP game client      | HTTPS client                    | ⚠️ Partial   |

**⚠️ Partial** = The software logic works but requires network; QEMU supports Ethernet (OpenCores MAC) with user-mode networking as a substitute for WiFi.

---

## 2. What Can Be Tested Under QEMU

### Tier 1 — Directly Testable (No Mocking Needed)
- **Boot sequence**: Second-stage bootloader → app_main entry
- **NVS read/write**: UserSettings persistence, BadgeStats storage
- **FAT filesystem**: DiskUtilities mount, file read/write operations
- **Console commands**: All registered console commands via emulated UART
- **FreeRTOS scheduling**: Task creation, semaphores, event groups, timers
- **State machine logic**: SystemState transitions, NotificationDispatcher event routing
- **JSON parsing**: cJSON operations in GameState, HTTPGameClient response parsing
- **Data structures**: CircularBuffer, hashmap operations
- **OTA partition management**: Partition table parsing, app descriptor comparison (without actual download)
- **eFuse emulation**: Secure boot / flash encryption testing without burning real fuses

### Tier 2 — Testable With Abstraction Layer / Mocking
- **WiFi → OpenCores Ethernet**: Network stack tests via QEMU's emulated Ethernet (requires `CONFIG_ETH_USE_OPENETH`)
- **HTTP/HTTPS client**: OTA download logic, game server communication (via Ethernet bridge)
- **BLE logic**: Service registration, GATT characteristic handling (mock NimBLE at API boundary)
- **Touch sensor logic**: TouchActions decision logic (mock touch_pad readings)
- **LED sequence logic**: Color computation, animation state machines (mock LED strip driver)
- **Battery sensor**: Voltage-to-percentage conversion (mock ADC readings)
- **PWM audio/synth**: Note generation logic (mock DAC output)

### Tier 3 — Cannot Be Tested in QEMU
- Actual RF behavior (WiFi association, BLE advertising/scanning)
- Real touch sensor calibration and threshold tuning
- Actual LED strip signal timing (RMT/SPI waveforms)
- ADC hardware accuracy / calibration curves
- Power consumption and sleep modes

---

## 3. Implementation Phases

### Phase 1: QEMU Environment Setup

#### 1a. Install Espressif QEMU (macOS)

```bash
# Install system dependencies
brew install libgcrypt glib pixman sdl2 libslirp

# Install pre-built QEMU binaries via idf_tools
python $IDF_PATH/tools/idf_tools.py install qemu-xtensa qemu-riscv32

# Re-export PATH
. $IDF_PATH/export.sh
```

#### 1b. Verify QEMU Works With Current Firmware

```bash
# Build the project
idf.py build

# Run in QEMU (will likely crash due to uninitialized peripherals, that's expected)
idf.py qemu monitor
```

**Expected outcome**: Firmware boots, NVS/FAT init succeeds, then crashes on touch_pad_init or BLE init. This confirms QEMU works and identifies the first peripheral boundary to abstract.

#### 1c. Create QEMU-Specific sdkconfig Overlay

Create `sdkconfig.ci.qemu` with QEMU-specific settings:

```
# Disable watchdog timers (cleaner QEMU output)
CONFIG_ESP_TASK_WDT_EN=n
CONFIG_ESP_INT_WDT=n

# Use OpenCores Ethernet instead of WiFi for network tests
CONFIG_ETH_USE_OPENETH=y

# Disable WiFi (not emulated)
# Note: WiFi component is still linked but init can be skipped at runtime

# Increase log verbosity for test output
CONFIG_LOG_DEFAULT_LEVEL_DEBUG=y

# Ensure console UART is enabled
CONFIG_ESP_CONSOLE_UART_DEFAULT=y
```

---

### Phase 2: Hardware Abstraction Layer (HAL) for Testability

The key architectural change is introducing a thin abstraction layer so QEMU builds can substitute stubs for unsupported peripherals.

#### 2a. Compile-Time Peripheral Gating

Add a new Kconfig option in `main/Kconfig.projbuild`:

```
config BADGE_QEMU_MODE
    bool "Build for QEMU emulation (stubs unsupported peripherals)"
    default n
    help
        When enabled, unsupported peripherals (touch, BLE, WiFi, ADC, LED strip, PWM)
        are replaced with software stubs. Use this for CI/QEMU testing only.
```

#### 2b. Stub Implementation Strategy

For each unsupported peripheral, create a stub source file that provides the same API but returns mock data or no-ops:

```
main/
  src/
    stubs/                          # New directory
      TouchSensor_Stub.c           # Returns configurable fake touch values
      BatterySensor_Stub.c         # Returns configurable fake ADC/voltage
      LedControl_Stub.c            # Logs LED state changes, no hardware
      BleControl_Stub.c            # Logs BLE operations, no radio
      WifiClient_Stub.c            # Either no-op or uses OpenCores Ethernet
      SynthMode_Stub.c             # Logs audio operations, no DAC
```

In `main/CMakeLists.txt`, conditionally select real vs. stub sources:

```cmake
if(CONFIG_BADGE_QEMU_MODE)
    set(STUB_SOURCES
        "src/stubs/TouchSensor_Stub.c"
        "src/stubs/BatterySensor_Stub.c"
        "src/stubs/LedControl_Stub.c"
        "src/stubs/BleControl_Stub.c"
        "src/stubs/WifiClient_Stub.c"
        "src/stubs/SynthMode_Stub.c"
    )
    # Exclude real implementations, include stubs
    # (implementation detail: use SRC_DIRS + EXCLUDE_SRCS or explicit file lists)
else()
    set(STUB_SOURCES "")
endif()
```

#### 2c. Runtime Peripheral Check (Alternative / Complementary Approach)

Instead of (or in addition to) compile-time gating, modify `SystemState_Init` to skip peripheral init based on a runtime flag or Kconfig:

```c
#ifndef CONFIG_BADGE_QEMU_MODE
    ESP_ERROR_CHECK(TouchSensor_Init(...));
    ESP_ERROR_CHECK(BleControl_Init(...));
    // ... other HW-dependent inits
#else
    ESP_LOGI(TAG, "QEMU mode: skipping hardware peripheral init");
    // Initialize stubs instead
#endif
```

---

### Phase 3: Test Framework Integration

#### 3a. Install pytest-embedded with QEMU Support

```bash
pip install pytest-embedded pytest-embedded-serial-esp pytest-embedded-idf pytest-embedded-qemu
```

Or add to a `requirements.txt` in the project root:

```
pytest-embedded>=1.0.0
pytest-embedded-serial-esp>=1.0.0
pytest-embedded-idf>=1.0.0
pytest-embedded-qemu>=1.0.0
```

#### 3b. Create Test Directory Structure

```
tests/
  conftest.py                   # pytest configuration
  pytest.ini                    # pytest settings
  test_boot.py                  # Boot sequence validation
  test_nvs.py                   # NVS read/write tests
  test_filesystem.py            # FAT filesystem tests
  test_console.py               # Console command tests
  test_state_machine.py         # SystemState transition tests
  test_notification.py          # NotificationDispatcher tests
  test_game_logic.py            # GameState logic tests
  test_json_parsing.py          # JSON response parsing tests
  test_ota_logic.py             # OTA version comparison logic
  test_data_structures.py       # CircularBuffer, hashmap tests
```

#### 3c. Example Test: Boot Validation

```python
# tests/test_boot.py
import pytest

@pytest.mark.esp32
@pytest.mark.qemu
def test_boot_to_app_main(dut):
    """Verify firmware boots successfully in QEMU and reaches app_main."""
    dut.expect("Initialize finished", timeout=30)

@pytest.mark.esp32
@pytest.mark.qemu
def test_nvs_init(dut):
    """Verify NVS initializes without errors."""
    dut.expect("Mounted data FATFS", timeout=30)

@pytest.mark.esp32
@pytest.mark.qemu
def test_console_ready(dut):
    """Verify console is ready and accepts commands."""
    # Wait for boot to complete
    dut.expect("esp>", timeout=30)
    # Send a test command
    dut.write("help")
    dut.expect("help", timeout=5)
```

#### 3d. Example conftest.py

```python
# tests/conftest.py
import pytest

@pytest.fixture(autouse=True)
def qemu_config(request):
    """Configure QEMU-specific test settings."""
    pass
```

#### 3e. Running Tests Locally

```bash
# Build with QEMU config
idf.py -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.ci.qemu" set-target esp32 build

# Run QEMU tests
cd tests/
pytest --target esp32 --embedded-services idf,qemu
```

---

### Phase 4: CI/CD Pipeline (GitHub Actions)

#### 4a. Create GitHub Actions Workflow

**IMPORTANT**: For now, we will be disabling the QEMU tests in the GitHub Actions workflow. We will be using the QEMU tests for local development and testing.

```yaml
# .github/workflows/qemu-test.yml
name: QEMU Tests

on:
  push:
    branches: [main, develop]
  pull_request:
    branches: [main]

jobs:
  qemu-test:
    runs-on: ubuntu-latest
    container:
      image: espressif/idf:v5.4.2

    steps:
      - name: Checkout
        uses: actions/checkout@v4
        with:
          submodules: recursive

      - name: Install QEMU
        run: |
          apt-get update && apt-get install -y libgcrypt20 libglib2.0-0 libpixman-1-0 libslirp0
          . $IDF_PATH/export.sh
          python $IDF_PATH/tools/idf_tools.py install qemu-xtensa
          . $IDF_PATH/export.sh

      - name: Install test dependencies
        run: |
          pip install pytest-embedded pytest-embedded-idf pytest-embedded-qemu

      - name: Build firmware (QEMU config)
        run: |
          . $IDF_PATH/export.sh
          idf.py -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.ci.qemu" set-target esp32 build

      - name: Run QEMU tests
        run: |
          . $IDF_PATH/export.sh
          cd tests/
          pytest --target esp32 --embedded-services idf,qemu --junitxml=results.xml

      - name: Upload test results
        if: always()
        uses: actions/upload-artifact@v4
        with:
          name: test-results
          path: tests/results.xml
```

---

### Phase 5: Expand Test Coverage Incrementally

#### Priority order for test development:

1. **Boot smoke test** — Does the QEMU image boot without panics?
2. **NVS/FAT filesystem** — Can we read/write persistent data?
3. **Console commands** — Do badge CLI commands work correctly?
4. **State machine transitions** — Does SystemState route notifications correctly?
5. **Game logic** — Does GameState compute correctly?
6. **JSON parsing** — Do HTTP response parsers handle edge cases?
7. **OTA logic** — Does version comparison work? (mock HTTP server via QEMU Ethernet)
8. **Network stack** — Can we reach a mock server via OpenCores Ethernet?

---

## 4. File Changes Summary

| Action   | Path                                      | Description                                  |
|----------|-------------------------------------------|----------------------------------------------|
| **New**  | `sdkconfig.ci.qemu`                      | QEMU-specific sdkconfig overlay              |
| **New**  | `main/src/stubs/`                         | Stub implementations for unsupported HW      |
| **Modify** | `main/Kconfig.projbuild`               | Add `CONFIG_BADGE_QEMU_MODE` option          |
| **Modify** | `main/CMakeLists.txt`                  | Conditional source selection for stubs       |
| **Modify** | `main/src/SystemState.c`               | Gate HW init on QEMU mode                   |
| **New**  | `tests/`                                  | Test directory with pytest scripts           |
| **New**  | `tests/conftest.py`                       | pytest-embedded configuration                |
| **New**  | `tests/pytest.ini`                        | pytest markers and settings                  |
| **New**  | `tests/test_boot.py`                      | Boot validation tests                        |
| **New**  | `requirements-test.txt`                   | Python test dependencies                     |
| **New**  | `.github/workflows/qemu-test.yml`        | CI pipeline definition                       |

---

## 5. Risks and Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| QEMU peripheral gaps cause false test failures | Medium | Stub layer isolates unsupported HW; tests only assert on supported subsystems |
| Stub divergence from real HW behavior | Medium | Keep stubs minimal (no-op + logging); don't test HW-specific behavior in QEMU |
| QEMU boot too slow for CI | Low | ESP32 QEMU boots in ~2-5 seconds; well within CI budgets |
| Maintaining two build configs | Low | `sdkconfig.ci.qemu` is an overlay, not a fork; base config stays authoritative |
| NimBLE / WiFi code still links even when stubbed | Medium | Use compile-time exclusion via CMake; ensure stub APIs match real signatures exactly |

---

## 6. Estimated Effort

| Phase | Effort | Dependencies |
|-------|--------|-------------|
| Phase 1: QEMU env setup | 1-2 hours | None |
| Phase 2: HAL / stub layer | 4-8 hours | Phase 1 |
| Phase 3: Test framework | 2-4 hours | Phase 2 |
| Phase 4: CI pipeline | 2-3 hours | Phase 3 |
| Phase 5: Expand coverage | Ongoing | Phase 4 |

**Total initial investment: ~1-2 days to first green CI pipeline.**

---

## 7. References

- [ESP-IDF QEMU Guide (v5.5.2)](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/tools/qemu.html)
- [Espressif QEMU ESP32 README](https://github.com/espressif/esp-toolchain-docs/blob/main/qemu/esp32/README.md)
- [pytest-embedded-qemu](https://pypi.org/project/pytest-embedded-qemu/)
- [ESP-IDF Tests with Pytest Guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/contribute/esp-idf-tests-with-pytest.html)
- [espressif/esp-idf-ci-action](https://github.com/espressif/esp-idf-ci-action)
