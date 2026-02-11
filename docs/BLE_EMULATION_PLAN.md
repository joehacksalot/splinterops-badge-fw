# BLE Stack Emulation Plan for SplinterOps Badge Firmware

## Executive Summary

This document evaluates approaches for emulating the BT/BLE stack under QEMU to enable testing of core badge game logic, settings updates, file transfers, interactive game features, and inter-badge communication (IWC) — all without physical hardware. After evaluating three candidate approaches, we recommend **HCI-level bridging** as the primary strategy, with a phased rollout that builds on the existing stub infrastructure.

---

## 1. Current State

### BLE Architecture in the Firmware

The badge firmware uses **NimBLE** (Apache MyNewt) as its BLE host stack, communicating with the ESP32's Bluetooth controller via the **VHCI (Virtual HCI)** interface. The architecture is:

```
┌─────────────────────────────────────────────┐
│            Badge Application Code           │
│  BleControl.c / BleControl_Service.c / ...  │
├─────────────────────────────────────────────┤
│         NimBLE Host Stack (GAP/GATT)        │
│   ble_gap_*, ble_gatts_*, ble_svc_gap_*     │
├─────────────────────────────────────────────┤
│      ESP HCI Transport (esp_nimble_hci)     │
│   ble_hci_trans_hs_* → esp_vhci_host_*     │
├─────────────────────────────────────────────┤
│       ESP32 BT Controller (hardware)        │
│          (not emulated in QEMU)             │
└─────────────────────────────────────────────┘
```

### BLE-Dependent Functionality to Test

| Feature | BLE Surface Used | Files |
|---------|-----------------|-------|
| **IWC Peer Discovery** | GAP passive scanning, adv parsing, manufacturer data | `BleControl_AdvScan.c` |
| **IWC Advertising** | GAP advertising with custom manufacturer payload | `BleControl_Service.c` |
| **File Transfer (LED sequences, settings)** | GATT service, write characteristic, frame reassembly | `BleControl_ServiceChar_FileTransfer.c` |
| **Interactive Game** | GATT read/write/notify characteristic | `BleControl_ServiceChar_InteractiveGame.c` |
| **Service Enable/Disable** | Dynamic GATT service registration/deletion | `BleControl_Service.c` |
| **Connection Mgmt** | GAP connect/disconnect events, MTU negotiation | `BleControl_Service.c` |
| **Pairing** | Pair ID exchange via BLE config frame | `BleControl_ServiceChar_FileTransfer.c` |
| **Touch → BLE Notify** | `ble_gatts_chr_updated()` push notifications | `BleControl_ServiceChar_InteractiveGame.c` |

### Existing Stub (Current QEMU Approach)

`main/src/stubs/BleControl_Stub.c` provides no-op stubs for all BLE functions. This allows the firmware to boot under QEMU but **cannot test any BLE-dependent logic** — all BLE calls are silently swallowed.

---

## 2. Approach Evaluation

### Approach A: API-Level Stubs (Current)

**How it works**: Replace `BleControl_*` functions with no-ops that log and return `ESP_OK`.

| Pros | Cons |
|------|------|
| Already implemented | Cannot test any real BLE logic |
| Zero external dependencies | File transfer frame reassembly untestable |
| Firmware boots cleanly | Interactive game BLE flow untestable |
| | Advertising/scanning payload logic untestable |
| | No coverage of NimBLE host stack interactions |

**Verdict**: Useful as a fallback boot mode, but insufficient for testing BLE-dependent game/settings features.

### Approach B: HCI-Level Bridging (Recommended)

