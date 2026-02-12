# Touch & LED Emulation Plan for QEMU with UI Visualization

## Executive Summary

This document outlines a plan to emulate the badge's **capacitive touch sensors** and **WS2812 LED strip** within the QEMU environment, replacing the current no-op stubs with functional emulations that communicate with an external **visualization UI**. The UI will render LED pixel state in real-time and allow the user to inject touch events, enabling full-loop testing of touch → notification → LED response without physical hardware.

---

## 1. Current State

### What Exists Today

| Component | Real Implementation | QEMU Stub | Gap |
|-----------|-------------------|-----------|-----|
| **Touch Sensor** | `TouchSensor.c` — reads ESP32 `touch_pad_*` API, detects press/release/short/long/very-long, fires `NOTIFICATION_EVENTS_TOUCH_SENSE_ACTION` | `TouchSensor_Stub.c` — no-op `Init`, returns 0 for all pads | No touch events can be injected; entire touch → action → LED pipeline is dead |
| **LED Control** | `LedControl.c` — manages `led_strip_handle_t` via SPI, calls `led_strip_set_pixel()` + `led_strip_refresh()` per frame at 50ms intervals | `LedControl_Stub.c` — no-op for all functions, logs mode changes | No pixel data is computed or visible; LED sequences, touch lighting, game events, etc. all invisible |

### Data Flow (Real Hardware)

```
Touch Pad HW → touch_pad_read_raw_data() → delta detection → NotificationDispatcher
    → SystemState_TouchSensorNotificationHandler
        → BleControl_SetTouchSensorActive()
        → LedControl_SetTouchSensorUpdate()
        → BadgeStats_IncrementNumTouches()
    → TouchActions (mode cycling, LED sequence changes, etc.)

LedControlTask (50ms loop):
    → Compute pixels per current mode (sequence, touch, game, BLE, etc.)
    → led_strip_set_pixel(handle, idx, r, g, b)  × LED_STRIP_LEN
    → led_strip_refresh(handle)  → SPI/RMT → WS2812 strip
```

### Why Current Stubs Are Insufficient

1. **Touch stub** silently discards all input — cannot test touch-driven state transitions (mode cycling, LED sequence changes, interactive game, synth mode).
2. **LED stub** never runs `LedControlTask` — cannot verify that LED sequences render correctly, that touch lighting maps work, or that game/BLE events produce expected visual output.
3. No observability: even if the real `LedControl.c` were running, there's no way to see the pixel output without physical WS2812 LEDs.

---

## 2. Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────┐
│                        QEMU (ESP32 Firmware)                        │
│                                                                     │
│  TouchSensor_Stub.c (enhanced)         LedControl.c (REAL, modified)│
│  ┌────────────────────────┐            ┌──────────────────────────┐ │
│  │ UART RX listener task  │            │ LedControlTask (50ms)    │ │
│  │ Parses touch commands  │            │ Computes all LED modes   │ │
│  │ from external UI       │            │ Calls SetPixel / Flush   │ │
│  │         │               │            │         │                │ │
│  │ Fires touch events via │            │ Flush → serialize pixel  │ │
│  │ NotificationDispatcher │            │ buffer → UART TX         │ │
│  └────────────────────────┘            └──────────────────────────┘ │
│           ↑                                      ↓                  │
│       QEMU UART2 (chardev)              QEMU UART2 (chardev)       │
│           ↑                                      ↓                  │
└───────────┼──────────────────────────────────────┼──────────────────┘
            │            TCP Socket                │
            │         localhost:1235               │
