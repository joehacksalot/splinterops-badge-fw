# Visualization Portal — Event Injection & Testing Control Panel

## Executive Summary

Extend the browser-based badge visualizer (`tools/viz_ui/`) with a **control panel** that lets the tester inject game events, manipulate mock server responses, simulate BLE peers, trigger OTA scenarios, and view touch-combination cheat-sheets — all without restarting QEMU or editing JSON files. The injection flows through the existing mock game server's HTTP API and (where needed) new WebSocket → viz_bridge → QEMU message types.

---

## 1. Architecture Overview

There are two injection paths depending on what we're testing:

### Path A: Mock Server Manipulation (Game Events, OTA, Peers)

The badge firmware already talks to `mock_game_server.py` via HTTP. We add a **REST control API** to the mock server so the viz UI can change responses on the fly. The next time the badge sends a heartbeat, it gets the manipulated response.

```
┌──────────────────────────┐
│   Viz Portal (Browser)   │
│                          │
│  Control Panel UI        │
│    ↓ HTTP POST           │
│    ↓ to mock server      │
├──────────────────────────┤
│                          │        ┌──────────────────────────┐
│  mock_game_server.py     │◄──────►│  QEMU (badge firmware)   │
│    /admin/config   (NEW) │        │  HTTPGameClient.c        │
│    /admin/trigger  (NEW) │        │  OtaUpdate.c             │
│    /heartbeat (existing) │        │  GameState.c             │
└──────────────────────────┘        └──────────────────────────┘
```

### Path B: Direct UART Injection (Force-trigger notifications)

For events that don't naturally flow through the mock server (e.g., forcing a heartbeat send, injecting a BLE peer sighting), we add a new QEMU viz transport message type that the firmware interprets as a "test injection" command.

```
Viz Portal → WebSocket → viz_bridge.py → UART2 TCP → QEMU firmware
                                          (new msg type: QEMU_VIZ_MSG_TEST_INJECT 0x05)
```

---

## 2. Mock Server Control API

### 2a. New Endpoints on `mock_game_server.py`

Add admin endpoints (served on the same port as `/heartbeat`):

| Endpoint | Method | Description |
|----------|--------|-------------|
| `GET /admin/config` | GET | Return current response template |
| `POST /admin/config` | POST | Replace the response template JSON |
| `POST /admin/config/patch` | POST | Merge (patch) fields into current template |
| `POST /admin/trigger/heartbeat` | POST | Force badge to send heartbeat immediately (relayed via viz bridge → UART) |
| `POST /admin/peers` | POST | Set the siblings list in the response template |
| `POST /admin/ota/enable` | POST | Enable OTA endpoint with provided binary or dummy payload |
| `POST /admin/ota/disable` | POST | Disable OTA endpoint (return 404) |
| `GET /admin/state` | GET | Return current badge registry, response template, OTA state |

### 2b. Example: Inject a Game Event

```http
POST /admin/config/patch
Content-Type: application/json

{
  "event": {
    "event": "QlgVrlHvkZs=",
    "stoneColor": 3,
    "power": 80.0,
    "msRemaining": 300000,
    "eventComplete": false
  }
}
```

On the next heartbeat, the badge receives this event and triggers `NOTIFICATION_EVENTS_GAME_EVENT_JOINED`.

### 2c. Example: Add Advertised Peers (Siblings)

```http
POST /admin/peers
Content-Type: application/json

{
  "siblings": ["badge_uuid_1", "badge_uuid_2", "badge_uuid_3"]
}
```

### 2d. Example: Enable OTA Update

```http
POST /admin/ota/enable
Content-Type: application/json

{
  "version": "2.0.0",
  "binary_path": "/path/to/firmware.bin"
}
```

The mock server's `GET /update` endpoint will then serve the binary (or a dummy payload for download progress testing).

### 2e. Implementation in `mock_game_server.py`

Add to the existing `GameServerHandler`:

```python
def do_POST(self):
    if self.path == "/heartbeat":
        # ... existing heartbeat handler ...
    elif self.path == "/admin/config":
        body = self.rfile.read(int(self.headers.get("Content-Length", 0)))
        GameServerHandler.response_template = json.loads(body)
        self._json_response({"status": "ok"})
    elif self.path == "/admin/config/patch":
        body = self.rfile.read(int(self.headers.get("Content-Length", 0)))
        patch = json.loads(body)
        deep_merge(GameServerHandler.response_template, patch)
        self._json_response({"status": "ok", "config": GameServerHandler.response_template})
    elif self.path == "/admin/peers":
        body = self.rfile.read(int(self.headers.get("Content-Length", 0)))
        data = json.loads(body)
        GameServerHandler.response_template["siblings"] = data.get("siblings", [])
        self._json_response({"status": "ok"})
    elif self.path == "/admin/ota/enable":
        body = self.rfile.read(int(self.headers.get("Content-Length", 0)))
        data = json.loads(body)
        GameServerHandler.ota_enabled = True
        GameServerHandler.ota_binary_path = data.get("binary_path", None)
        self._json_response({"status": "ok"})
    elif self.path == "/admin/ota/disable":
        GameServerHandler.ota_enabled = False
        self._json_response({"status": "ok"})
    elif self.path == "/admin/trigger/heartbeat":
        # Relay to viz bridge WebSocket to inject SEND_HEARTBEAT
        self._json_response({"status": "ok", "note": "trigger relayed"})
```

---

## 3. Viz Portal UI — Control Panel

### 3a. Layout

Add a **collapsible left sidebar** (or a tabbed drawer below the LED canvas) with these sections:

```
┌─────────────────────────────────────────────────────────────────┐
│ SPLINTEROPS BADGE VISUALIZER                          [FMAN25] │
├────────────┬───────────────────────────────┬────────────────────┤
│            │                               │  Touch Sensors     │
│  CONTROL   │      LED Canvas               │  Status            │
│  PANEL     │                               │  Buzzer            │
│            │                               │  Event Log         │
│  [Game]    │                               │                    │
│  [OTA]     │                               │                    │
│  [Peers]   │                               │                    │
│  [Keys]    │                               │                    │
│            │                               │                    │
├────────────┴───────────────────────────────┴────────────────────┤
│ Console Logs                                                    │
└─────────────────────────────────────────────────────────────────┘
```

### 3b. Game Event Injection Panel

```
┌─ Game Events ───────────────────────────┐
│                                         │
│  Preset: [▼ Select scenario        ]   │
│    • No Event (clear)                   │
│    • Event Join (Red, 5min)             │
│    • Event Join (Cyan, 10min)           │
│    • Event Complete (fanfare)           │
│    • All Stones Unlocked                │
│    • All Songs Unlocked                 │
│                                         │
│  ── Custom ──                           │
│  Stone Color:  [▼ Red/Yellow/Green/...] │
│  Power Level:  [====●====] 75%          │
│  Duration:     [  5  ] min              │
│  Event ID:     [QlgVrlHvkZs=         ] │
│                                         │
│  Stones: ☑Red ☑Yellow ☐Green           │
│          ☐Cyan ☐Blue ☐Magenta          │
│                                         │
│  Songs:  ☑1 ☐2 ☐3 ☐4 ☑5              │
│                                         │
│  [ Apply & Trigger Heartbeat ]          │
│  [ Apply (wait for next HB)  ]          │
└─────────────────────────────────────────┘
```

### 3c. OTA Update Panel

```
┌─ OTA Update ────────────────────────────┐
│                                         │
│  Status: [ Disabled ]                   │
│                                         │
│  [ Enable OTA (dummy 1MB) ]             │
│  [ Enable OTA (dummy 4MB) ]             │
│  [ Disable OTA ]                        │
│                                         │
│  Binary path: [                       ] │
│  [ Enable OTA (custom) ]                │
│                                         │
│  Note: Badge checks for OTA on WiFi    │
│  connect. Trigger a heartbeat to       │
│  initiate the check.                   │
└─────────────────────────────────────────┘
```

