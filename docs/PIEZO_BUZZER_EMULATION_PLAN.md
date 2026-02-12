# Piezo Buzzer Sound Emulation Plan for QEMU Visualization

## Executive Summary

This document outlines a plan to emulate the badge's **piezo buzzer** (driven by `SynthMode.c` via LEDC PWM) within the QEMU visualization UI, so that real audible sounds play through the host machine's speakers when the firmware triggers tones or songs. This builds on the existing LED + touch emulation architecture (UART2 transport → Python bridge → WebSocket → browser) by adding a new **tone event** message type that the browser renders using the **Web Audio API**.

---

## 1. Current State

### What Exists Today

| Component | Real Implementation | QEMU Stub | Gap |
|-----------|-------------------|-----------|-----|
| **SynthMode** | `SynthMode.c` — uses LEDC PWM on GPIO 18 to drive a piezo buzzer. Plays touch tones (per-sensor frequency mapping) and queued songs (note-by-note with timing). Fires `NOTIFICATION_EVENTS_SONG_NOTE_ACTION` on tone start/stop/song start/stop. | `SynthMode_Stub.c` — initializes struct, logs calls, but **no task, no notification handlers, no tone output**. | The entire sound pipeline is dead: no touch tones, no songs, no song-note notifications (which also drive Song LED mode). |

### Data Flow (Real Hardware)

```
Touch Event → SynthMode_TouchSensorNotificationHandler
    → touchFrequencyMapping[sensorIdx] → SynthMode_PlayTone(note)
        → GetNoteFrequency(note) → ledc_set_freq() → PWM → Piezo Buzzer

Play Song Notification → SynthMode_PlaySongNotificationHandler
    → Enqueue song → SynthModeTask loop:
        → GetSong(song) → iterate notes[]
        → SynthMode_PlayTone(note) / SynthMode_StopTone()
        → Fires NOTIFICATION_EVENTS_SONG_NOTE_ACTION per note change
        → vTaskDelay for note duration
```

### Key Firmware Details

- **Touch-to-frequency mapping** (`SynthMode.c:35-59`): 9 sensors → D3, E3, F3, G3, A3, B3, C4, D4, E4 (with octave shift support)
- **Note frequencies** (`Notes.h`): Full chromatic scale C0–B8, defined as `FREQ_NOTE_*` macros (e.g., `FREQ_NOTE_A4 = 440.0`)
- **Song definitions** (`main/src/songs/*.c`): 22 songs (Zelda themes, sound effects, etc.), each with tempo, note array (note + noteType + slur)
- **Note timing**: `GetNoteTypeInMilliseconds(tempo, noteType)` computes hold time; 50ms pause between non-slurred notes
- **Notification events**: `SONG_NOTE_CHANGE_TYPE_TONE_START`, `TONE_STOP`, `SONG_START`, `SONG_STOP` — these drive the Song LED mode in `LedControl.c`

### Why the Current Stub Is Insufficient

1. **No SynthModeTask** — songs never play, the song queue is never drained.
2. **No notification handlers** — touch events and play-song events are never received.
3. **No `NOTIFICATION_EVENTS_SONG_NOTE_ACTION`** — the Song LED mode never activates because it depends on note-change notifications from SynthMode.
4. **No audible output** — even if the task ran, LEDC PWM doesn't work in QEMU.

---

## 2. Architecture Overview

