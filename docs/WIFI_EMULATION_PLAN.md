# WiFi Stack Emulation Plan for SplinterOps Badge Firmware

## Executive Summary

This document outlines a plan to emulate the badge's **WiFi networking stack** under QEMU, enabling testing of the full game heartbeat pipeline, OTA update logic, and HTTP client communication — all without physical hardware or a real WiFi radio. After evaluating three approaches, we recommend **OpenCores Ethernet bridging** as the primary strategy, which replaces the ESP32's WiFi radio with QEMU's emulated Ethernet MAC while keeping the entire `esp_netif` / lwIP / HTTP client stack running unmodified.

---

## 1. Current State

### WiFi Architecture in the Firmware

The badge firmware uses ESP-IDF's WiFi STA (station) driver to connect to access points, then uses `esp_http_client` and `esp_https_ota` over the lwIP TCP/IP stack for all network communication:

```
┌─────────────────────────────────────────────────────────────┐
│                   Badge Application Code                     │
│  HTTPGameClient.c / OtaUpdate.c / WifiClient.c              │
├─────────────────────────────────────────────────────────────┤
│       esp_http_client / esp_https_ota / esp_crt_bundle      │
├─────────────────────────────────────────────────────────────┤
│              lwIP TCP/IP Stack (sockets, TLS)                │
├─────────────────────────────────────────────────────────────┤
│              esp_netif (network interface abstraction)        │
├─────────────────────────────────────────────────────────────┤
│         ESP32 WiFi Driver (esp_wifi_*)                       │
│   esp_wifi_init → esp_wifi_start → esp_wifi_scan_start       │
│   esp_wifi_connect → DHCP → IP_EVENT_STA_GOT_IP             │
│         (requires real WiFi radio — NOT in QEMU)             │
└─────────────────────────────────────────────────────────────┘
```

### WiFi-Dependent Functionality to Test

| Feature | WiFi Surface Used | Files |
|---------|------------------|-------|
| **Game Heartbeat** | HTTP POST to cloud server, JSON request/response | `HTTPGameClient.c`, `GameState.c` |
| **OTA Update** | HTTPS GET + streaming OTA download, version comparison | `OtaUpdate.c` |
| **WiFi Connect/Disconnect** | `esp_wifi_*` STA driver, event handler, retry logic | `WifiClient.c` |
| **Network Test** | Connect + report success/failure via notification | `WifiClient.c` (`WifiClient_TestConnect`) |
| **Game State Sync** | Heartbeat response → stone/song/event state updates | `GameState.c` (`_GameState_ProcessHeartBeatResponse`) |
| **Time Sync** | Server timestamp → `settimeofday()` | `HTTPGameClient.c` (`_ParseJsonResponseString`) |
| **Peer Reporting** | Aggregated BLE peer data sent via HTTP heartbeat | `HTTPGameClient.c` (`HTTPGameClient_GameStateRequestNotificationHandler`) |

### Notification Event Data Flow

```
GameState_Task (periodic timer)
  → NOTIFICATION_EVENTS_WIFI_HEARTBEAT_READY_TO_SEND
      → HTTPGameClient_GameStateRequestNotificationHandler
          → Builds JSON heartbeat payload
          → Enqueues HTTP POST request
              → HTTPGameClientTask picks up request
                  → WifiClient_RequestConnect()
                      → WifiClient_Enable() → esp_wifi_start → scan → connect
                  → WifiClient_WaitForConnected()
                  → esp_http_client_perform() → HTTPS POST to cloud
                  → Parse JSON response
                  → NOTIFICATION_EVENTS_WIFI_HEARTBEAT_RESPONSE_RECV
                      → GameState processes stones/songs/events
                      → NOTIFICATION_EVENTS_GAME_EVENT_JOINED / ENDED
                  → WifiClient_Disconnect()
```

### Existing Stub (Current QEMU Approach)

`main/src/stubs/WifiClient_Stub.c` provides no-op stubs for all `WifiClient_*` functions. `WifiClient_WaitForConnected()` always returns `ESP_FAIL`. This means:

- **HTTPGameClientTask** starts but immediately fails every connection attempt — the request queue is never drained
- **OtaUpdateTask** starts but `WifiClient_WaitForConnected()` always fails — OTA never proceeds
- **GameState** sends `NOTIFICATION_EVENTS_WIFI_HEARTBEAT_READY_TO_SEND` but the heartbeat never reaches the server and no response is ever received
- The entire game state sync pipeline is dead: no stone unlocks, no song unlocks, no event join/end, no time sync

---

## 2. Approach Evaluation

### Approach A: API-Level Stubs (Current)

**How it works**: Replace `WifiClient_*` functions with no-ops that always report disconnected.

| Pros | Cons |
|------|------|
| Already implemented | Cannot test any network-dependent logic |
| Zero external dependencies | HTTPGameClient request/response pipeline untestable |
| Firmware boots cleanly | OTA update logic completely untestable |
| | Game state sync never happens |
| | JSON request building / response parsing untestable via WiFi path |