### 3d. Advertised Peers Panel

```
┌─ Peers / Siblings ──────────────────────┐
│                                         │
│  Active siblings in next heartbeat:     │
│                                         │
│  [badge_uuid_1               ] [✕]     │
│  [badge_uuid_2               ] [✕]     │
│  [                           ] [+]     │
│                                         │
│  Quick Add: [ 5 random peers ]          │
│  [ Clear All ]                          │
│                                         │
│  Registered badges: 1                   │
│  (auto-populated from badge registry)   │
└─────────────────────────────────────────┘
```

### 3e. Touch Combinations / Key Reference Panel

This is a **static reference panel** that shows all touch combinations per badge type, along with the corresponding keyboard shortcuts in the viz.

```
┌─ Touch Commands (FMAN25) ───────────────┐
│                                         │
│  Action           Sensors    Keys       │
│  ─────────────────────────────────────  │
│  Enable Touch     Center     [5]        │
│    (short press)  (short)    (hold 1s)  │
│                                         │
│  Disable Touch    L4+Ctr+R4  [4]+[5]+   │
│    (short press)  (short)    [6](hold)  │
│                                         │
│  Next Sequence    Center+R1  [5]+[9]    │
│                   (touch)               │
│                                         │
│  Prev Sequence    L1+Center  [1]+[5]    │
│                   (touch)               │
│                                         │
│  Battery Meter    Center+R2  [5]+[8]    │
│                   (touch)               │
│                                         │
│  Enable BLE       Center+R3  [5]+[7]    │
│                   (touch)               │
│                                         │
│  Disable BLE      L3+Center  [3]+[5]    │
│                   (touch)               │
│                                         │
│  Synth Mode       L1+R1      [1]+[9]    │
│                   (touch)               │
│                                         │
│  Network Test     L2+Center  [2]+[5]    │
│                   (touch)               │
│                                         │
│  Key Layout (left-to-right):            │
│  [1]=L1 [2]=L2 [3]=L3 [4]=L4           │
│  [5]=Center                             │
│  [6]=R4 [7]=R3 [8]=R2 [9]=R1           │
└─────────────────────────────────────────┘
```

**This panel dynamically updates based on the selected badge type** (FMAN25/CREST/TRON/REACTOR), using a data structure in `viz.js` that mirrors `TouchActions.c`.

---

## 4. Touch Combinations Data (All Badge Types)

### FMAN25

| Action | Sensors | Min State | Keys |
|--------|---------|-----------|------|
| Enable Touch | Center | Short Press | `[5]` hold 1s |
| Disable Touch | L4 + Center + R4 | Short Press | `[4]+[5]+[6]` hold 1s |
| Next LED Sequence | Center + R1 | Touch | `[5]+[9]` |
| Prev LED Sequence | L1 + Center | Touch | `[1]+[5]` |
| Battery Meter | Center + R2 | Touch | `[5]+[8]` |
| Enable BLE | Center + R3 | Touch | `[5]+[7]` |
| Disable BLE | L3 + Center | Touch | `[3]+[5]` |
| Toggle Synth Mode | L1 + R1 | Touch | `[1]+[9]` |
| Network Test | L2 + Center | Touch | `[2]+[5]` |

### CREST

| Action | Sensors | Min State | Keys |
|--------|---------|-----------|------|
| Enable Touch | Tail | Short Press | `[5]` hold 1s |
| Disable Touch | RW3 + RW2 + RW1 | Short Press | `[7]+[8]+[9]` hold 1s |
| Next LED Sequence | LW1 + RW1 | Touch | `[1]+[9]` |
| Prev LED Sequence | LW2 + RW1 | Touch | `[2]+[9]` |
| Battery Meter | Tail + RW1 | Touch | `[5]+[9]` |
| Enable BLE | LW1 + Tail | Touch | `[1]+[5]` |
| Disable BLE | LW2 + Tail | Touch | `[2]+[5]` |
| Toggle Synth Mode | LW4 + RW4 | Touch | `[4]+[6]` |
| Network Test | LW4 + Tail + RW4 | Touch | `[4]+[5]+[6]` |