┌───────────┼──────────────────────────────────────┼──────────────────┐
│           ↓           Host Machine               ↓                  │
│  ┌─────────────────────────────────────────────────────────────────┐│
│  │              Visualization Bridge (Python)                      ││
│  │                                                                 ││
│  │  TCP client ←→ QEMU UART2                                      ││
│  │       │                                                         ││
│  │  Parses LED frames → WebSocket → Browser UI                    ││
│  │  Browser touch clicks → touch commands → TCP → QEMU            ││
│  └─────────────────────────────────────────────────────────────────┘│
│                            ↕ WebSocket                              │
│  ┌─────────────────────────────────────────────────────────────────┐│
│  │                    Browser UI (React/HTML)                      ││
│  │                                                                 ││
│  │  ┌──────────────────┐    ┌───────────────────────────────────┐ ││
│  │  │  Badge SVG/Canvas │    │  Touch Sensor Panel              │ ││
│  │  │  LED pixels shown │    │  Click to inject touch/release   │ ││
│  │  │  with real-time   │    │  9 buttons, shows active state   │ ││
│  │  │  RGB colors       │    │  Duration → short/long/vlong     │ ││
│  │  └──────────────────┘    └───────────────────────────────────┘ ││
│  │                                                                 ││
│  │  ┌──────────────────────────────────────────────────────────┐  ││
│  │  │  Event Log / Console                                     │  ││
│  │  │  Shows touch events, mode changes, notification flow     │  ││
│  │  └──────────────────────────────────────────────────────────┘  ││
│  └─────────────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────────────┘
```

### Key Design Decisions

1. **Run REAL `LedControl.c`** — not a stub. The entire pixel computation pipeline (sequences, touch lighting, game modes, etc.) runs unmodified. Only the hardware output path (`led_strip_set_pixel` / `led_strip_refresh`) is redirected.
2. **Use a shared UART channel** for both LED output and touch input. A simple binary protocol distinguishes LED frames from touch commands.
3. **Browser-based UI** for maximum portability — works on any OS, no native GUI dependencies.

---

## 3. Communication Protocol

### 3a. LED Frame Protocol (Firmware → UI)

Sent on every `led_strip_refresh()` call (~20 Hz):

```
Byte 0:     0xAA           (frame start marker)
Byte 1:     0x01           (message type: LED_FRAME)
Byte 2-3:   uint16_t       (number of LEDs, little-endian)
Byte 4+:    [R, G, B] × N  (3 bytes per LED, in strip order)
Byte last:  0x55           (frame end marker)
```

For `LED_STRIP_LEN = 77` (TRON), total frame = 1 + 1 + 2 + (77×3) + 1 = **236 bytes** per frame. At 20 Hz = ~4.7 KB/s — trivially within UART bandwidth even at 115200 baud.

### 3b. Touch Event Protocol (UI → Firmware)

```
Byte 0:     0xAA           (frame start marker)
Byte 1:     0x02           (message type: TOUCH_EVENT)
Byte 2:     uint8_t        (touch sensor index: 0–8)
Byte 3:     uint8_t        (event type: 0=released, 1=touched, 2=short, 3=long, 4=very_long)
Byte 4:     0x55           (frame end marker)
```

### 3c. Mode Change Notification (Firmware → UI, informational)

```
Byte 0:     0xAA
Byte 1:     0x03           (message type: MODE_CHANGE)
Byte 2:     uint8_t        (LedMode enum value)
Byte 3:     uint8_t        (InnerLedState)
Byte 4:     uint8_t        (OuterLedState)
Byte 5:     0x55
```

---

## 4. Implementation Plan

### Phase 1: LED Strip Emulation Layer

**Goal**: Run the real `LedControl.c` under QEMU by replacing the `led_strip` hardware driver with a virtual driver that serializes pixel data over UART.

#### 1a. Create `main/src/stubs/led_strip_qemu.c`

A QEMU-only replacement for the `led_strip` component APIs used by `LedControl.c`:

```c
// Replaces: led_strip_new_spi_device(), led_strip_set_pixel(), led_strip_refresh(), led_strip_clear()
// Under CONFIG_BADGE_QEMU_MODE, these functions:
//   - Store pixel data in a RAM buffer (replacing SPI/RMT output)
//   - On refresh(), serialize the pixel buffer over UART2 using the LED frame protocol
//   - On clear(), zero the pixel buffer

typedef struct {
    uint8_t pixels[LED_STRIP_LEN_MAX][3];  // RGB per pixel
    uint16_t num_leds;
    uart_port_t uart_port;                  // UART2
} led_strip_qemu_t;
```

This file provides the same function signatures as ESP-IDF's `led_strip` component so `LedControl.c` links without changes.

#### 1b. UART2 Initialization for Visualization Channel

```c
// In led_strip_qemu.c or a shared qemu_viz_transport.c
#define QEMU_VIZ_UART_NUM    UART_NUM_2
#define QEMU_VIZ_UART_BAUD   921600       // High baud for low-latency frames
#define QEMU_VIZ_TX_PIN      17           // Arbitrary, QEMU doesn't care about GPIO nums
#define QEMU_VIZ_RX_PIN      16