**Verdict**: Useful as a fallback boot mode, but insufficient for testing WiFi-dependent game and OTA features.

### Approach B: OpenCores Ethernet Bridging (Recommended)

**How it works**: Replace the WiFi radio layer with QEMU's emulated OpenCores Ethernet MAC. The `WifiClient.c` is replaced with a new `WifiClient_QemuEth.c` that uses `esp_eth` + `esp_netif` to bring up networking via the emulated Ethernet. Everything above the network interface layer — lwIP, `esp_http_client`, `esp_https_ota`, TLS — runs **unmodified**. A local mock HTTP server on the host provides canned game server and OTA responses.

```
┌──────────────────────────────────────┐     ┌──────────────────────────────────┐
│          QEMU (ESP32 FW)             │     │        Host Machine              │
│                                      │     │                                  │
│  HTTPGameClient.c (REAL, unmodified) │     │  Mock Game Server (Python)       │
│  OtaUpdate.c      (REAL, unmodified) │     │    /heartbeat → JSON response    │
│          │                           │     │    /update    → OTA binary        │
│  esp_http_client  (unmodified)       │     │                                  │
│  lwIP / TLS       (unmodified)       │     │          ↑                       │
│          │                           │     │          │                       │
│  esp_netif (Ethernet, not WiFi)      │     │     localhost:8080               │
│          │                           │     │                                  │
│  WifiClient_QemuEth.c (NEW)         │     │                                  │
│    esp_eth_mac_new_openeth()         │     │                                  │
│          │                           │     │                                  │
│  OpenCores Ethernet MAC (emulated)  ─────────→  QEMU SLIRP NAT              │
│                                      │     │    (user-mode networking)        │
└──────────────────────────────────────┘     └──────────────────────────────────┘
```

| Pros | Cons |
|------|------|
| HTTPGameClient.c and OtaUpdate.c run **completely unmodified** | Requires new `WifiClient_QemuEth.c` to replace WiFi-specific init |
| Full lwIP, TLS, HTTP client stack exercised | Requires mock HTTP server for game/OTA endpoints |
| Can test real JSON request building + response parsing | QEMU SLIRP NAT adds slight complexity to URL routing |
| Can test OTA version comparison + download pipeline | TLS certificate validation may need adjustment for mock server |
| Can test WiFi state machine logic (connect/disconnect/retry) | |
| Proven: QEMU ESP32 Ethernet support is documented and tested by Espressif | |
| Can test `WifiClient_TestConnect()` → `NOTIFICATION_EVENTS_NETWORK_TEST_COMPLETE` | |

**Verdict**: Best balance of test fidelity and implementation cost. Exercises the real HTTP/networking code paths with minimal firmware changes.

### Approach C: Notification-Level Injection

**How it works**: Keep the WiFi stub as-is, but inject `NOTIFICATION_EVENTS_WIFI_HEARTBEAT_RESPONSE_RECV` events directly via console commands or a test harness, bypassing the entire WiFi + HTTP stack.

| Pros | Cons |
|------|------|
| No network setup needed | WiFi connect/disconnect logic untested |
| Can test GameState response processing | HTTP request building untested |
| Can test JSON response parsing in isolation | OTA download pipeline untested |
| Simple to implement | TLS / certificate handling untested |
| | Not a real network path — may miss integration bugs |

**Verdict**: Useful as a complementary test technique (and already partially possible via console), but does not replace real network testing. Can be used alongside Approach B for targeted GameState tests.

---

## 3. Viability Confirmation: OpenCores Ethernet Bridging

### Why it works for ESP32 under QEMU

1. **QEMU supports OpenCores Ethernet MAC**: Espressif's QEMU fork includes a fully emulated OpenCores Ethernet MAC controller. This is documented and used in ESP-IDF protocol examples.

2. **ESP-IDF has built-in OpenCores Ethernet driver**: The `esp_eth` component supports `esp_eth_mac_new_openeth()` behind `CONFIG_ETH_USE_OPENETH`. This driver provides a standard `esp_netif` network interface — identical in API to the WiFi interface.

3. **lwIP doesn't care about the link layer**: Once `esp_netif` is up and has an IP address (via DHCP from QEMU's SLIRP), all socket operations, HTTP client calls, and TLS connections work identically regardless of whether the underlying link is WiFi or Ethernet.

4. **QEMU SLIRP provides NAT + DHCP**: QEMU's user-mode networking (`-nic user,model=open_eth`) provides a built-in DHCP server and NAT, so the emulated ESP32 automatically gets an IP address and can reach the host network.

5. **Port forwarding enables mock servers**: QEMU's `-nic` option supports `hostfwd` to forward traffic from the guest to host-side mock servers.

### Key Architectural Insight

The `WifiClient.c` abstraction already encapsulates all WiFi-specific operations behind a clean API:

```c
WifiClient_Init()           // Setup
WifiClient_RequestConnect()  // Start connection
WifiClient_WaitForConnected() // Block until connected
WifiClient_GetState()        // Poll state
WifiClient_Disconnect()      // Teardown
WifiClient_TestConnect()     // One-shot connect test
```