**How it works**: Replace only the bottom HCI transport layer (`esp_nimble_hci`) with a custom transport that bridges HCI packets over a QEMU UART/socket to an external virtual BLE controller (e.g., Google's Bumble). The entire NimBLE host stack, GAP, GATT, and all application BLE code runs unmodified.

```
┌──────────────────────────────────┐     ┌─────────────────────────────┐
│         QEMU (ESP32 FW)         │     │   Host Machine (Python)     │
│                                  │     │                             │
│  Badge App (BleControl_*.c)     │     │  Bumble Virtual Controller  │
│          │                       │     │      (HCI responses)        │
│   NimBLE Host (unmodified)      │     │          │                  │
│          │                       │     │  Bumble Virtual Link Layer  │
│  HCI UART Transport (new)  ────────────→  TCP/Socket HCI Server    │
│          │                       │     │          │                  │
│   QEMU Serial Port / TCP   ─────│─────│─→ Bumble Host/Scripted    │
│                                  │     │    Test Peers              │
└──────────────────────────────────┘     └─────────────────────────────┘
```

| Pros | Cons |
|------|------|
| All application BLE code runs unmodified | Requires custom HCI transport shim |
| Full NimBLE host stack exercised | External dependency on Bumble (Python) |
| Can test advertising, scanning, GATT R/W/N | More complex setup than stubs |
| Can simulate multi-badge interactions | QEMU serial/socket plumbing required |
| Can inject arbitrary BLE events for testing | |
| Bumble provides scriptable peers (GATT client, advertiser) | |
| Proven pattern: Zephyr QEMU + btproxy, Renode + Bumble | |

**Verdict**: Best balance of test fidelity and implementation cost. Exercises the real code paths.

### Approach C: Full Controller Emulation in QEMU

**How it works**: Implement a BLE controller peripheral inside QEMU itself that emulates the ESP32's Bluetooth hardware registers.

| Pros | Cons |
|------|------|
| Completely transparent to firmware | Enormous engineering effort |
| No firmware changes needed | ESP32 BT controller is undocumented proprietary silicon |
| | Would need to reverse-engineer register-level behavior |
| | Not viable without Espressif's direct support |

**Verdict**: Not viable. Espressif has not implemented this in their QEMU fork, and the controller silicon is proprietary.

---

## 3. Viability Confirmation: HCI-Level Bridging

### Why it works for ESP32 + NimBLE

1. **Clean HCI boundary**: ESP-IDF's NimBLE integration has a well-defined HCI transport layer in `esp_nimble_hci.c`. The host calls `ble_hci_trans_hs_cmd_tx()` and `ble_hci_trans_hs_acl_tx()` to send HCI commands/data. The controller calls `host_rcv_pkt()` to deliver events/data back. This is a narrow, well-documented interface.

2. **VHCI is already virtual**: The existing transport (`esp_nimble_hci`) already talks to a *virtual* HCI interface (`esp_vhci_host_send_packet`), not directly to hardware registers. Replacing this with a UART/socket-based H4 transport is a lateral move, not an architectural change.

3. **NimBLE supports UART transport natively**: Apache NimBLE has built-in support for HCI UART (H4) transport (`nimble/transport/uart`). ESP-IDF just happens to use the VHCI variant instead.

4. **Bumble provides the controller side**: Google's Bumble Python library can act as a virtual BLE controller, accepting HCI H4 packets over TCP/socket and providing full link-layer simulation including advertising, scanning, connections, and GATT operations.

5. **Proven in other ecosystems**: Zephyr uses exactly this pattern (QEMU ↔ btproxy ↔ BlueZ controller). Renode uses it with Bumble. The architecture is battle-tested.

### Key Risk: `nimble_port_init()` and Controller Init

On real ESP32, `nimble_port_init()` calls `esp_nimble_hci_init()` which calls `esp_bt_controller_init()` + `esp_bt_controller_enable()`. These functions try to initialize the actual BT controller hardware, which will **fail in QEMU**.

**Mitigation**: We gate the controller init behind `CONFIG_BADGE_QEMU_MODE` and provide a replacement `esp_nimble_hci_init()` that sets up the UART/socket HCI transport instead. The NimBLE host stack itself does not need any changes — it only cares that HCI packets flow.

---

## 4. Implementation Plan

### Phase 1: UART-Based HCI Transport Shim

**Goal**: Replace `esp_nimble_hci` with a custom transport that sends/receives HCI H4 packets over a QEMU serial port or TCP socket.

#### 1a. Create `main/src/hci_transport_qemu.c`

This file replaces `esp_nimble_hci.c` when `CONFIG_BADGE_QEMU_MODE` is enabled. It implements the NimBLE HCI transport API:

```c
// Functions NimBLE host calls to send HCI packets:
int ble_hci_trans_hs_cmd_tx(uint8_t *cmd);     // Send HCI command
int ble_hci_trans_hs_acl_tx(struct os_mbuf *om); // Send ACL data

// Functions we call to deliver HCI packets from controller to host:
// (registered via ble_hci_trans_cfg_hs())
// - evt_cb(uint8_t *hci_ev, void *arg)   // HCI events
// - acl_cb(struct os_mbuf *om, void *arg) // ACL data

// Transport init (replaces esp_nimble_hci_init):
esp_err_t hci_transport_qemu_init(void);
```

The transport:
- Opens a QEMU serial port (UART1) or TCP socket connection
- Wraps outgoing HCI packets in H4 framing (1-byte type indicator + packet)
- Reads incoming H4 frames from the serial/socket and dispatches to NimBLE's registered callbacks
- Runs a FreeRTOS task for the receive loop

#### 1b. QEMU Serial Port Plumbing

QEMU's ESP32 emulation supports multiple UARTs. We dedicate UART1 for HCI transport:

```
# QEMU launch with HCI serial port exposed as TCP
qemu-system-xtensa ... \
    -serial tcp:localhost:1234,server,nowait   # UART1 → TCP for HCI
```

Alternatively, use QEMU's `-chardev socket` option:

```
qemu-system-xtensa ... \
    -chardev socket,id=hci,host=localhost,port=1234,server=on,wait=off \
    -serial chardev:hci
```

#### 1c. Conditional Build Integration

In `main/CMakeLists.txt`, when `CONFIG_BADGE_QEMU_MODE` is set:

- Exclude: `BleControl.c`, `BleControl_AdvScan.c`, `BleControl_Service.c`, `BleControl_ServiceChar_*.c` from using real `esp_nimble_hci`
- Include: `hci_transport_qemu.c` as the HCI transport
- **Keep all application BLE code** — it links against NimBLE host APIs which still work
- Skip `esp_bt_controller_init()` / `esp_bt_controller_enable()` calls

The key insight: we do NOT stub out `BleControl_*.c` anymore. We keep the real BLE application code and only replace the bottom transport layer.

#### 1d. Modified `BleControl_Init()` for QEMU

```c
esp_err_t BleControl_Init(BleControl *this, ...) {
    // ... existing struct init code ...
    
#ifdef CONFIG_BADGE_QEMU_MODE
    // Skip esp_bt_controller_init/enable — no real controller
    // Use our custom HCI transport instead
    ESP_ERROR_CHECK(hci_transport_qemu_init());
#endif
    
    ESP_ERROR_CHECK(nimble_port_init());
    
    // ... rest of NimBLE host config (unchanged) ...
    ble_hs_cfg.reset_cb = _BleControl_ResetCallbackHandler;
    ble_hs_cfg.sync_cb = _BleControl_SyncCallbackHandler;
    // ...
}
```

### Phase 2: Bumble Virtual Controller Setup

**Goal**: Run a Bumble-based virtual BLE controller on the host machine that connects to QEMU's HCI port.

#### 2a. Install Bumble

```bash
pip install bumble
```

#### 2b. Create Virtual Controller Script

```python
# tools/ble_virtual_controller.py
"""
Virtual BLE controller for QEMU testing.
Connects to QEMU's HCI TCP port and provides a full BLE controller.
"""
import asyncio
from bumble.controller import Controller
from bumble.link import LocalLink
from bumble.transport import open_transport

async def main():
    # Create a local link (virtual radio medium)
    link = LocalLink()
    
    # Create virtual controller connected to QEMU via TCP
    transport = await open_transport("tcp-server:0.0.0.0:1234")
    controller = Controller("badge-controller", 
                           host_source=transport.source,
                           host_sink=transport.sink,
                           link=link)
    
    # Keep running
    await asyncio.get_event_loop().create_future()

asyncio.run(main())
```

#### 2c. Multi-Badge Simulation

For testing IWC (inter-badge communication via advertising), we can attach multiple virtual devices to the same Bumble `LocalLink`:

```python
async def main():
    link = LocalLink()
    
    # Controller for the QEMU badge
    qemu_transport = await open_transport("tcp-server:0.0.0.0:1234")
    qemu_controller = Controller("badge", 
                                 host_source=qemu_transport.source,
                                 host_sink=qemu_transport.sink,
                                 link=link)
    
    # Simulated peer badge (Bumble host acting as another badge)
    peer_device = Device(name="PeerBadge")
    peer_controller = Controller("peer", link=link)
    peer_device.host = Host(peer_controller, peer_controller)
    
    # Peer advertises with badge IWC payload
    await peer_device.power_on()
    adv_data = build_iwc_advertising_payload(
        magic=0x1337, badge_type=0x01, 
        badge_id=b'\x01\x02\x03\x04\x05\x06\x07\x08',
        event_id=b'\x00' * 8
    )
    await peer_device.start_advertising(advertising_data=adv_data)
```

### Phase 3: Test Scenarios

#### 3a. IWC Peer Discovery Test

**What it validates**: `BleControl_AdvScan.c` → `_BleControl_ProcessAdvertisement()` → `NotificationDispatcher` event

```python
# tests/test_ble_iwc.py
async def test_peer_discovery(qemu_dut, bumble_link):
    """Verify badge detects peer advertising packets."""
    # Start a Bumble peer advertising with IWC payload
    peer = create_peer_badge(bumble_link, badge_type=BADGE_TRON)
    await peer.start_advertising(iwc_payload(magic=0x1337))
    
    # Expect the QEMU badge to log peer detection
    qemu_dut.expect("Badge advertising packet found", timeout=10)
```

#### 3b. File Transfer Test

**What it validates**: GATT write → frame reassembly → JSON parse → `NotificationDispatcher` event

```python
async def test_settings_file_transfer(qemu_dut, bumble_link):
    """Verify settings update via BLE file transfer."""
    # Connect Bumble GATT client to badge's GATT server
    client = create_gatt_client(bumble_link)
    await client.connect()
    await client.discover_services()
    
    # Write config frame (frame 0)
    config_frame = build_config_frame(num_frames=2, frame_len=200, 
                                       file_type=FILE_TYPE_SETTINGS)
    await client.write_characteristic(FILE_TRANSFER_UUID, config_frame)
    
    # Write data frames
    settings_json = '{"soundEnabled":true,"vibrationEnabled":false}'
    for frame in split_into_frames(settings_json, frame_len=200):
        await client.write_characteristic(FILE_TRANSFER_UUID, frame)
    
    # Verify badge processed the settings
    qemu_dut.expect("Updating settings", timeout=10)
    qemu_dut.expect("NOTIFICATION_EVENTS_BLE_FILE_SETTINGS_RECVD", timeout=5)
```

#### 3c. Interactive Game Test

**What it validates**: GATT write → `_BleControl_BleReceiveInteractiveGameDataAction()` → notification dispatch

```python
async def test_interactive_game_action(qemu_dut, bumble_link):
    """Verify interactive game BLE commands."""
    client = create_gatt_client(bumble_link)
    await client.connect()
    
    # Write interactive game command (feathers to light)
    game_data = struct.pack('<H', 0x000F)  # Light feathers 0-3
    await client.write_characteristic(INTERACTIVE_GAME_UUID, game_data)
    
    # Verify notification dispatched
    qemu_dut.expect("NOTIFICATION_EVENTS_INTERACTIVE_GAME_ACTION", timeout=5)
    
    # Read back touch sensor state
    response = await client.read_characteristic(INTERACTIVE_GAME_UUID)
    # Verify response format matches InteractiveGameData
```

#### 3d. Service Enable/Disable Lifecycle Test

**What it validates**: Dynamic GATT service add/delete, advertising mode changes, timeout timer

```python
async def test_ble_service_lifecycle(qemu_dut, bumble_link):
    """Verify BLE service enable → connect → disconnect → re-enable cycle."""
    # Trigger service enable via BLE service advertisement
    peer = create_service_enabler(bumble_link)
    await peer.start_advertising(service_enable_uuid())
    
    qemu_dut.expect("enabling BLE Service", timeout=10)
    
    # Connect, then disconnect
    client = create_gatt_client(bumble_link)
    await client.connect()
    qemu_dut.expect("Device 0 Connected", timeout=5)
    
    await client.disconnect()
    qemu_dut.expect("Disconnected", timeout=5)
    qemu_dut.expect("Disabling BLE Service", timeout=5)
```

### Phase 4: Integration With Existing Test Framework

#### 4a. pytest Fixture for Bumble + QEMU

```python
# tests/conftest.py
import pytest
import asyncio
from bumble.controller import Controller
from bumble.link import LocalLink
from bumble.transport import open_transport

@pytest.fixture(scope="session")
def bumble_link():
    """Shared virtual radio medium for all tests."""
    return LocalLink()

@pytest.fixture(autouse=True)
async def bumble_controller(bumble_link):
    """Virtual BLE controller connected to QEMU's HCI port."""
    transport = await open_transport("tcp-client:localhost:1234")
    controller = Controller("qemu-badge",
                           host_source=transport.source,
                           host_sink=transport.sink,
                           link=bumble_link)
    yield controller
    await transport.close()
```

#### 4b. Test Runner Script

```bash
#!/bin/bash
# tools/run_ble_tests.sh

# 1. Build firmware with QEMU+BLE mode
idf.py -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.ci.qemu" build

# 2. Start Bumble virtual controller in background
python tools/ble_virtual_controller.py &
BUMBLE_PID=$!

# 3. Start QEMU with HCI serial port
idf.py qemu --qemu-extra-args="-serial tcp:localhost:1234" &
QEMU_PID=$!

# 4. Run tests
cd tests/
pytest --target esp32 --embedded-services idf,qemu test_ble_*.py

# 5. Cleanup
kill $BUMBLE_PID $QEMU_PID
```

---

## 5. File Changes Summary

| Action | Path | Description |
|--------|------|-------------|
| **New** | `main/src/hci_transport_qemu.c` | Custom HCI H4 transport over UART/TCP for QEMU |
| **New** | `main/inc/hci_transport_qemu.h` | Header for QEMU HCI transport |
| **Modify** | `main/src/BleControl.c` | Gate `esp_bt_controller_*` calls on `CONFIG_BADGE_QEMU_MODE`; use `hci_transport_qemu_init()` |
| **Modify** | `main/CMakeLists.txt` | Conditionally link `hci_transport_qemu.c` instead of `esp_nimble_hci` in QEMU mode |
| **Modify** | `main/Kconfig.projbuild` | Ensure `CONFIG_BADGE_QEMU_MODE` option exists (already planned) |
| **Modify** | `sdkconfig.ci.qemu` | Add BLE-related QEMU config (disable BT controller, enable NimBLE host-only) |
| **New** | `tools/ble_virtual_controller.py` | Bumble virtual controller launcher |
| **New** | `tools/ble_test_peers.py` | Scriptable peer badges for IWC testing |
| **New** | `tests/test_ble_iwc.py` | IWC peer discovery tests |
| **New** | `tests/test_ble_file_transfer.py` | BLE file transfer tests |
| **New** | `tests/test_ble_interactive_game.py` | Interactive game BLE tests |
| **New** | `tests/test_ble_service_lifecycle.py` | Service enable/disable tests |
| **Remove** | `main/src/stubs/BleControl_Stub.c` | No longer needed (real BLE code runs) |

---

## 6. sdkconfig Changes for BLE-in-QEMU

```
# In sdkconfig.ci.qemu — additions for BLE emulation

# Enable NimBLE host stack
CONFIG_BT_ENABLED=y
CONFIG_BT_NIMBLE_ENABLED=y

# Disable the ESP32 BT controller (no hardware in QEMU)
CONFIG_BT_CONTROLLER_DISABLED=y

# Keep NimBLE host features enabled
CONFIG_BT_NIMBLE_ROLE_PERIPHERAL=y
CONFIG_BT_NIMBLE_ROLE_CENTRAL=y
CONFIG_BT_NIMBLE_ROLE_OBSERVER=y
CONFIG_BT_NIMBLE_ROLE_BROADCASTER=y

# Use external HCI transport (our custom UART/TCP shim)
CONFIG_BT_NIMBLE_TRANSPORT_UART=y
```

> **Note**: The exact Kconfig symbols depend on the ESP-IDF v5.4.2 NimBLE integration. The key requirement is: enable NimBLE host, disable the ESP32 BT controller, configure an external HCI transport. If `CONFIG_BT_CONTROLLER_DISABLED` is not available as a standalone option, we may need to use `CONFIG_BT_CONTROLLER_ONLY=n` and gate the controller init in code.

---

## 7. Risks and Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| `nimble_port_init()` hardcoded to call `esp_bt_controller_init()` | **High** | Wrap in `#ifdef CONFIG_BADGE_QEMU_MODE`; or provide a weak-linked replacement for the init path |
| ESP-IDF NimBLE transport not easily replaceable | **Medium** | NimBLE's transport is behind `ble_hci_trans_*` function pointers — we can override these at link time |
| Bumble HCI compatibility gaps | **Low** | Bumble's virtual controller is well-tested with Zephyr and Android; standard HCI H4 |
| QEMU UART1 not available or configured differently | **Low** | Fall back to TCP socket chardev; QEMU supports both |
| Timing differences between virtual and real BLE | **Low** | Tests should use event-driven assertions (expect log lines), not timing-based |
| NimBLE host memory allocation in QEMU | **Low** | QEMU emulates full ESP32 memory; NimBLE host allocations work normally |

---

## 8. Implementation Priority / Phasing

| Phase | Effort | What It Unlocks |
|-------|--------|-----------------|
| Phase 1: HCI transport shim | 3-5 days | NimBLE host boots in QEMU, GAP/GATT stack operational |
| Phase 2: Bumble controller | 1-2 days | Virtual radio medium, basic advertising/scanning works |
| Phase 3: Test scenarios | 3-5 days | IWC, file transfer, interactive game, service lifecycle tests |
| Phase 4: pytest integration | 1-2 days | Automated test suite runnable locally and in CI |

**Total: ~8-14 days to full BLE test coverage under QEMU.**

---

## 9. Comparison: Stub vs. HCI Bridge Coverage

| Test Scenario | Stub (current) | HCI Bridge (proposed) |
|---------------|:--------------:|:--------------------:|
| Firmware boots | ✅ | ✅ |
| NVS/FAT/console | ✅ | ✅ |
| IWC peer discovery | ❌ | ✅ |
| Advertising payload correctness | ❌ | ✅ |
| GATT service registration | ❌ | ✅ |
| File transfer frame reassembly | ❌ | ✅ |
| Settings update via BLE | ❌ | ✅ |
| Interactive game commands | ❌ | ✅ |
| BLE service enable/disable timer | ❌ | ✅ |
| Connection parameter negotiation | ❌ | ✅ |
| MTU exchange | ❌ | ✅ |
| Multi-badge simulation | ❌ | ✅ |

---

## 10. References

- [ESP-IDF Controller & VHCI API](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/bluetooth/controller_vhci.html)
- [ESP-IDF NimBLE Host APIs](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/bluetooth/nimble/index.html)
- [esp_nimble_hci.c — ESP-IDF HCI transport source](https://github.com/espressif/esp-idf/blob/master/components/bt/host/nimble/esp-hci/src/esp_nimble_hci.c)
- [Google Bumble — Python Bluetooth Stack](https://google.github.io/bumble/)
- [Bumble Transports (TCP, Socket, VHCI, etc.)](https://google.github.io/bumble/transports/index.html)
- [Apache NimBLE UART Transport](https://github.com/apache/mynewt-nimble/tree/master/nimble/transport/uart)
- [Apache NimBLE Socket Transport](https://github.com/apache/mynewt-nimble/blob/master/nimble/transport/socket/src/ble_hci_socket.c)
- [Zephyr QEMU + btproxy BLE Testing](https://docs.zephyrproject.org/latest/connectivity/bluetooth/bluetooth-tools.html)
- [Renode + Bumble HCI Bridge](https://antmicro.com/blog/2023/08/ble-host-guest-communication-via-hci-in-renode/)
- [emb-team/esp-idf-qemu — WiFi/BLE QEMU fork](https://github.com/emb-team/esp-idf-qemu)