```
┌────────────────────────────────────────────────────────────────────┐
│                      QEMU (ESP32 Firmware)                         │
│                                                                    │
│  SynthMode.c (REAL, modified)                                      │
│  ┌──────────────────────────────────────────────┐                  │
│  │ SynthModeTask — plays songs note-by-note     │                  │
│  │ TouchSensor handler — plays touch tones      │                  │
│  │                                              │                  │
│  │ SynthMode_PlayTone(note):                    │                  │
│  │   → GetNoteFrequency(note) → frequency       │                  │
│  │   → Send TONE_EVENT msg over UART2           │  ← NEW          │
│  │   → (skip LEDC PWM calls under QEMU)         │                  │
│  │                                              │                  │
│  │ SynthMode_StopTone():                        │                  │
│  │   → Send TONE_STOP msg over UART2            │  ← NEW          │
│  └──────────────────────────────────────────────┘                  │
│                         │ UART2 TX                                  │
└─────────────────────────┼──────────────────────────────────────────┘
                          │ TCP :1235
┌─────────────────────────┼──────────────────────────────────────────┐
│            Visualization Bridge (Python)                            │
│                                                                    │
│  Parses new MSG_TONE_EVENT frames                   ← NEW          │
│  Converts to JSON: {type: "tone", freq, action}                    │
│  Forwards via WebSocket to browser                                 │
└─────────────────────────┼──────────────────────────────────────────┘
                          │ WebSocket
┌─────────────────────────┼──────────────────────────────────────────┐
│               Browser Visualization UI                             │
│                                                                    │
│  Web Audio API OscillatorNode                       ← NEW          │
│  ┌──────────────────────────────────────────────┐                  │
│  │ On "tone_start": create oscillator at freq   │                  │
│  │   → "square" waveform (matches piezo timbre) │                  │
│  │   → Connect to gain node for volume control  │                  │
│  │ On "tone_stop": stop oscillator              │                  │
│  │                                              │                  │
│  │ UI: Volume slider, mute toggle, waveform     │                  │
│  │       indicator in sidebar                   │                  │
│  └──────────────────────────────────────────────┘                  │
└────────────────────────────────────────────────────────────────────┘
```

### Key Design Decisions

1. **Run REAL `SynthMode.c`** — not the stub. The full song-playing task, touch-tone handler, and notification dispatch all run unmodified. Only the LEDC PWM output is replaced with UART2 transport.
2. **New protocol message type `MSG_TONE_EVENT (0x04)`** — carries frequency and action (start/stop) over the existing UART2 viz transport.
3. **Web Audio API square-wave oscillator** — closely approximates a piezo buzzer's timbre. No audio file downloads needed.
4. **Song LED mode works automatically** — because the real `SynthMode.c` fires `NOTIFICATION_EVENTS_SONG_NOTE_ACTION`, the LED mode that visualizes songs will also work.

---

## 3. Communication Protocol

### New Message: Tone Event (Firmware → UI)

```
Byte 0:     0xAA           (frame start marker)
Byte 1:     0x04           (message type: TONE_EVENT)
Byte 2:     uint8_t        (action: 0=stop, 1=start)
Byte 3-4:   uint16_t       (frequency in Hz, little-endian; 0 for stop)
Byte 5:     0x55           (frame end marker)
```

Total: 6 bytes per tone event. Sent on every `SynthMode_PlayTone()` and `SynthMode_StopTone()` call.

### JSON over WebSocket (Bridge → Browser)

```json
{"type": "tone", "action": "start", "frequency": 440}
{"type": "tone", "action": "stop", "frequency": 0}
```

---

## 4. Implementation Plan

### Phase 1: Firmware — Replace SynthMode Stub with Real Implementation

**Goal**: Run the real `SynthMode.c` under QEMU, with LEDC PWM calls replaced by UART2 tone-event messages.

#### 1a. Add `MSG_TONE_EVENT` to the viz transport protocol

In `main/inc/qemu_viz_transport.h`, add:
```c
#define QEMU_VIZ_MSG_TONE_EVENT     0x04
```

#### 1b. Modify `SynthMode.c` to send tone events over UART2 in QEMU mode

Wrap the LEDC calls in `SynthMode_PlayTone()` and `SynthMode_StopTone()` with `#ifdef CONFIG_BADGE_QEMU_MODE` blocks that send tone events over the viz transport instead:

```c
// In SynthMode_PlayTone():
#ifdef CONFIG_BADGE_QEMU_MODE
    // Send tone event over viz transport
    uint16_t freq_hz = (uint16_t)frequency;
    uint8_t frame[] = {
        QEMU_VIZ_FRAME_START,
        QEMU_VIZ_MSG_TONE_EVENT,
        1,  // action: start
        (uint8_t)(freq_hz & 0xFF),
        (uint8_t)((freq_hz >> 8) & 0xFF),
        QEMU_VIZ_FRAME_END
    };
    qemu_viz_tx_lock();
    qemu_viz_send(frame, sizeof(frame));
    qemu_viz_tx_unlock();
#else
    ledc_set_freq(DEFAULT_LEDC_SPEED_MODE, DEFAULT_LEDC_TIMER, frequency);
    ledc_set_duty(DEFAULT_LEDC_SPEED_MODE, DEFAULT_LEDC_CHANNEL, DEFAULT_LEDC_DUTY_ON);
    ledc_update_duty(DEFAULT_LEDC_SPEED_MODE, DEFAULT_LEDC_CHANNEL);
#endif
```

Similarly in `SynthMode_StopTone()`:
```c
#ifdef CONFIG_BADGE_QEMU_MODE
    uint8_t frame[] = {
        QEMU_VIZ_FRAME_START,
        QEMU_VIZ_MSG_TONE_EVENT,
        0,  // action: stop
        0, 0,  // frequency = 0
        QEMU_VIZ_FRAME_END
    };
    qemu_viz_tx_lock();
    qemu_viz_send(frame, sizeof(frame));
    qemu_viz_tx_unlock();
#else
    ledc_set_duty(DEFAULT_LEDC_SPEED_MODE, DEFAULT_LEDC_CHANNEL, DEFAULT_LEDC_DUTY_OFF);
    ledc_update_duty(DEFAULT_LEDC_SPEED_MODE, DEFAULT_LEDC_CHANNEL);
#endif
```

#### 1c. Conditionally skip LEDC PWM initialization in QEMU mode

In `SynthMode_ConfigurePWM()`, guard the `ledc_timer_config()` and `ledc_channel_config()` calls:
```c
#ifdef CONFIG_BADGE_QEMU_MODE
    ESP_LOGI(TAG, "QEMU mode: skipping LEDC PWM config, using viz transport");
    return ESP_OK;
#else
    // existing LEDC init code...
#endif
```

#### 1d. Update `main/CMakeLists.txt`