`HTTPGameClient.c` and `OtaUpdate.c` only call these `WifiClient_*` functions — they never touch `esp_wifi_*` directly. This means we only need to replace the **internals** of `WifiClient`, not its callers.

### Key Risk: URL Routing

The real firmware connects to `https://us-central1-iwc-dc32.cloudfunctions.net/heartbeat` (HEARTBEAT_URL) and a configured OTA URL. Under QEMU, these external URLs are unreachable. We need to either:

1. **Override URLs at compile time** via `sdkconfig` / `Kconfig` for QEMU builds, or
2. **Use DNS-level redirection** where QEMU's SLIRP DNS resolves the hostnames to the mock server, or
3. **Use an HTTP proxy** that intercepts requests

We recommend option 1 (compile-time URL override) for simplicity.

---

## 4. Implementation Plan

### Phase 1: OpenCores Ethernet WifiClient Replacement

**Goal**: Replace the WiFi-specific `WifiClient.c` with an Ethernet-based implementation that provides identical behavior to callers, using QEMU's OpenCores Ethernet MAC.

#### 1a. Create `main/src/stubs/WifiClient_QemuEth.c`

This file replaces the WiFi stub when `CONFIG_BADGE_QEMU_MODE` is enabled. It provides the same `WifiClient_*` API but uses OpenCores Ethernet instead of `esp_wifi_*`:

```c
#include "WifiClient.h"
#include "esp_eth.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"

static const char *TAG = "WIFI_QEMU_ETH";

// Ethernet event handlers — map to same state machine as WiFi
static void eth_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data);
static void got_ip_handler(void *arg, esp_event_base_t event_base,
                            int32_t event_id, void *event_data);

esp_err_t WifiClient_Init(WifiClient *this, NotificationDispatcher *pNotificationDispatcher,
                           UserSettings *pUserSettings)
{
    assert(this);
    memset(this, 0, sizeof(WifiClient));
    this->pNotificationDispatcher = pNotificationDispatcher;
    this->pUserSettings = pUserSettings;
    this->state = WIFI_CLIENT_STATE_DISCONNECTED;
    this->clientMutex = xSemaphoreCreateMutex();
    this->wifiEventGroup = xEventGroupCreate();

    // Initialize TCP/IP stack and event loop (same as real WifiClient)
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create default Ethernet netif (replaces esp_netif_create_default_wifi_sta)
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&netif_cfg);

    // Initialize OpenCores Ethernet MAC (emulated by QEMU)
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    esp_eth_mac_t *mac = esp_eth_mac_new_openeth(&mac_config);

    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    esp_eth_phy_t *phy = esp_eth_phy_new_dp83848(&phy_config);

    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth_handle = NULL;
    ESP_ERROR_CHECK(esp_eth_driver_install(&eth_config, &eth_handle));
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)));

    // Register event handlers (mirror the WiFi event handler logic)
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID,
                                                &eth_event_handler, this));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP,
                                                &got_ip_handler, this));

    // Start Ethernet (always on — no scan/connect cycle needed)
    ESP_ERROR_CHECK(esp_eth_start(eth_handle));

    ESP_LOGI(TAG, "QEMU Ethernet: initialized OpenCores Ethernet MAC");

    // Start the WifiClient task (same as real implementation)
    assert(xTaskCreatePinnedToCore(_WifiTask, "WifiClientTask",
           configMINIMAL_STACK_SIZE * 2, this,
           WIFI_CONTROL_TASK_PRIORITY, NULL, APP_CPU_NUM) == pdPASS);

    return ESP_OK;
}
```

The key differences from real `WifiClient.c`:
- Uses `esp_eth_mac_new_openeth()` instead of `esp_wifi_init()` + `esp_wifi_start()`
- No AP scanning — Ethernet connects directly
- IP_EVENT_ETH_GOT_IP instead of IP_EVENT_STA_GOT_IP
- ETH_EVENT instead of WIFI_EVENT for link up/down
- `WifiClient_Enable()` / `WifiClient_RequestConnect()` handle state transitions similarly, but the "connection" is effectively instant (Ethernet link up + DHCP)
- The state machine (`DISCONNECTED → ATTEMPTING → CONNECTING → CONNECTED → FAILED`) is preserved identically

#### 1b. Ethernet Event Handlers

The event handlers mirror the WiFi event handler structure:

```c
static void eth_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    WifiClient *this = (WifiClient *)arg;
    if (xSemaphoreTake(this->clientMutex, WIFI_MUTEX_TIMEOUT_MS) == pdTRUE)
    {
        switch (event_id)
        {
            case ETHERNET_EVENT_START:
                this->state = WIFI_CLIENT_STATE_CONNECTING;
                ESP_LOGI(TAG, "Ethernet started");
                break;
            case ETHERNET_EVENT_STOP:
                this->state = WIFI_CLIENT_STATE_DISCONNECTED;
                xEventGroupSetBits(this->wifiEventGroup, WIFI_DISCONNECTED);
                ESP_LOGI(TAG, "Ethernet stopped");
                break;
            case ETHERNET_EVENT_CONNECTED:
                ESP_LOGI(TAG, "Ethernet link up");
                break;
            case ETHERNET_EVENT_DISCONNECTED:
                this->state = WIFI_CLIENT_STATE_FAILED;
                xEventGroupSetBits(this->wifiEventGroup, WIFI_DISCONNECTED);
                ESP_LOGI(TAG, "Ethernet link down");
                break;
        }
        xSemaphoreGive(this->clientMutex);
    }
}

static void got_ip_handler(void *arg, esp_event_base_t event_base,
                            int32_t event_id, void *event_data)
{
    WifiClient *this = (WifiClient *)arg;
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    if (xSemaphoreTake(this->clientMutex, WIFI_MUTEX_TIMEOUT_MS) == pdTRUE)
    {
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        this->retryCount = 0;
        this->state = WIFI_CLIENT_STATE_CONNECTED;
        xEventGroupSetBits(this->wifiEventGroup, WIFI_CONNECTED);
        xSemaphoreGive(this->clientMutex);
    }
}
```

#### 1c. Remaining WifiClient API Functions

`WifiClient_RequestConnect()`, `WifiClient_WaitForConnected()`, `WifiClient_GetState()`, `WifiClient_Disconnect()`, and `WifiClient_TestConnect()` are implemented using the same mutex/event-group/state-machine logic as the real `WifiClient.c`, just without the WiFi-specific scan/connect cycle. Since Ethernet is "always connected" once started, `RequestConnect()` primarily manages the state machine and client reference counting.

#### 1d. Update `main/CMakeLists.txt`

Change the QEMU excluded sources to use the new Ethernet-based WiFi stub:

```cmake
set(QEMU_EXCLUDED_SRCS
    "src/TouchSensor.c"
    "src/BatterySensor.c"
    "src/BleSpec.c"
    "src/WifiClient.c"
    "src/stubs/WifiClient_Stub.c"     # Remove old no-op stub
    "src/stubs/BleControl_Stub.c"
    "src/stubs/LedControl_Stub.c"
    "src/stubs/TouchSensor_Stub.c"
    "src/stubs/SynthMode_Stub.c"
)
# WifiClient_QemuEth.c is in src/stubs/ and will be picked up automatically
```

#### 1e. Update `sdkconfig.ci.qemu`

Add Ethernet-specific configuration:

```
# --- WiFi emulation via OpenCores Ethernet ---
# Enable OpenCores Ethernet MAC (emulated by QEMU)
CONFIG_ETH_USE_OPENETH=y

# Keep WiFi component linked (headers needed) but we don't call esp_wifi_* in QEMU
# The WifiClient_QemuEth.c replaces all WiFi driver calls with Ethernet equivalents
```

`CONFIG_ETH_USE_OPENETH=y` is already present in the current `sdkconfig.ci.qemu`.

#### 1f. WifiClient.h Compatibility

The `WifiClient` struct in `WifiClient.h` includes WiFi-specific fields like `wifi_config_t wifiConfig`, `esp_netif_t *wifiStationConfig`, etc. Two options:

**Option A (Recommended)**: Keep the header unchanged. The `WifiClient_QemuEth.c` simply doesn't use the WiFi-specific struct fields — it stores its Ethernet handle and netif in the existing pointer fields or in file-scope statics. The struct is allocated by the caller (`SystemState`) and is large enough to accommodate unused fields.

**Option B**: Add `#ifdef CONFIG_BADGE_QEMU_MODE` guards in the header to swap WiFi types for Ethernet types. More intrusive, less recommended.

---

### Phase 2: Mock HTTP Game Server

**Goal**: Run a lightweight HTTP server on the host machine that mimics the cloud game server's `/heartbeat` endpoint, returning configurable JSON responses.

#### 2a. Create `tools/mock_game_server.py`