### TRON

| Action | Sensors | Min State | Keys |
|--------|---------|-----------|------|
| Battery Meter | 8 o'clock + 11 o'clock | Touch | `[3]+[9]` |
| Enable BLE | 12 o'clock + 8 o'clock | Touch | `[1]+[3]` (see note) |
| Disable BLE | 12 o'clock + 11 o'clock | Touch | `[1]+[9]` (see note) |
| Next LED Sequence | 2 o'clock + 7 o'clock | Touch | `[7]+[4]` (see note) |

*Note: TRON sensor mapping differs — see `TouchSensor.h` for index-to-clock mapping.*

### REACTOR

| Action | Sensors | Min State | Keys |
|--------|---------|-----------|------|
| Enable Touch | 2+4+8+10 o'clock | Short Press | `[7]+[6]+[3]+[2]` hold 1s |
| Battery Meter | 1 o'clock + 11 o'clock | Touch | `[8]+[9]` |
| Next LED Sequence | 2 o'clock + 10 o'clock | Touch | `[7]+[2]` |
| Prev LED Sequence | 4 o'clock + 10 o'clock | Touch | `[6]+[2]` |
| Enable BLE | 2 o'clock + 8 o'clock | Touch | `[7]+[3]` |
| Disable BLE | 4 o'clock + 8 o'clock | Touch | `[6]+[3]` |
| Toggle Synth Mode | 4+5+7+8 o'clock | Touch | `[6]+[5]+[4]+[3]` |
| Network Test | 5 o'clock + 7 o'clock | Touch | `[5]+[4]` |

---

## 5. Implementation Steps

### Phase 1: Mock Server Admin API
1. Add `/admin/*` endpoints to `mock_game_server.py`
2. Add `deep_merge()` utility for config patching
3. Add OTA binary serving capability to `GET /update`
4. Add CORS headers to admin endpoints (for browser fetch)
5. Test with curl

### Phase 2: Viz Bridge — Heartbeat Trigger Relay
1. Add new message type `QEMU_VIZ_MSG_TEST_INJECT (0x05)` to `qemu_viz_transport.h`
2. Add handler in viz_bridge.py: when the browser sends `{"type": "inject", "command": "send_heartbeat"}`, encode and send to QEMU over UART2
3. Add firmware handler: register RX callback for `0x05` that fires `NOTIFICATION_EVENTS_SEND_HEARTBEAT`

### Phase 3: Viz Portal UI — Control Panel
1. Add left sidebar HTML structure in `index.html`
2. Add CSS for the control panel (collapsible, tabbed sections)
3. Implement `controlPanel.js` (or extend `viz.js`) with:
   - `applyGameEvent()` — POST to `/admin/config/patch`, optionally trigger heartbeat
   - `setOtaState()` — POST to `/admin/ota/enable` or `/admin/ota/disable`
   - `setPeers()` — POST to `/admin/peers`
   - `triggerHeartbeat()` — send `{"type": "inject", "command": "send_heartbeat"}` via WebSocket
4. Add preset buttons for common test scenarios

### Phase 4: Touch Combination Reference Pane
1. Define `TOUCH_COMBOS` data structure in `viz.js` keyed by badge type
2. Build the reference table dynamically when badge type changes
3. Highlight active sensors in real-time as keys are pressed
4. Show which command would fire based on current held keys

### Phase 5: Enhanced Mock Server State Display
1. Add a small status area in the control panel showing:
   - Current mock server response template summary
   - Number of registered badges
   - Last heartbeat request timestamp
   - OTA state (enabled/disabled)
2. Poll `GET /admin/state` periodically (every 5s)

---

## 6. File Changes Summary