In the QEMU build configuration:
- **Remove** `src/SynthMode.c` from the excluded sources list (it was excluded because LEDC doesn't work in QEMU)
- **Remove** `src/stubs/SynthMode_Stub.c` from the build (no longer needed)
- Add `qemu_viz_transport.h` include dependency (already exists)

```cmake
# In the QEMU excluded sources, REMOVE this line:
#   "src/SynthMode.c"
# And REMOVE:
#   "src/stubs/SynthMode_Stub.c"  from the QEMU stub sources
```

#### 1e. Ensure `qemu_viz_transport_init()` is called before SynthMode

The viz transport is already initialized early in the boot sequence (used by LED and touch stubs). Verify that `SynthMode_Init()` happens after transport init. No changes likely needed.

---

### Phase 2: Visualization Bridge — Parse Tone Events

**Goal**: Extend `tools/viz_bridge.py` to parse the new `MSG_TONE_EVENT` binary frames and forward them as JSON to WebSocket clients.

#### 2a. Add protocol constant

```python
MSG_TONE_EVENT = 0x04
```

#### 2b. Add handler in `_process_qemu_frames()`

```python
elif msg_type == MSG_TONE_EVENT:
    await self._handle_tone_event()
```

#### 2c. Implement `_handle_tone_event()`

```python
async def _handle_tone_event(self):
    """Parse tone event and broadcast to WebSocket clients."""
    # Read 3 bytes: action (uint8), frequency (uint16 LE)
    data = await self.qemu_reader.readexactly(3)
    end_byte = await self.qemu_reader.readexactly(1)
    if end_byte[0] != FRAME_END:
        log.warning("Missing frame end marker on tone event")
        return

    action = data[0]  # 0=stop, 1=start
    frequency = struct.unpack("<H", data[1:3])[0]

    msg = {
        "type": "tone",
        "action": "start" if action == 1 else "stop",
        "frequency": frequency,
    }
    await self.broadcast_json(msg)
```

---

### Phase 3: Browser UI — Web Audio API Sound Playback

**Goal**: Play real audio tones through the browser when tone events are received, using the Web Audio API with a square-wave oscillator to approximate the piezo buzzer's timbre.

#### 3a. Audio Engine in `viz.js`

```javascript
// ----------------------------------------------------------------
// Piezo Buzzer Audio Emulation (Web Audio API)
// ----------------------------------------------------------------

let audioCtx = null;
let currentOscillator = null;
let currentGainNode = null;
let buzzerVolume = 0.15;  // Default volume (piezo is harsh)
let buzzerMuted = false;

function initAudioContext() {
    if (!audioCtx) {
        audioCtx = new (window.AudioContext || window.webkitAudioContext)();
    }
    // Resume if suspended (browser autoplay policy)
    if (audioCtx.state === 'suspended') {
        audioCtx.resume();
    }
}

function handleToneEvent(msg) {
    // Lazy-init audio context (must happen after user gesture)
    initAudioContext();

    if (msg.action === 'start' && msg.frequency > 0) {
        startTone(msg.frequency);
    } else {
        stopTone();
    }
}

function startTone(frequency) {
    // Stop any existing tone first
    stopTone();

    if (buzzerMuted || !audioCtx) return;

    // Create oscillator with square wave (closest to piezo buzzer)
    currentOscillator = audioCtx.createOscillator();
    currentOscillator.type = 'square';
    currentOscillator.frequency.setValueAtTime(frequency, audioCtx.currentTime);

    // Gain node for volume control + click prevention
    currentGainNode = audioCtx.createGain();
    currentGainNode.gain.setValueAtTime(0, audioCtx.currentTime);
    currentGainNode.gain.linearRampToValueAtTime(
        buzzerVolume, audioCtx.currentTime + 0.005  // 5ms fade-in
    );

    currentOscillator.connect(currentGainNode);
    currentGainNode.connect(audioCtx.destination);
    currentOscillator.start();

    // Update UI indicator
    updateBuzzerIndicator(true, frequency);
}

function stopTone() {
    if (currentGainNode && currentOscillator) {
        // Quick fade-out to prevent click
        const now = audioCtx.currentTime;
        currentGainNode.gain.cancelScheduledValues(now);
        currentGainNode.gain.setValueAtTime(currentGainNode.gain.value, now);
        currentGainNode.gain.linearRampToValueAtTime(0, now + 0.005);

        // Stop oscillator after fade-out
        const osc = currentOscillator;
        setTimeout(() => { try { osc.stop(); } catch(e) {} }, 10);

        currentOscillator = null;
        currentGainNode = null;
    }

    updateBuzzerIndicator(false, 0);
}
```

#### 3b. Add `tone` case to `handleMessage()` in `viz.js`

```javascript
case 'tone':
    handleToneEvent(msg);
    break;
```

#### 3c. UI Controls in `index.html` sidebar

Add a new **Buzzer** panel to the sidebar, between the Status panel and the Event Log:

```html
<div class="panel">
    <div class="panel-header">Buzzer</div>
    <div class="panel-body">
        <div class="status-row">
            <span class="status-label">Status</span>
            <span class="status-value" id="buzzerStatus">Silent</span>
        </div>
        <div class="status-row">
            <span class="status-label">Frequency</span>
            <span class="status-value" id="buzzerFrequency">—</span>
        </div>
        <div class="status-row">
            <span class="status-label">Volume</span>
            <input type="range" id="buzzerVolume" min="0" max="100" value="15"
                style="width:100px; accent-color:var(--accent);">
        </div>
        <div class="status-row">
            <span class="status-label">Mute</span>
            <button id="buzzerMuteBtn"
                style="background:var(--bg-tertiary);color:var(--text-secondary);
                border:1px solid var(--border);border-radius:4px;padding:3px 10px;
                font-family:inherit;font-size:11px;cursor:pointer;">
                🔊 On
            </button>
        </div>
    </div>
</div>
```

#### 3d. Buzzer UI update functions in `viz.js`

```javascript
function updateBuzzerIndicator(playing, frequency) {
    const statusEl = document.getElementById('buzzerStatus');
    const freqEl = document.getElementById('buzzerFrequency');

    if (playing) {
        statusEl.textContent = '♪ Playing';
        statusEl.style.color = 'var(--accent)';
        freqEl.textContent = `${frequency} Hz`;
    } else {
        statusEl.textContent = 'Silent';
        statusEl.style.color = 'var(--text-secondary)';
        freqEl.textContent = '—';
    }
}

// Volume slider handler
document.getElementById('buzzerVolume').addEventListener('input', (e) => {
    buzzerVolume = e.target.value / 100;
    if (currentGainNode) {
        currentGainNode.gain.setValueAtTime(buzzerVolume, audioCtx.currentTime);
    }
});

// Mute button handler
document.getElementById('buzzerMuteBtn').addEventListener('click', () => {
    buzzerMuted = !buzzerMuted;
    const btn = document.getElementById('buzzerMuteBtn');
    btn.textContent = buzzerMuted ? '🔇 Off' : '🔊 On';
    if (buzzerMuted) stopTone();
});

// Initialize AudioContext on first user interaction (browser autoplay policy)
document.addEventListener('click', () => initAudioContext(), { once: true });
document.addEventListener('keydown', () => initAudioContext(), { once: true });
```

#### 3e. Browser Autoplay Policy Handling

Modern browsers block `AudioContext` creation until a user gesture. The plan handles this by:
1. Lazy-initializing `AudioContext` on first `click` or `keydown` event
2. Calling `audioCtx.resume()` if suspended
3. The user will naturally interact with the touch sensor buttons before any sound triggers, satisfying the autoplay policy

---

### Phase 4: Integration & Testing

#### 4a. Verify touch tones

1. Launch QEMU + viz bridge + browser UI
2. Enable touch sound mode (short press on appropriate sensor to enter synth mode, or via console command)
3. Click/press touch sensor buttons in the UI
4. **Expected**: Each sensor plays its mapped note (D3 through E4) through browser audio
5. **Expected**: Releasing the sensor stops the tone

#### 4b. Verify song playback

1. Trigger a song via console command or game event (e.g., `song play 0` for Secret Sound)
2. **Expected**: The browser plays the full note sequence with correct timing, frequencies, and pauses
3. **Expected**: Song LED mode activates simultaneously (because `NOTIFICATION_EVENTS_SONG_NOTE_ACTION` fires)

#### 4c. Verify volume/mute controls

1. Adjust the volume slider — tone loudness should change in real-time
2. Toggle mute — all sound should stop immediately and no new tones should play
3. Unmute — next tone event should resume audio

#### 4d. Edge cases

- Rapid tone changes (fast songs like Secret Sound at 120 BPM with sixteenth notes)
- Slurred notes (no gap between consecutive tones)
- Song interruption (start a new song while one is playing)
- Browser tab in background (AudioContext may suspend — handle gracefully)

---

## 5. File Changes Summary

| Action | Path | Description |
|--------|------|-------------|
| **Modify** | `main/inc/qemu_viz_transport.h` | Add `QEMU_VIZ_MSG_TONE_EVENT 0x04` |
| **Modify** | `main/src/SynthMode.c` | Add `#ifdef CONFIG_BADGE_QEMU_MODE` blocks: skip LEDC init, send tone events via viz transport instead of LEDC PWM |
| **Modify** | `main/CMakeLists.txt` | Un-exclude `SynthMode.c` from QEMU build; remove `SynthMode_Stub.c` from QEMU stubs |
| **Delete** | `main/src/stubs/SynthMode_Stub.c` | No longer needed — real `SynthMode.c` runs with QEMU-conditional output |
| **Modify** | `tools/viz_bridge.py` | Add `MSG_TONE_EVENT = 0x04`; add `_handle_tone_event()` parser; add dispatch in `_process_qemu_frames()` |
| **Modify** | `tools/viz_ui/viz.js` | Add Web Audio API buzzer engine (oscillator, gain, volume, mute); add `tone` case to `handleMessage()`; add buzzer UI update functions |
| **Modify** | `tools/viz_ui/index.html` | Add Buzzer panel to sidebar (status, frequency display, volume slider, mute button) |

---

## 6. Interaction With Existing Emulation

### Relationship to Touch + LED Emulation

- **Touch tones require both systems**: Touch events from the UI → firmware's `SynthMode_TouchSensorNotificationHandler` → `SynthMode_PlayTone` → tone event → UI audio. The touch emulation is already working; this plan adds the audio output path.
- **Song LED mode**: The real `SynthMode.c` fires `NOTIFICATION_EVENTS_SONG_NOTE_ACTION` which `LedControl.c` uses to drive Song LED mode. By running the real SynthMode, **both audio and LED visualization work simultaneously** for songs.

### Relationship to BLE Emulation

- BLE game events can trigger songs (e.g., `SONG_SUCCESS_SOUND`, `SONG_CHEST_SOUND`). With both BLE + audio emulation active, the full game → song → audio + LED pipeline is testable.

### UART2 Transport

- The tone event messages share UART2 with LED frames, mode changes, and touch events. The existing framed protocol with message-type byte handles multiplexing. The tone events are small (6 bytes) and infrequent relative to LED frames.

---

## 7. Risks and Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| Browser autoplay policy blocks AudioContext | **Medium** | Lazy-init AudioContext on first user gesture; UI naturally requires interaction before sounds trigger |
| LEDC driver init fails in QEMU | **High** | Guard `SynthMode_ConfigurePWM()` with `#ifdef CONFIG_BADGE_QEMU_MODE` to skip LEDC config entirely |
| Audio clicks/pops on rapid tone changes | **Low** | Use 5ms gain ramps (fade-in/fade-out) on oscillator start/stop |
| Square wave sounds harsh at high volumes | **Low** | Default volume at 15%; user can adjust via slider. Could also offer "sawtooth" or custom waveform option |
| `SynthMode.c` depends on `qemu_viz_transport.h` (QEMU-only header) | **Low** | Include is inside `#ifdef CONFIG_BADGE_QEMU_MODE` block; normal builds unaffected |
| Song timing drift (QEMU clock vs real-time) | **Low** | Songs use `vTaskDelay` which tracks QEMU's virtual clock. Bridge/browser process events as they arrive. Minor drift is acceptable |

---

## 8. Estimated Effort

| Phase | Effort | Dependencies |
|-------|--------|-------------|
| Phase 1: Firmware — real SynthMode + UART2 tone output | 1–2 hours | Existing viz transport (already working) |
| Phase 2: Bridge — tone event parser | 30 min | Phase 1 |
| Phase 3: Browser UI — Web Audio + controls | 1–2 hours | Phase 2 |
| Phase 4: Integration testing | 1 hour | Phases 1–3 |

**Total: ~3–5 hours** — significantly less than the LED/touch emulation because the transport layer, bridge, and UI framework already exist.

---

## 9. Future Enhancements

- **Waveform selector**: Let the user choose between square, sawtooth, triangle, and sine waveforms to compare timbres.
- **Visual waveform display**: Render a real-time oscilloscope view of the audio output using `AnalyserNode`.
- **Song name display**: Show the currently playing song name in the buzzer panel (requires adding song name to the tone event protocol or inferring from `console_log` messages).
- **Note name display**: Convert frequency back to note name (e.g., "A4") for educational/debugging purposes.
- **Record audio**: Capture the audio output to a WAV file for sharing or regression testing.
- **Piezo frequency response curve**: Apply a frequency-dependent gain curve that models the real piezo buzzer's resonance characteristics for more accurate sound reproduction.