esp_err_t qemu_viz_transport_init(void);   // Sets up UART2 with ring buffer
```

QEMU launch command exposes UART2 as a TCP socket:
```bash
qemu-system-xtensa ... \
    -serial mon:stdio \                        # UART0 → console
    -serial tcp:localhost:1234,server,nowait \  # UART1 → HCI (BLE)
    -serial tcp:localhost:1235,server,nowait    # UART2 → Viz (LED+Touch)
```

#### 1c. Modify `main/CMakeLists.txt`

In QEMU mode:
- **Remove** `LedControl.c` from the excluded sources list (it was previously excluded; now we keep it)
- **Add** `src/stubs/led_strip_qemu.c` to the build
- **Remove** `src/stubs/LedControl_Stub.c` from the build (no longer needed)
- The `led_strip` component dependency can remain — we override its functions at link time via our stub, or we exclude the component and provide all needed symbols.

```cmake
if(CONFIG_BADGE_QEMU_MODE)
    set(QEMU_EXCLUDED_SRCS
        "src/TouchSensor.c"
        "src/BatterySensor.c"
        "src/BleSpec.c"
        "src/WifiClient.c"
        "src/SynthMode.c"
        "src/stubs/BleControl_Stub.c"
        "src/stubs/LedControl_Stub.c"     # Remove old LED stub
    )
    # LedControl.c is NO LONGER excluded — real LED logic runs
    # led_strip_qemu.c provides the hardware abstraction
    ...