```python
#!/usr/bin/env python3
"""
Mock game server for QEMU WiFi emulation testing.
Provides /heartbeat endpoint with configurable responses.
"""
import json
import time
import argparse
from http.server import HTTPServer, BaseHTTPRequestHandler
import ssl

DEFAULT_RESPONSE = {
    "stones": [1, 3],
    "songs": [5, 2],
    "event": {
        "event": "QlgVrlHvkZs=",
        "stoneColor": 2,
        "power": 64.12,
        "msRemaining": 300000
    },
    "badgeRequestTime": 0,
    "serverResponseTime": {
        "tv_sec": int(time.time()),
        "tv_nsec": 0
    },
    "siblings": []
}

class GameServerHandler(BaseHTTPRequestHandler):
    response_data = DEFAULT_RESPONSE

    def do_POST(self):
        if self.path == "/heartbeat":
            content_length = int(self.headers.get("Content-Length", 0))
            body = self.rfile.read(content_length)
            print(f"[heartbeat] Received: {body.decode('utf-8', errors='replace')[:200]}")

            # Update badgeRequestTime echo and server time
            try:
                req = json.loads(body)
                self.response_data["badgeRequestTime"] = req.get("badgeRequestTime", 0)
            except json.JSONDecodeError:
                pass
            self.response_data["serverResponseTime"]["tv_sec"] = int(time.time())

            response = json.dumps(self.response_data).encode()
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(response)))
            self.end_headers()
            self.wfile.write(response)
        else:
            self.send_response(404)
            self.end_headers()

    def do_GET(self):
        if self.path.startswith("/update"):
            # Serve OTA binary if available
            self.send_response(404)  # No update available by default
            self.end_headers()
        else:
            self.send_response(404)
            self.end_headers()

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=8080)
    parser.add_argument("--response-file", help="JSON file with custom response")
    parser.add_argument("--tls", action="store_true", help="Enable HTTPS with self-signed cert")
    args = parser.parse_args()

    if args.response_file:
        with open(args.response_file) as f:
            GameServerHandler.response_data = json.load(f)

    server = HTTPServer(("0.0.0.0", args.port), GameServerHandler)
    if args.tls:
        server.socket = ssl.wrap_socket(server.socket,
                                         certfile="tools/mock_cert.pem",
                                         keyfile="tools/mock_key.pem",
                                         server_side=True)
    print(f"Mock game server running on port {args.port}")
    server.serve_forever()
```

#### 2b. Configurable Response Scenarios

The mock server supports loading response files for different test scenarios:

```json
// tools/mock_responses/heartbeat_event_join.json
{
    "stones": [1, 2, 3],
    "songs": [1, 5],
    "event": {
        "event": "QlgVrlHvkZs=",
        "stoneColor": 2,
        "power": 75.0,
        "msRemaining": 600000,
        "eventComplete": false
    },
    "serverResponseTime": {"tv_sec": 0, "tv_nsec": 0}
}
```

```json
// tools/mock_responses/heartbeat_event_complete.json
{
    "stones": [1, 2, 3, 4, 5, 6],
    "songs": [1, 2, 3, 4, 5],
    "event": {
        "event": "QlgVrlHvkZs=",
        "stoneColor": 2,
        "power": 100.0,
        "msRemaining": 0,
        "eventComplete": true
    },
    "serverResponseTime": {"tv_sec": 0, "tv_nsec": 0}
}
```

#### 2c. URL Override for QEMU Builds

Add Kconfig options to override the game server and OTA URLs in QEMU mode:

In `main/Kconfig.projbuild`:
```
config QEMU_HEARTBEAT_URL
    string "QEMU mode heartbeat URL"
    default "http://10.0.2.2:8080/heartbeat"
    depends on BADGE_QEMU_MODE
    help
        URL of the mock game server's heartbeat endpoint.
        10.0.2.2 is the host machine's IP as seen from QEMU SLIRP NAT.

config QEMU_OTA_URL
    string "QEMU mode OTA update URL"
    default "http://10.0.2.2:8080/update"
    depends on BADGE_QEMU_MODE
    help
        URL of the mock OTA server endpoint.
```

In `HTTPGameClient.c`, override the URL:
```c
#ifdef CONFIG_BADGE_QEMU_MODE
static const char * HEARTBEAT_URL = CONFIG_QEMU_HEARTBEAT_URL;
#else
static const char * HEARTBEAT_URL = "https://us-central1-iwc-dc32.cloudfunctions.net/heartbeat";
#endif
```

Similarly in `OtaUpdate.c`:
```c
#ifdef CONFIG_BADGE_QEMU_MODE
#define OTA_URL CONFIG_QEMU_OTA_URL
#else
// ... existing per-badge-type URL logic
#endif
```

> **Note on SLIRP addressing**: In QEMU's user-mode networking, the host machine is accessible from the guest at `10.0.2.2`. This is a well-known QEMU SLIRP convention and avoids the need for port forwarding or bridge networking.

#### 2d. TLS Considerations

The real firmware uses `esp_crt_bundle_attach` for TLS certificate validation. Under QEMU with a local mock server:

**Option A (Recommended for simplicity)**: Use plain HTTP (`http://`) for mock server endpoints in QEMU mode. The `esp_http_client` supports HTTP without TLS. This avoids certificate complexity entirely.

**Option B**: Generate a self-signed CA + server certificate, add the CA cert to a custom certificate bundle for QEMU builds, and run the mock server with TLS. This tests the full TLS path but adds setup complexity.

We recommend starting with Option A and adding TLS testing in a later phase.

---

### Phase 3: QEMU Launch Configuration

**Goal**: Configure QEMU to enable OpenCores Ethernet with user-mode networking.

#### 3a. QEMU Command Line

```bash
qemu-system-xtensa \
    -nographic \
    -machine esp32 \
    -drive file=build/flash_image.bin,if=mtd,format=raw \
    -serial mon:stdio \
    -serial tcp:localhost:1234,server,nowait \
    -serial tcp:localhost:1235,server,nowait \
    -nic user,model=open_eth,id=net0 \
    -m 4M
```