| Action | Path | Description |
|--------|------|-------------|
| **Modify** | `tools/mock_game_server.py` | Add `/admin/*` REST endpoints, OTA serving, CORS |
| **Modify** | `tools/viz_ui/index.html` | Add left sidebar control panel HTML |
| **Modify** | `tools/viz_ui/viz.js` | Add control panel logic, touch combo data, mock server API calls |
| **Modify** | `tools/viz_bridge.py` | Handle `inject` WebSocket messages, relay to QEMU |
| **Modify** | `main/inc/qemu_viz_transport.h` | Add `QEMU_VIZ_MSG_TEST_INJECT 0x05` |
| **New** | `main/src/stubs/QemuTestInject.c` | RX handler for test injection commands (fires notifications) |
| **Modify** | `main/CMakeLists.txt` | Include `QemuTestInject.c` in QEMU builds |
| **Modify** | `tools/viz_ui/badge_layouts.json` | Add `touch_combos` per badge type (or embed in viz.js) |

---

## 7. Preset Scenarios (Quick Buttons)

| Button Label | What It Does |
|-------------|-------------|
| **"Join Red Event (5min)"** | Patches event with stoneColor=1, power=75, msRemaining=300000, triggers heartbeat |
| **"Join Cyan Event (10min)"** | Patches event with stoneColor=4, power=50, msRemaining=600000, triggers heartbeat |
| **"Complete Event (fanfare)"** | Patches event with eventComplete=true, power=100, msRemaining=0, triggers heartbeat |
| **"Clear Event"** | Patches event to blank ID `AAAAAAAAAAA=`, triggers heartbeat |
| **"Unlock All Stones"** | Sets stones=[1,2,3,4,5,6], triggers heartbeat |
| **"Unlock All Songs"** | Sets songs=[1,2,3,4,5], triggers heartbeat |
| **"Add 5 Peers"** | Adds 5 random UUID siblings |
| **"Clear Peers"** | Sets siblings=[] |
| **"Enable OTA (1MB dummy)"** | Enables OTA endpoint with dummy binary |
| **"Disable OTA"** | Disables OTA endpoint |

---

## 8. Stone Color ↔ Index Mapping Reference

From `GameTypes.h` and `HTTPGameClient.c`:

| Server `stoneColor` value | `GameState_EventColor` enum | LED Color |
|--------------------------|----------------------------|-----------|
| 1 | `GAMESTATE_EVENTCOLOR_RED` (0) | Red |
| 2 | `GAMESTATE_EVENTCOLOR_YELLOW` (1) | Yellow |
| 3 | `GAMESTATE_EVENTCOLOR_GREEN` (2) | Green |
| 4 | `GAMESTATE_EVENTCOLOR_CYAN` (3) | Cyan |
| 5 | `GAMESTATE_EVENTCOLOR_BLUE` (4) | Blue |
| 6 | `GAMESTATE_EVENTCOLOR_MAGENTA` (5) | Magenta |

*Note: The server uses 1-indexed values; the firmware subtracts 1 when parsing (`stoneColor - 1`).*

Stone bits in `stoneBits` use shifts: Red=bit0, Yellow=bit1, Blue=bit2, Cyan=bit3, Magenta=bit4, Green=bit5.

---

## 9. Future Enhancements

- **BLE Peer Injection via UART**: Add a new inject command that directly fires `NOTIFICATION_EVENTS_BLE_PEER_HEARTBEAT_DETECTED` with a fabricated `PeerReport` struct, bypassing the need for actual BLE advertising
- **Interactive Game Control**: Add buttons to inject `NOTIFICATION_EVENTS_INTERACTIVE_GAME_ACTION` / `NOTIFICATION_EVENTS_INTERACTIVE_GAME_STATE_CHANGE`
- **LED Sequence Browser**: List available LED sequences and allow direct selection
- **Settings Editor**: Modify `UserSettings` (brightness, badge name, etc.) via injection
- **Test Automation**: Expose the control panel API as a programmatic interface for pytest integration
- **Timeline Recorder**: Record and replay sequences of injected events for regression testing