endif()
```

#### 1d. Handle `led_strip` Component Dependency

Two options:

**Option A (Preferred)**: Create `led_strip_qemu.c` as a weak-symbol override. The `led_strip` component is still linked, but our implementations of `led_strip_new_spi_device()`, `led_strip_set_pixel()`, `led_strip_refresh()`, and `led_strip_clear()` take precedence.

**Option B**: Exclude the `led_strip` component entirely in QEMU mode and provide all needed type definitions and function stubs in our file. This is more invasive but avoids any SPI/RMT init code from the real component.

Recommend **Option A** for simplicity — the `led_strip` component's header (`led_strip.h`) still provides the type definitions (`led_strip_config_t`, `led_strip_handle_t`, etc.) that `LedControl.h` references.

---

### Phase 2: Touch Sensor Emulation Layer

**Goal**: Replace the current no-op `TouchSensor_Stub.c` with an enhanced stub that receives touch commands from the visualization UI over the same UART2 channel.

#### 2a. Enhance `TouchSensor_Stub.c` → `TouchSensor_QemuViz.c`

```c
// main/src/stubs/TouchSensor_QemuViz.c
//
// Replaces TouchSensor_Stub.c in QEMU mode.
// Instead of a task that reads touch_pad hardware, this runs a task that:
//   1. Reads UART2 for incoming touch commands (protocol 3b)
//   2. Translates them into TouchSensorEventNotificationData
//   3. Fires NotificationDispatcher_NotifyEvent(NOTIFICATION_EVENTS_TOUCH_SENSE_ACTION, ...)
//
// This means the entire downstream pipeline works:
//   SystemState_TouchSensorNotificationHandler → BleControl, LedControl, BadgeStats
//   TouchActions → mode cycling, LED sequence changes, etc.
```

Key implementation details:
- Runs a FreeRTOS task (`TouchSensorQemuTask`) that blocks on `uart_read_bytes(UART_NUM_2, ...)`
- Parses the 5-byte touch command frame
- Fires the same notification events as the real `TouchSensor.c`
- The UI can send raw events (touched/released) or high-level events (short_press/long_press) — the stub dispatches them identically

#### 2b. Shared Transport Layer

Both `led_strip_qemu.c` and `TouchSensor_QemuViz.c` share the same UART2. Create a small transport multiplexer:

```c
// main/src/stubs/qemu_viz_transport.c
//
// Initializes UART2 once
// Provides:
//   qemu_viz_send(uint8_t *data, size_t len)  — TX (LED frames, mode changes)
//   qemu_viz_register_rx_handler(uint8_t msg_type, rx_callback_t cb) — RX demux
//   qemu_viz_rx_task() — reads UART2, parses frame headers, dispatches to handlers
```

---

### Phase 3: Visualization Bridge (Python)

**Goal**: A Python process that bridges QEMU's UART2 TCP socket to a WebSocket server for the browser UI.

#### 3a. `tools/viz_bridge.py`

```python
# Connects to QEMU's UART2 TCP port (localhost:1235)
# Runs a WebSocket server (e.g., ws://localhost:8765)
#
# LED frames:   TCP → parse → JSON → WebSocket → browser
# Touch events: WebSocket → browser click → JSON → encode → TCP → QEMU
#
# Dependencies: asyncio, websockets (pip install websockets)
```

Protocol translation:
- **LED frame** (binary from QEMU) → `{"type": "led_frame", "pixels": [[r,g,b], [r,g,b], ...]}` (JSON to browser)
- **Mode change** (binary from QEMU) → `{"type": "mode_change", "mode": 1, "inner": 2, "outer": 3}` (JSON)
- **Touch event** (JSON from browser) → binary touch command → TCP to QEMU

#### 3b. Frame Rate Throttling

The firmware flushes at ~20 Hz. The bridge passes frames through as-is. The browser uses `requestAnimationFrame` for smooth rendering. If needed, the bridge can skip frames (send latest only) to reduce WebSocket bandwidth.

---

### Phase 4: Browser Visualization UI

**Goal**: A single-page web app that renders the badge's LEDs and provides clickable touch sensor buttons.

#### 4a. UI Components

1. **LED Strip Visualizer** (Canvas/SVG)
   - Renders `LED_STRIP_LEN` circles/rectangles arranged in the badge's physical layout
   - Supports all 4 badge variants (TRON: 77 LEDs dual-ring, REACTOR: 48 LEDs dual-ring, CREST: 59 LEDs wing layout, FMAN25: 45 LEDs)
   - Badge variant selection dropdown
   - Each pixel shows its real RGB color
   - Inner/outer ring distinction visible

2. **Touch Sensor Panel**
   - 9 buttons arranged to match badge layout
   - Click = instant touch+release (fires `TOUCHED` then `RELEASED` after 100ms)
   - Click-and-hold = generates `TOUCHED` → (after 1s) `SHORT_PRESSED` → (after 3s) `LONG_PRESSED` → (after 5s) `VERY_LONG_PRESSED` → (on release) `RELEASED`
   - Visual feedback: button changes color based on current press state
   - Labels match badge variant (clock positions for TRON/REACTOR, feather names for CREST, touch zones for FMAN25)

3. **Status Panel**
   - Current LED mode (Sequence / Touch / Battery / Event / Game / BLE / Song / Interactive)
   - Inner/outer LED state
   - Touch event log (scrolling list of recent events with timestamps)
   - Connection status (QEMU connected / disconnected)

4. **Console Log**
   - Streams UART0 output (separate TCP connection to QEMU's console) for cross-referencing firmware logs with visual state

#### 4b. Technology Stack

- **Framework**: Vanilla HTML/CSS/JS or lightweight React — keep it simple, single `index.html` + `viz.js`
- **Rendering**: HTML5 Canvas for LED pixels (good performance at 20+ fps)
- **WebSocket**: Native browser `WebSocket` API
- **Layout**: CSS Grid for responsive arrangement

#### 4c. Badge Layout Data

The UI needs to know the physical position of each LED for each badge variant. This can be a static JSON map:

```json
{
  "TRON": {
    "led_count": 77,
    "inner_ring": {"offset": 0, "count": 27},
    "outer_ring": {"offset": 27, "count": 50},
    "touch_sensors": 9,
    "touch_labels": ["12", "1", "2", "4", "5", "7", "8", "10", "11"]
  },
  "CREST": {
    "led_count": 59,
    "inner_ring": {"offset": 0, "count": 6},
    "outer_ring": {"offset": 6, "count": 53},
    "touch_sensors": 9,
    "touch_labels": ["RW1", "RW2", "RW3", "RW4", "Tail", "LW4", "LW3", "LW2", "LW1"]
  }
}
```

---

### Phase 5: Integration & Testing

#### 5a. Launch Script

```bash
#!/bin/bash
# tools/run_viz.sh — One-command launch for QEMU + visualization

# 1. Build firmware with QEMU mode
idf.py -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.ci.qemu" set-target esp32 build