The key addition is `-nic user,model=open_eth` which creates the OpenCores Ethernet MAC with SLIRP networking.

If port forwarding is needed (e.g., for incoming connections to the ESP32):
```bash
-nic user,model=open_eth,id=net0,hostfwd=tcp:127.0.0.1:3333-:3333
```

#### 3b. Update `tools/run_viz.sh`

Add the Ethernet NIC option and mock server startup:

```bash
# 2a. Start mock game server in background
python tools/mock_game_server.py --port 8080 &
MOCK_PID=$!

# 2b. Start QEMU with Ethernet + all serial ports
qemu-system-xtensa \
    -nographic \
    -machine esp32 \
    -drive file=build/flash_image.bin,if=mtd,format=raw \
    -serial mon:stdio \
    -serial tcp:localhost:1234,server,nowait \
    -serial tcp:localhost:1235,server,nowait \
    -nic user,model=open_eth \
    -m 4M &
QEMU_PID=$!

# Cleanup on exit
trap "kill $QEMU_PID $BRIDGE_PID $MOCK_PID 2>/dev/null" EXIT
```

---

### Phase 4: Test Scenarios

#### 4a. WiFi Connect / Disconnect Lifecycle Test

**What it validates**: `WifiClient_QemuEth.c` state machine, event handling, `NOTIFICATION_EVENTS_NETWORK_TEST_COMPLETE`

```python
# tests/test_wifi_connect.py
@pytest.mark.esp32
@pytest.mark.qemu
def test_wifi_connects_via_ethernet(dut):
    """Verify WifiClient connects via OpenCores Ethernet in QEMU."""
    dut.expect("QEMU Ethernet: initialized OpenCores Ethernet MAC", timeout=30)
    dut.expect("Got IP:", timeout=30)

@pytest.mark.esp32
@pytest.mark.qemu
def test_network_test_connect(dut):
    """Verify WifiClient_TestConnect reports success."""
    # Trigger network test via console command
    dut.write("wifi test")
    dut.expect("NOTIFICATION_EVENTS_NETWORK_TEST_COMPLETE", timeout=30)
```

#### 4b. Game Heartbeat End-to-End Test

**What it validates**: `GameState` → `HTTPGameClient` → HTTP POST → mock server → JSON parse → `GameState` update

```python
@pytest.mark.esp32
@pytest.mark.qemu
def test_heartbeat_request_response(dut, mock_server):
    """Verify full heartbeat request/response cycle."""
    # Wait for first heartbeat to be sent (GameState periodic timer)
    dut.expect("Heartbeat JSON:", timeout=60)
    dut.expect("HTTP Status = 200", timeout=30)
    dut.expect("Heartbeat Response Sent", timeout=5)
    dut.expect("WIFI Response Recv", timeout=5)

@pytest.mark.esp32
@pytest.mark.qemu
def test_heartbeat_updates_game_state(dut, mock_server):
    """Verify heartbeat response updates stone/song state."""
    # Mock server returns stones=[1,3] and songs=[5,2]
    dut.expect("New status received from cloud", timeout=60)
    dut.expect("stoneBits:", timeout=5)
    dut.expect("songUnlockedBits:", timeout=5)

@pytest.mark.esp32
@pytest.mark.qemu
def test_heartbeat_event_join(dut, mock_server):
    """Verify heartbeat with new event triggers GAME_EVENT_JOINED."""
    # Mock server returns a new event ID
    dut.expect("Game event joined notification", timeout=60)
```

#### 4c. OTA Update Logic Test

**What it validates**: `OtaUpdate.c` version comparison, download progress, abort on same version

```python
@pytest.mark.esp32
@pytest.mark.qemu
def test_ota_no_update_needed(dut, mock_server):
    """Verify OTA skips update when version matches."""
    # Mock server serves OTA binary with same SHA as running firmware
    dut.expect("Connected to WiFi", timeout=60)
    dut.expect("Current version matches update. OTA Skip", timeout=30)

@pytest.mark.esp32
@pytest.mark.qemu
def test_ota_update_available(dut, mock_server_with_ota):
    """Verify OTA detects new version and begins download."""
    dut.expect("OTA Update Starting", timeout=60)
    dut.expect("image download starting", timeout=10)
    dut.expect("Firmware image download progress", timeout=60)
```

#### 4d. Time Sync Test

**What it validates**: `settimeofday()` from server response timestamp

```python
@pytest.mark.esp32
@pytest.mark.qemu
def test_time_sync_from_heartbeat(dut, mock_server):
    """Verify system time is set from heartbeat response."""
    dut.expect("Successfully set the system time", timeout=60)
```

---

### Phase 5: Integration With Existing Test Framework

#### 5a. pytest Fixture for Mock Server

```python
# tests/conftest.py (additions)
import subprocess
import time

@pytest.fixture(scope="session")
def mock_server():
    """Start mock game server for WiFi tests."""
    proc = subprocess.Popen(
        ["python", "tools/mock_game_server.py", "--port", "8080"],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE
    )
    time.sleep(1)  # Wait for server to start
    yield proc
    proc.terminate()
    proc.wait()
```