# 2. Start QEMU with all serial ports
qemu-system-xtensa \
    -nographic \
    -machine esp32 \
    -drive file=build/flash_image.bin,if=mtd,format=raw \
    -serial mon:stdio \
    -serial tcp:localhost:1234,server,nowait \
    -serial tcp:localhost:1235,server,nowait &
QEMU_PID=$!

# 3. Start visualization bridge
python tools/viz_bridge.py --qemu-port 1235 --ws-port 8765 &
BRIDGE_PID=$!

# 4. Open browser UI
open http://localhost:8080  # or python -m http.server 8080 in tools/viz_ui/

# Cleanup on exit
trap "kill $QEMU_PID $BRIDGE_PID" EXIT
wait
```

#### 5b. pytest Integration

The visualization transport can also be used by pytest tests to:
- **Inject touch events** programmatically (connect to TCP port 1235, send touch commands)
- **Assert LED state** (connect to TCP port 1235, read LED frames, verify pixel colors)

```python
# tests/test_touch_led.py

@pytest.mark.esp32
@pytest.mark.qemu
async def test_touch_lights_correct_leds(qemu_dut, viz_client):
    """Verify touching sensor 0 lights the correct LED positions."""
    # Inject touch event for sensor 0
    viz_client.send_touch(sensor_idx=0, event=TOUCH_SENSOR_EVENT_TOUCHED)

    # Wait for LED frame
    frame = await viz_client.read_led_frame(timeout=1.0)

    # Verify expected LEDs are lit (using touchMap from LedControl.c)
    expected_lit = TOUCH_MAP[BADGE_CREST][0]  # e.g., indexes [8,9,10,11,12]
    for idx in expected_lit:
        assert frame.pixels[idx] != (0, 0, 0), f"LED {idx} should be lit"

@pytest.mark.esp32
@pytest.mark.qemu
async def test_touch_mode_cycling(qemu_dut, viz_client):
    """Verify short press on sensor 0 cycles LED sequence."""
    viz_client.send_touch(sensor_idx=0, event=TOUCH_SENSOR_EVENT_SHORT_PRESSED)
    qemu_dut.expect("Touch Action", timeout=5)

@pytest.mark.esp32
@pytest.mark.qemu
async def test_led_sequence_renders(qemu_dut, viz_client):
    """Verify LED sequence mode produces non-black frames."""
    # Wait for boot to complete
    qemu_dut.expect("Initialize finished", timeout=30)

    # Read several LED frames
    for _ in range(10):
        frame = await viz_client.read_led_frame(timeout=2.0)
        if any(p != (0, 0, 0) for p in frame.pixels):
            return  # At least one frame has non-black pixels
    pytest.fail("No non-black LED frames observed")
```

#### 5c. pytest Fixture for Visualization Client

```python
# tests/conftest.py (additions)

@pytest.fixture(scope="session")
async def viz_client():
    """Client for the QEMU visualization transport."""
    client = QemuVizClient(host="localhost", port=1235)
    await client.connect()
    yield client
    await client.close()
```

---

## 5. File Changes Summary

| Action | Path | Description |
|--------|------|-------------|
| **New** | `main/src/stubs/qemu_viz_transport.c` | Shared UART2 transport: init, TX, RX demux task |
| **New** | `main/inc/qemu_viz_transport.h` | Header for viz transport |
| **New** | `main/src/stubs/led_strip_qemu.c` | Virtual `led_strip` driver: RAM buffer + UART TX of pixel frames |
| **New** | `main/src/stubs/TouchSensor_QemuViz.c` | Enhanced touch stub: UART RX → notification dispatch |
| **Remove** | `main/src/stubs/LedControl_Stub.c` | No longer needed — real `LedControl.c` runs |
| **Remove** | `main/src/stubs/TouchSensor_Stub.c` | Replaced by `TouchSensor_QemuViz.c` |
| **Modify** | `main/CMakeLists.txt` | Un-exclude `LedControl.c`; add new stubs; adjust component deps |
| **Modify** | `sdkconfig.ci.qemu` | Add UART2 config, increase UART ring buffer if needed |
| **New** | `tools/viz_bridge.py` | Python TCP↔WebSocket bridge |
| **New** | `tools/viz_ui/index.html` | Browser visualization UI |
| **New** | `tools/viz_ui/viz.js` | UI logic: WebSocket client, Canvas renderer, touch input |
| **New** | `tools/viz_ui/badge_layouts.json` | LED positions and touch labels per badge variant |
| **New** | `tools/run_viz.sh` | One-command launcher script |
| **New** | `tests/test_touch_led.py` | Automated LED/touch integration tests |
| **Modify** | `tests/conftest.py` | Add `viz_client` fixture |

---

## 6. Interaction With Existing QEMU Plans

### Relationship to BLE Emulation (BLE_EMULATION_PLAN.md)

- BLE uses **UART1** for HCI transport; this plan uses **UART2** for visualization. No conflict.
- Both share the same QEMU launch command — just add the third `-serial` argument.
- The interactive game BLE flow (`BleControl_ServiceChar_InteractiveGame.c`) writes to `LedControl` via notifications. With both BLE HCI bridging and LED visualization active, we can test the **full BLE → LED pipeline** end-to-end.

### Relationship to Base QEMU Plan (QEMU_IMPLEMENTATION_PLAN.md)

- Builds on Phase 2 (HAL/stub layer) — this plan replaces two stubs with smarter emulations.
- Keeps `CONFIG_BADGE_QEMU_MODE` as the gate.
- Does not change the base QEMU boot path — just adds richer peripheral emulation.

---

## 7. Risks and Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| ESP-IDF `led_strip` component init tries to configure SPI/RMT hardware | **High** | Override `led_strip_new_spi_device()` with a no-op that returns a valid handle pointing to our RAM buffer |
| QEMU ESP32 may only support 2 UARTs (UART0 + UART1) | **Medium** | Fall back to TCP socket chardev directly (bypass UART driver, use raw TCP socket from FreeRTOS `lwip_socket()`) or multiplex LED/touch onto UART1 alongside HCI with a framing layer |
| `LedControl.c` depends on `DiskUtilities`, `GameState`, `BatterySensor` at init | **Low** | These already work in QEMU (NVS/FAT are supported) or have their own stubs (`BatterySensor_Stub.c`) |
| Real `LedControl.c` references `led_strip.h` types | **None** | `led_strip` component header is still included; only the implementation functions are overridden |
| UI rendering performance at 20 Hz | **Low** | 77 LEDs × 3 bytes = 231 bytes/frame. Canvas can handle this trivially. Throttle to 15 fps if needed |
| Touch timing fidelity (press duration detection) | **Low** | UI sends high-level events (short_press, long_press) directly, bypassing firmware-side duration detection. Alternatively, UI sends touch/release and lets firmware compute duration — both modes are supported |

---

## 8. Estimated Effort

| Phase | Effort | Dependencies |
|-------|--------|-------------|
| Phase 1: LED strip emulation + UART transport | 2–3 days | Base QEMU setup (Phase 1 of QEMU plan) |
| Phase 2: Touch sensor emulation | 1–2 days | Phase 1 (shared transport) |
| Phase 3: Python bridge | 1 day | Phase 1 |
| Phase 4: Browser UI | 2–3 days | Phase 3 |
| Phase 5: Integration + test fixtures | 1–2 days | Phases 1–4 |

**Total: ~7–11 days to full touch+LED visualization.**

---

## 9. Future Enhancements

- **Record & playback**: Record LED frame sequences to file for regression comparison (pixel-diff testing).
- **Automated visual regression**: Compare LED frame snapshots against golden files in CI.
- **GPIO visualization**: Extend the protocol to include eye LEDs (GPIO) and vibration motor state.
- **Audio visualization**: Add a waveform display for the synth/buzzer output (if PWM emulation is added).
- **Multi-badge view**: Run multiple QEMU instances with different badge variants, all connected to the same UI, for IWC visual testing.

---

## 10. References

- [ESP-IDF LED Strip Component](https://components.espressif.com/components/espressif/led_strip)
- [ESP-IDF UART Driver](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/uart.html)
- [ESP32 QEMU Serial Ports](https://github.com/espressif/esp-toolchain-docs/blob/main/qemu/esp32/README.md)
- [QEMU Chardev Options](https://www.qemu.org/docs/master/system/invocation.html#hxtool-5)
- [WebSocket API (MDN)](https://developer.mozilla.org/en-US/docs/Web/API/WebSocket)
- [Python websockets library](https://websockets.readthedocs.io/)