#### 5b. Console Command for Manual Testing

Add a console command to trigger a heartbeat manually (for interactive debugging):

```
esp> heartbeat
```

This fires `NOTIFICATION_EVENTS_SEND_HEARTBEAT` which triggers `GameState` to send a heartbeat immediately, without waiting for the periodic timer.

---

## 5. File Changes Summary

| Action | Path | Description |
|--------|------|-------------|
| **New** | `main/src/stubs/WifiClient_QemuEth.c` | OpenCores Ethernet-based WifiClient replacement for QEMU |
| **Modify** | `main/CMakeLists.txt` | Exclude `WifiClient_Stub.c` from QEMU build; `WifiClient_QemuEth.c` is auto-included from `src/stubs/` |
| **Modify** | `main/Kconfig.projbuild` | Add `CONFIG_QEMU_HEARTBEAT_URL` and `CONFIG_QEMU_OTA_URL` options |
| **Modify** | `main/src/HTTPGameClient.c` | Add `#ifdef CONFIG_BADGE_QEMU_MODE` URL override for heartbeat endpoint |
| **Modify** | `main/src/OtaUpdate.c` | Add `#ifdef CONFIG_BADGE_QEMU_MODE` URL override for OTA endpoint |
| **Modify** | `sdkconfig.ci.qemu` | Add QEMU heartbeat/OTA URL defaults, verify `CONFIG_ETH_USE_OPENETH=y` |
| **New** | `tools/mock_game_server.py` | Python mock HTTP server for heartbeat + OTA endpoints |
| **New** | `tools/mock_responses/` | Directory with canned JSON response files for test scenarios |
| **Modify** | `tools/run_viz.sh` | Add mock server startup and `-nic user,model=open_eth` to QEMU launch |
| **New** | `tests/test_wifi_connect.py` | WiFi connect/disconnect lifecycle tests |
| **New** | `tests/test_heartbeat.py` | Game heartbeat end-to-end tests |
| **New** | `tests/test_ota_logic.py` | OTA version comparison and download tests |
| **Modify** | `tests/conftest.py` | Add `mock_server` fixture |
| **Delete** | *(none — `WifiClient_Stub.c` is kept for reference but excluded from QEMU build)* | |

---

## 6. sdkconfig Changes for WiFi-in-QEMU

```
# In sdkconfig.ci.qemu — additions for WiFi emulation

# OpenCores Ethernet (already present)
CONFIG_ETH_USE_OPENETH=y

# Mock server URLs (HTTP, not HTTPS, for simplicity)
CONFIG_QEMU_HEARTBEAT_URL="http://10.0.2.2:8080/heartbeat"
CONFIG_QEMU_OTA_URL="http://10.0.2.2:8080/update"

# Disable TLS certificate validation for mock HTTP server
# (only applies in QEMU builds — production builds use real TLS)
# Note: esp_http_client with http:// URLs doesn't use TLS at all,
# so no additional config is needed if using plain HTTP.
```

---

## 7. Interaction With Existing Emulation

### Relationship to BLE Emulation

- BLE uses **UART1** for HCI transport; WiFi uses **OpenCores Ethernet**. No conflict.
- With both BLE and WiFi emulation active, the full game loop works:
  - BLE peer discovery → `NOTIFICATION_EVENTS_BLE_PEER_HEARTBEAT_DETECTED` → `GameState` aggregates peer reports → heartbeat HTTP POST includes peer data → server responds with game state → events/songs/stones update
- The `GameState_SendHeartBeat()` → `HTTPGameClient` → WiFi → server → response → `_GameState_ProcessHeartBeatResponse()` pipeline is fully exercised.

### Relationship to Touch/LED Emulation

- Touch events can trigger game actions that update `GameState`, which eventually leads to heartbeat requests. With WiFi emulation active, these heartbeats actually reach the mock server and get real responses.
- Game event join/end notifications from heartbeat responses drive LED mode changes (`LedModing_SetGameEventActive`), which are now visible in the LED visualization UI.

### Relationship to Piezo Buzzer Emulation

- Heartbeat responses can trigger songs (e.g., `SONG_FANFARE` on `eventComplete`). With both WiFi and buzzer emulation active, the full heartbeat → event complete → fanfare → audio + LED pipeline works.

### UART Port Allocation

| UART | Purpose | TCP Port |
|------|---------|----------|
| UART0 | Console (stdin/stdout) | stdio |
| UART1 | BLE HCI transport | 1234 |
| UART2 | LED/Touch/Tone visualization | 1235 |
| N/A | OpenCores Ethernet (not UART) | SLIRP NAT |

No UART conflicts — Ethernet uses a separate emulated peripheral, not a UART.

---

## 8. Risks and Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| `WifiClient.h` struct has WiFi-specific types that may not compile without `esp_wifi.h` | **Medium** | `esp_wifi.h` is still included in the QEMU build (WiFi component is linked for headers). The struct fields exist but are unused by `WifiClient_QemuEth.c`. |
| QEMU SLIRP DNS resolution for external hostnames | **Low** | We use `10.0.2.2` (host IP) directly, avoiding DNS. If DNS is needed, SLIRP provides a built-in DNS forwarder at `10.0.2.3`. |
| `esp_crt_bundle_attach` may fail for HTTP (non-TLS) URLs | **Low** | `esp_http_client` only uses TLS when the URL scheme is `https://`. With `http://` URLs, the cert bundle is never invoked. |
| Mock server responses may not match real server format exactly | **Medium** | Use captured real-server responses as test fixtures. Validate JSON parse logic against known-good payloads. |
| `esp_eth_mac_new_openeth()` not available when `CONFIG_ETH_USE_OPENETH` is disabled | **Low** | Already gated by `CONFIG_ETH_USE_OPENETH=y` in `sdkconfig.ci.qemu`. |
| Ethernet "connection" is instant — doesn't test WiFi retry/timeout logic | **Low** | The state machine and retry logic are preserved in `WifiClient_QemuEth.c`. We can simulate link-down events to test retry behavior. Alternatively, the real WiFi retry logic is simple enough to verify by code review. |
| OTA download may require larger buffers or timeouts in QEMU | **Low** | QEMU's emulated Ethernet is fast. Adjust `CONFIG_OTA_UPDATE_RECV_TIMEOUT` in QEMU sdkconfig if needed. |

---

## 9. Estimated Effort

| Phase | Effort | Dependencies |
|-------|--------|-------------|
| Phase 1: `WifiClient_QemuEth.c` + build config | 2–3 days | Base QEMU setup (already working) |
| Phase 2: Mock game server + URL overrides | 1–2 days | Phase 1 |
| Phase 3: QEMU launch config | 30 min | Phase 1 |
| Phase 4: Test scenarios | 2–3 days | Phases 1–3 |
| Phase 5: pytest integration | 1 day | Phase 4 |

**Total: ~6–9 days to full WiFi/HTTP/OTA test coverage under QEMU.**

---

## 10. Comparison: Stub vs. Ethernet Bridge Coverage

| Test Scenario | Stub (current) | Ethernet Bridge (proposed) |
|---------------|:--------------:|:--------------------------:|
| Firmware boots | ✅ | ✅ |
| NVS/FAT/console | ✅ | ✅ |
| WiFi connect state machine | ❌ | ✅ |
| WiFi disconnect + retry logic | ❌ | ✅ |
| Network test (WifiClient_TestConnect) | ❌ | ✅ |
| HTTP heartbeat request building | ❌ | ✅ |
| HTTP heartbeat JSON parsing | ❌ | ✅ |
| Game state sync (stones/songs/events) | ❌ | ✅ |
| Time sync from server | ❌ | ✅ |
| Peer report aggregation in heartbeat | ❌ | ✅ |
| Event join/end from heartbeat | ❌ | ✅ |
| OTA version comparison | ❌ | ✅ |
| OTA download pipeline | ❌ | ✅ |
| Full BLE → game → WiFi → LED pipeline | ❌ | ✅ |

---

## 11. Future Enhancements

- **Dynamic mock server control**: REST API on the mock server to change response data mid-test, enabling scenario-driven testing (e.g., inject event start, then event end).
- **HTTPS testing**: Add self-signed TLS to the mock server + custom cert bundle for QEMU builds to test the full TLS handshake path.
- **Network fault injection**: Use QEMU's network configuration to simulate packet loss, latency, or disconnection for resilience testing.
- **Mock OTA server with real binaries**: Serve actual firmware binaries to test the full OTA download + verification + reboot cycle.
- **Multi-badge network simulation**: Run multiple QEMU instances on the same SLIRP network, each reporting to the mock server, to test the full peer-report + game-sync ecosystem.
- **WebSocket live view**: Extend the viz bridge to show real-time heartbeat request/response data in the browser UI, alongside LED and touch state.

---

## 12. References

- [ESP-IDF QEMU Ethernet Support](https://github.com/espressif/esp-toolchain-docs/blob/main/qemu/esp32/README.md#ethernet-support)
- [ESP-IDF OpenCores Ethernet Driver](https://github.com/espressif/esp-idf/tree/master/components/esp_eth/src)
- [ESP-IDF esp_netif API](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/network/esp_netif.html)
- [ESP-IDF Ethernet Basic Example](https://github.com/espressif/esp-idf/tree/master/examples/ethernet/basic)
- [ESP-IDF esp_http_client API](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/protocols/esp_http_client.html)
- [ESP-IDF OTA API](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/ota.html)
- [QEMU User Mode Networking](https://wiki.qemu.org/Documentation/Networking#User_Networking_(SLIRP))
- [QEMU SLIRP IP Addressing](https://wiki.qemu.org/Documentation/Networking#User_Networking_(SLIRP)) (host = 10.0.2.2, DNS = 10.0.2.3)
