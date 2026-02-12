/**
 * SplinterOps Badge Visualizer — Browser UI
 *
 * Connects to the viz_bridge.py WebSocket server to:
 *   - Render LED pixel state in real-time on a Canvas
 *   - Send touch events when the user clicks touch sensor buttons
 *   - Display mode changes and event logs
 */

// ----------------------------------------------------------------
// Constants
// ----------------------------------------------------------------

const WS_URL = `ws://${window.location.hostname || 'localhost'}:8765`;
const MOCK_SERVER_URL = `http://${window.location.hostname || 'localhost'}:9080`;
const TOUCH_EVENT = {
    RELEASED: 0,
    TOUCHED: 1,
    SHORT_PRESSED: 2,
    LONG_PRESSED: 3,
    VERY_LONG_PRESSED: 4,
};

// Synth mode note mappings per badge (from SynthMode.c touchFrequencyMapping[])
const SYNTH_NOTES = {
    FMAN25:  ['D3', 'E3', 'F3', 'G3', 'A3', 'B3', 'C4', 'D4', 'E4'],
    CREST:   ['D3', 'E3', 'F3', 'G3', 'A3', 'B3', 'C4', 'D4', 'E4'],
    TRON:    ['D3', 'E3', 'F3', 'G3', 'A3', 'B3', 'C4', 'D4', 'E4'],
    REACTOR: ['D3', 'E3', 'F3', 'G3', 'A3', 'B3', 'C4', 'D4', 'E4'],
};

const LED_MODE_NAMES = [
    'Sequence', 'Touch', 'Battery', 'Event', 'Game Status',
    'BLE Xfer Enabled', 'BLE Xfer Connected', 'BLE Xfer %',
    'BLE Reconnecting', 'Network Test', 'Song', 'Interactive Game',
    'OTA Download',
];

const INNER_STATE_NAMES = [
    'Off', 'LED Sequence', 'Touch Lighting', 'Game Status', 'Game Event',
    'Battery Status', 'Status Indicator', 'BLE Xfer %', 'Network Test',
];

const OUTER_STATE_NAMES = [
    'Off', 'LED Sequence', 'Touch Lighting', 'Game Event', 'Battery Status',
    'BLE Xfer Status', 'BLE Service Enable', 'BLE Connected', 'OTA Download IP',
    'Status Indicator', 'Game Status', 'Game Interactive', 'BLE Reconnecting',
    'BLE Xfer %', 'Network Test', 'Song Mode',
];

// ----------------------------------------------------------------
// State
// ----------------------------------------------------------------

let ws = null;
let badgeLayouts = {};
let badgePcbPositions = {};
let currentBadge = 'FMAN25';
let pixels = [];
let framesReceived = 0;
let frameTimestamps = [];
let touchTimers = {};

// Console log state
const LEVEL_PRIORITY = { error: 0, warn: 1, info: 2, debug: 3, verbose: 4 };
let consoleLogBuffer = [];       // All received log entries: { level, text }
let consoleAutoScroll = true;
const CONSOLE_MAX_LINES = 5000;  // Max lines kept in buffer

// ----------------------------------------------------------------
// DOM Elements
// ----------------------------------------------------------------

const canvas = document.getElementById('ledCanvas');
const ctx = canvas.getContext('2d');
const badgeSelect = document.getElementById('badgeSelect');
const statusDot = document.getElementById('statusDot');
const statusText = document.getElementById('statusText');
const fpsDisplay = document.getElementById('fpsDisplay');
const touchGrid = document.getElementById('touchGrid');
const eventLog = document.getElementById('eventLog');
const ledModeValue = document.getElementById('ledModeValue');
const innerStateValue = document.getElementById('innerStateValue');
const outerStateValue = document.getElementById('outerStateValue');
const framesRecvValue = document.getElementById('framesRecvValue');
const ledCountDisplay = document.getElementById('ledCountDisplay');
const consoleBody = document.getElementById('consoleBody');
const logLevelFilter = document.getElementById('logLevelFilter');
const logTagFilter = document.getElementById('logTagFilter');
const consoleClearBtn = document.getElementById('consoleClearBtn');
const consoleScrollLockBtn = document.getElementById('consoleScrollLockBtn');
const consoleLineCount = document.getElementById('consoleLineCount');

// ----------------------------------------------------------------
// Badge Layout Loading
// ----------------------------------------------------------------

async function loadBadgeLayouts() {
    try {
        const resp = await fetch('badge_layouts.json');
        badgeLayouts = await resp.json();
    } catch (e) {
        console.error('Failed to load badge_layouts.json:', e);
        // Fallback defaults
        badgeLayouts = {
            FMAN25: { led_count: 45, inner_ring: { offset: 32, count: 13 }, outer_ring: { offset: 0, count: 32 }, touch_sensors: 9, touch_labels: ['R1','R2','R3','R4','Center','L4','L3','L2','L1'] },
        };
    }

    try {
        const resp = await fetch('badge_pcb_positions.json');
        badgePcbPositions = await resp.json();
    } catch (e) {
        console.warn('Failed to load badge_pcb_positions.json, using fallback positions:', e);
        badgePcbPositions = {};
    }
}

// ----------------------------------------------------------------
// WebSocket Connection
// ----------------------------------------------------------------

function connectWebSocket() {
    ws = new WebSocket(WS_URL);

    ws.onopen = () => {
        logEvent('Connected to viz bridge', 'info');
    };

    ws.onclose = () => {
        setConnectionStatus(false);
        logEvent('WebSocket disconnected, reconnecting...', 'error');
        setTimeout(connectWebSocket, 2000);
    };

    ws.onerror = () => {
        // onclose will fire after this
    };

    ws.onmessage = (event) => {
        try {
            const msg = JSON.parse(event.data);
            handleMessage(msg);
        } catch (e) {
            console.error('Failed to parse message:', e);
        }
    };
}

function handleMessage(msg) {
    switch (msg.type) {
        case 'led_frame':
            handleLedFrame(msg);
            break;
        case 'mode_change':
            handleModeChange(msg);
            break;
        case 'status':
            setConnectionStatus(msg.connected);
            break;
        case 'console_log':
            handleConsoleLog(msg);
            break;
        case 'tone':
            handleToneEvent(msg);
            break;
    }
}

function handleLedFrame(msg) {
    pixels = msg.pixels;
    framesReceived++;
    framesRecvValue.textContent = framesReceived;
    ledCountDisplay.textContent = `${msg.num_leds} LEDs`;

    // Track FPS
    const now = performance.now();
    frameTimestamps.push(now);
    // Keep only last 2 seconds of timestamps
    while (frameTimestamps.length > 0 && frameTimestamps[0] < now - 2000) {
        frameTimestamps.shift();
    }
}

function handleModeChange(msg) {
    const modeName = LED_MODE_NAMES[msg.mode] || `Unknown (${msg.mode})`;
    const innerName = INNER_STATE_NAMES[msg.inner_state] || `Unknown (${msg.inner_state})`;
    const outerName = OUTER_STATE_NAMES[msg.outer_state] || `Unknown (${msg.outer_state})`;

    ledModeValue.textContent = modeName;
    innerStateValue.textContent = innerName;
    outerStateValue.textContent = outerName;

    logEvent(`Mode: ${modeName} | Inner: ${innerName} | Outer: ${outerName}`, 'mode');
}

function setConnectionStatus(connected) {
    if (connected) {
        statusDot.classList.add('connected');
        statusText.textContent = 'Connected';
    } else {
        statusDot.classList.remove('connected');
        statusText.textContent = 'Disconnected';
    }
}

// ----------------------------------------------------------------
// Touch Sensor UI
// ----------------------------------------------------------------

// Keyboard-to-sensor mapping.
// Keys 1-9 map left-to-right spatially on the badge:
//   Key 1=L1(idx8), 2=L2(idx7), 3=L3(idx6), 4=L4(idx5),
//   5=Center(idx4), 6=R4(idx3), 7=R3(idx2), 8=R2(idx1), 9=R1(idx0)
// This mapping is rebuilt per badge in buildTouchButtons().
let keyToSensor = {};
let sensorToKey = {};
let activeKeys = new Set();
let touchBtnElements = {};

function buildTouchButtons() {
    touchGrid.innerHTML = '';
    const layout = badgeLayouts[currentBadge];
    if (!layout) return;

    // Build keyboard mapping: keys 1-N map spatially left-to-right.
    // Touch labels are ordered [R1,R2,R3,R4,Center,L4,L3,L2,L1] (right-to-left),
    // so key 1 → last sensor (L1), key 9 → first sensor (R1).
    keyToSensor = {};
    sensorToKey = {};
    touchBtnElements = {};
    const n = layout.touch_sensors;
    for (let k = 0; k < n && k < 9; k++) {
        const sensorIdx = n - 1 - k; // key 1 → sensor n-1, key 2 → sensor n-2, ...
        const key = String(k + 1);   // '1', '2', ..., '9'
        keyToSensor[key] = sensorIdx;
        sensorToKey[sensorIdx] = key;
    }

    for (let i = 0; i < n; i++) {
        const btn = document.createElement('div');
        btn.className = 'touch-btn';
        btn.dataset.index = i;
        const keyHint = sensorToKey[i] ? `<span class="keyhint">[${sensorToKey[i]}]</span>` : '';
        const notes = SYNTH_NOTES[currentBadge] || [];
        const noteHint = notes[i] ? `<span class="note">${notes[i]}</span>` : '';
        btn.innerHTML = `<span class="label">${layout.touch_labels[i]}</span><span class="index">Sensor ${i}</span>${noteHint}${keyHint}`;

        // Mouse events for click-and-hold behavior
        btn.addEventListener('mousedown', (e) => onTouchStart(i, btn, e));
        btn.addEventListener('mouseup', (e) => onTouchEnd(i, btn, e));
        btn.addEventListener('mouseleave', (e) => onTouchEnd(i, btn, e));

        // Touch events for mobile
        btn.addEventListener('touchstart', (e) => { e.preventDefault(); onTouchStart(i, btn, e); });
        btn.addEventListener('touchend', (e) => { e.preventDefault(); onTouchEnd(i, btn, e); });
        btn.addEventListener('touchcancel', (e) => { e.preventDefault(); onTouchEnd(i, btn, e); });

        touchGrid.appendChild(btn);
        touchBtnElements[i] = btn;
    }
}

// Keyboard handlers for multi-touch support
document.addEventListener('keydown', (e) => {
    // Ignore if typing in an input field
    if (e.target.tagName === 'INPUT' || e.target.tagName === 'SELECT' || e.target.tagName === 'TEXTAREA') return;

    const sensorIdx = keyToSensor[e.key];
    if (sensorIdx === undefined) return;
    if (activeKeys.has(e.key)) return; // Already held, ignore repeat

    activeKeys.add(e.key);
    updateComboHighlights();
    const btn = touchBtnElements[sensorIdx];
    if (btn) onTouchStart(sensorIdx, btn, e);
});

document.addEventListener('keyup', (e) => {
    const sensorIdx = keyToSensor[e.key];
    if (sensorIdx === undefined) return;
    if (!activeKeys.has(e.key)) return;

    activeKeys.delete(e.key);
    updateComboHighlights();
    const btn = touchBtnElements[sensorIdx];
    if (btn) onTouchEnd(sensorIdx, btn, e);
});

function onTouchStart(sensorIdx, btn, event) {
    // Clear any existing timer for this sensor
    clearTouchTimers(sensorIdx);

    // Immediately send TOUCHED
    sendTouchEvent(sensorIdx, TOUCH_EVENT.TOUCHED);
    btn.classList.add('touched');

    // Schedule escalating press events
    touchTimers[sensorIdx] = {
        short: setTimeout(() => {
            sendTouchEvent(sensorIdx, TOUCH_EVENT.SHORT_PRESSED);
            btn.classList.remove('touched');
            btn.classList.add('short-pressed');
        }, 1000),

        long: setTimeout(() => {
            sendTouchEvent(sensorIdx, TOUCH_EVENT.LONG_PRESSED);
            btn.classList.remove('short-pressed');
            btn.classList.add('long-pressed');
        }, 3000),

        vlong: setTimeout(() => {
            sendTouchEvent(sensorIdx, TOUCH_EVENT.VERY_LONG_PRESSED);
            btn.classList.remove('long-pressed');
            btn.classList.add('very-long-pressed');
        }, 5000),
    };
}

function onTouchEnd(sensorIdx, btn, event) {
    clearTouchTimers(sensorIdx);

    // Send RELEASED
    sendTouchEvent(sensorIdx, TOUCH_EVENT.RELEASED);
    btn.classList.remove('touched', 'short-pressed', 'long-pressed', 'very-long-pressed', 'active');
}

function clearTouchTimers(sensorIdx) {
    if (touchTimers[sensorIdx]) {
        clearTimeout(touchTimers[sensorIdx].short);
        clearTimeout(touchTimers[sensorIdx].long);
        clearTimeout(touchTimers[sensorIdx].vlong);
        delete touchTimers[sensorIdx];
    }
}

function sendTouchEvent(sensorIdx, eventType) {
    if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({
            type: 'touch_event',
            sensor_idx: sensorIdx,
            event_type: eventType,
        }));
    }
}

// ----------------------------------------------------------------
// Event Log
// ----------------------------------------------------------------

function logEvent(message, type = '') {
    const entry = document.createElement('div');
    entry.className = 'log-entry';

    const now = new Date();
    const timeStr = now.toLocaleTimeString('en-US', { hour12: false }) + '.' + String(now.getMilliseconds()).padStart(3, '0');

    entry.innerHTML = `<span class="log-time">${timeStr}</span><span class="log-msg ${type}">${message}</span>`;

    eventLog.appendChild(entry);

    // Keep max 200 entries
    while (eventLog.children.length > 200) {
        eventLog.removeChild(eventLog.firstChild);
    }

    // Auto-scroll
    eventLog.scrollTop = eventLog.scrollHeight;
}

// ----------------------------------------------------------------
// ANSI escape code → HTML converter
// ----------------------------------------------------------------

const ANSI_COLORS_FG = {
    '30': '#4a4a4a', '31': '#ff4444', '32': '#00ff88', '33': '#ffaa00',
    '34': '#4488ff', '35': '#ff44ff', '36': '#00dddd', '37': '#cccccc',
    '90': '#666666', '91': '#ff6666', '92': '#66ff99', '93': '#ffcc44',
    '94': '#6699ff', '95': '#ff66ff', '96': '#44eeff', '97': '#ffffff',
};

const ANSI_COLORS_BG = {
    '40': '#4a4a4a', '41': '#ff4444', '42': '#00ff88', '43': '#ffaa00',
    '44': '#4488ff', '45': '#ff44ff', '46': '#00dddd', '47': '#cccccc',
    '100': '#666666', '101': '#ff6666', '102': '#66ff99', '103': '#ffcc44',
    '104': '#6699ff', '105': '#ff66ff', '106': '#44eeff', '107': '#ffffff',
};

function escapeHtml(str) {
    return str.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
}

function ansiToHtml(text) {
    // Split on ANSI escape sequences: ESC[ ... m
    const parts = text.split(/(\x1b\[[0-9;]*m)/);
    if (parts.length === 1) {
        // No ANSI codes at all
        return escapeHtml(text);
    }

    let html = '';
    let spanOpen = false;
    let fg = null;
    let bg = null;
    let bold = false;

    for (const part of parts) {
        const match = part.match(/^\x1b\[([0-9;]*)m$/);
        if (match) {
            // Parse SGR parameters
            const codes = match[1] ? match[1].split(';') : ['0'];
            for (const code of codes) {
                if (code === '0' || code === '') {
                    // Reset
                    fg = null; bg = null; bold = false;
                } else if (code === '1') {
                    bold = true;
                } else if (code === '22') {
                    bold = false;
                } else if (ANSI_COLORS_FG[code]) {
                    fg = ANSI_COLORS_FG[code];
                } else if (code === '39') {
                    fg = null;
                } else if (ANSI_COLORS_BG[code]) {
                    bg = ANSI_COLORS_BG[code];
                } else if (code === '49') {
                    bg = null;
                }
            }

            // Close previous span if open
            if (spanOpen) {
                html += '</span>';
                spanOpen = false;
            }

            // Open new span if any style is active
            if (fg || bg || bold) {
                let style = '';
                if (fg) style += `color:${fg};`;
                if (bg) style += `background:${bg};`;
                if (bold) style += 'font-weight:bold;';
                html += `<span style="${style}">`;
                spanOpen = true;
            }
        } else {
            html += escapeHtml(part);
        }
    }

    if (spanOpen) html += '</span>';
    return html;
}

// ----------------------------------------------------------------
// Console Log
// ----------------------------------------------------------------

function handleConsoleLog(msg) {
    const entry = { level: msg.level || 'info', text: msg.text || '' };
    consoleLogBuffer.push(entry);

    // Trim buffer
    if (consoleLogBuffer.length > CONSOLE_MAX_LINES) {
        consoleLogBuffer = consoleLogBuffer.slice(-CONSOLE_MAX_LINES);
    }

    // Update line count
    consoleLineCount.textContent = `${consoleLogBuffer.length} lines`;

    // Check if this entry passes the current filter
    if (entryPassesFilter(entry)) {
        appendConsoleLine(entry);
    }
}

function entryPassesFilter(entry) {
    // Level filter
    const filterLevel = logLevelFilter.value;
    if (LEVEL_PRIORITY[entry.level] > LEVEL_PRIORITY[filterLevel]) {
        return false;
    }

    // Tag filter
    const tagFilter = logTagFilter.value.trim().toLowerCase();
    if (tagFilter && !entry.text.toLowerCase().includes(tagFilter)) {
        return false;
    }

    return true;
}

function appendConsoleLine(entry) {
    const div = document.createElement('div');
    div.className = `console-line level-${entry.level}`;
    div.innerHTML = ansiToHtml(entry.text);
    consoleBody.appendChild(div);

    // Trim visible DOM (keep max 2000 visible lines)
    while (consoleBody.children.length > 2000) {
        consoleBody.removeChild(consoleBody.firstChild);
    }

    if (consoleAutoScroll) {
        consoleBody.scrollTop = consoleBody.scrollHeight;
    }
}

function rebuildConsoleView() {
    consoleBody.innerHTML = '';
    const fragment = document.createDocumentFragment();
    for (const entry of consoleLogBuffer) {
        if (entryPassesFilter(entry)) {
            const div = document.createElement('div');
            div.className = `console-line level-${entry.level}`;
            div.innerHTML = ansiToHtml(entry.text);
            fragment.appendChild(div);
        }
    }
    consoleBody.appendChild(fragment);

    if (consoleAutoScroll) {
        consoleBody.scrollTop = consoleBody.scrollHeight;
    }
}

// Console controls
logLevelFilter.addEventListener('change', rebuildConsoleView);

let tagFilterTimeout = null;
logTagFilter.addEventListener('input', () => {
    clearTimeout(tagFilterTimeout);
    tagFilterTimeout = setTimeout(rebuildConsoleView, 200);
});

consoleClearBtn.addEventListener('click', () => {
    consoleLogBuffer = [];
    consoleBody.innerHTML = '';
    consoleLineCount.textContent = '0 lines';
});

consoleScrollLockBtn.addEventListener('click', () => {
    consoleAutoScroll = !consoleAutoScroll;
    consoleScrollLockBtn.textContent = consoleAutoScroll ? '⬇ Auto' : '⏸ Paused';
    consoleScrollLockBtn.style.color = consoleAutoScroll ? '' : 'var(--warning)';
    if (consoleAutoScroll) {
        consoleBody.scrollTop = consoleBody.scrollHeight;
    }
});

// Pause auto-scroll when user scrolls up manually
consoleBody.addEventListener('scroll', () => {
    const atBottom = consoleBody.scrollHeight - consoleBody.scrollTop - consoleBody.clientHeight < 30;
    if (!atBottom && consoleAutoScroll) {
        consoleAutoScroll = false;
        consoleScrollLockBtn.textContent = '⏸ Paused';
        consoleScrollLockBtn.style.color = 'var(--warning)';
    }
});

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

function updateBuzzerIndicator(playing, frequency) {
    const statusEl = document.getElementById('buzzerStatus');
    const freqEl = document.getElementById('buzzerFrequency');

    if (playing) {
        statusEl.textContent = '\u266A Playing';
        statusEl.style.color = 'var(--accent)';
        freqEl.textContent = `${frequency} Hz`;
    } else {
        statusEl.textContent = 'Silent';
        statusEl.style.color = 'var(--text-secondary)';
        freqEl.textContent = '\u2014';
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
    btn.textContent = buzzerMuted ? '\uD83D\uDD07 Off' : '\uD83D\uDD0A On';
    if (buzzerMuted) stopTone();
});

// Initialize AudioContext on first user interaction (browser autoplay policy)
document.addEventListener('click', () => initAudioContext(), { once: true });
document.addEventListener('keydown', () => initAudioContext(), { once: true });

// ----------------------------------------------------------------
// LED Canvas Rendering
// ----------------------------------------------------------------

function computeLedPositions(layout) {
    const pcbData = badgePcbPositions[currentBadge];
    if (pcbData && pcbData.coords.length === layout.led_count) {
        return mapPcbCoordsToCanvas(pcbData, layout);
    }
    // Fallback to generic concentric circles if no PCB data
    return computeGenericRingPositions(layout);
}

/**
 * Map PCB pick-and-place coordinates to canvas positions for any badge.
 * Applies the correctedPixelOffset mapping: for each logical index i,
 * the physical PCB position is pcbData.coords[pcbData.pixel_offset[i]].
 */
function mapPcbCoordsToCanvas(pcbData, layout) {
    const positions = [];
    const coords = pcbData.coords;
    const pixelOffset = pcbData.pixel_offset;

    // Find bounding box of all physical LED coordinates
    let minX = Infinity, maxX = -Infinity, minY = Infinity, maxY = -Infinity;
    for (const [px, py] of coords) {
        if (px < minX) minX = px;
        if (px > maxX) maxX = px;
        if (py < minY) minY = py;
        if (py > maxY) maxY = py;
    }

    const pcbW = maxX - minX || 1;
    const pcbH = maxY - minY || 1;

    // Map PCB coords to canvas with padding
    const margin = 50;
    const availW = canvas.width - margin * 2;
    const availH = canvas.height - margin * 2;
    const scale = Math.min(availW / pcbW, availH / pcbH);

    // Center the layout on the canvas
    const offsetX = (canvas.width - pcbW * scale) / 2;
    const offsetY = (canvas.height - pcbH * scale) / 2;

    const innerOffset = layout.inner_ring.offset;
    const innerEnd = innerOffset + layout.inner_ring.count;

    for (let i = 0; i < layout.led_count; i++) {
        // Apply pixel offset mapping: logical index i → physical PCB index
        const physIdx = pixelOffset[i];
        const [px, py] = coords[physIdx];

        // Map to canvas: X maps directly, Y is negated (PCB Y convention varies,
        // but we always map minY→bottom, maxY→top for consistent display)
        const screenX = (px - minX) * scale + offsetX;
        const screenY = (maxY - py) * scale + offsetY;

        const ring = (i >= innerOffset && i < innerEnd) ? 'inner' : 'outer';
        positions[i] = { x: screenX, y: screenY, ring };
    }

    return positions;
}

/**
 * Fallback: generic concentric circle positions when no PCB data is available.
 */
function computeGenericRingPositions(layout) {
    const positions = [];
    const cx = canvas.width / 2;
    const cy = canvas.height / 2;

    const innerCount = layout.inner_ring.count;
    const outerCount = layout.outer_ring.count;
    const innerOffset = layout.inner_ring.offset;
    const outerOffset = layout.outer_ring.offset;

    const maxRadius = Math.min(cx, cy) - 30;
    const outerRadius = maxRadius * 0.85;
    const innerRadius = maxRadius * 0.45;

    for (let i = 0; i < innerCount; i++) {
        const angle = (2 * Math.PI * i / innerCount) - Math.PI / 2;
        positions[innerOffset + i] = {
            x: cx + innerRadius * Math.cos(angle),
            y: cy + innerRadius * Math.sin(angle),
            ring: 'inner',
        };
    }

    for (let i = 0; i < outerCount; i++) {
        const angle = (2 * Math.PI * i / outerCount) - Math.PI / 2;
        positions[outerOffset + i] = {
            x: cx + outerRadius * Math.cos(angle),
            y: cy + outerRadius * Math.sin(angle),
            ring: 'outer',
        };
    }

    return positions;
}

let ledPositions = [];
let touchPadPositions = []; // Computed touch pad positions on the canvas

// Per-badge touch sensor → LED index mapping (from firmware touchMap in LedControl.c).
// Each sensor maps to a cluster of outer-ring LEDs.
const TOUCH_LED_MAPS = {
    FMAN25: [
        [21, 22, 23], // sensor 0 (R1) — top-right corner
        [18, 19, 20], // sensor 1 (R2) — right edge upper
        [15, 16, 17], // sensor 2 (R3) — right edge mid
        [13, 14, 15], // sensor 3 (R4) — right edge lower
        [10, 11, 12], // sensor 4 (Center) — bottom vertex
        [7, 8, 9],    // sensor 5 (L4) — left edge lower
        [5, 6, 7],    // sensor 6 (L3) — left edge mid
        [2, 3, 4],    // sensor 7 (L2) — left edge upper
        [0, 1, 31],   // sensor 8 (L1) — top-left corner
    ],
    TRON: [
        [24, 25, 47, 35, 36, 37], // sensor 0 (12 o'clock)
        [26, 27, 24],             // sensor 1 (1 o'clock)
        [28, 29, 30],             // sensor 2 (2 o'clock)
        [30, 31, 32],             // sensor 3 (4 o'clock)
        [33, 34, 24],             // sensor 4 (5 o'clock)
        [38, 39, 24],             // sensor 5 (7 o'clock)
        [40, 41, 42],             // sensor 6 (8 o'clock)
        [42, 43, 44],             // sensor 7 (10 o'clock)
        [45, 46, 47],             // sensor 8 (11 o'clock)
    ],
    REACTOR: [
        [24, 25, 47, 35, 36, 37], // sensor 0 (12 o'clock)
        [26, 27, 24],             // sensor 1 (1 o'clock)
        [28, 29, 30],             // sensor 2 (2 o'clock)
        [30, 31, 32],             // sensor 3 (4 o'clock)
        [33, 34, 24],             // sensor 4 (5 o'clock)
        [38, 39, 24],             // sensor 5 (7 o'clock)
        [40, 41, 42],             // sensor 6 (8 o'clock)
        [42, 43, 44],             // sensor 7 (10 o'clock)
        [45, 46, 47],             // sensor 8 (11 o'clock)
    ],
    CREST: [
        [8, 9, 10, 11, 12],      // sensor 0 (RW1)
        [16, 17, 18],             // sensor 1 (RW2)
        [23, 24],                 // sensor 2 (RW3)
        [28],                     // sensor 3 (RW4)
        [31],                     // sensor 4 (Tail)
        [35],                     // sensor 5 (LW4)
        [40, 41],                 // sensor 6 (LW3)
        [46, 47, 48],             // sensor 7 (LW2)
        [52, 53, 54, 55, 56],    // sensor 8 (LW1)
    ],
};

/**
 * Compute touch pad positions from LED positions for any badge.
 * Each pad is placed at the centroid of its mapped LED cluster,
 * pushed slightly outward from the overall LED centroid.
 */
function computeTouchPadPositions() {
    const layout = badgeLayouts[currentBadge];
    const touchMap = TOUCH_LED_MAPS[currentBadge];
    if (!layout || !touchMap || ledPositions.length < layout.led_count) {
        touchPadPositions = [];
        return;
    }

    // Compute overall centroid of all LED positions
    let ctrX = 0, ctrY = 0, count = 0;
    for (const pos of ledPositions) {
        if (pos) { ctrX += pos.x; ctrY += pos.y; count++; }
    }
    ctrX /= count;
    ctrY /= count;

    touchPadPositions = [];
    for (let s = 0; s < touchMap.length; s++) {
        const ledIdxs = touchMap[s];
        // Centroid of mapped LEDs
        let sx = 0, sy = 0, validCount = 0;
        for (const idx of ledIdxs) {
            if (ledPositions[idx]) {
                sx += ledPositions[idx].x;
                sy += ledPositions[idx].y;
                validCount++;
            }
        }
        if (validCount === 0) continue;
        sx /= validCount;
        sy /= validCount;

        // Push outward from center by a fixed amount
        const dx = sx - ctrX;
        const dy = sy - ctrY;
        const dist = Math.sqrt(dx * dx + dy * dy);
        const pushOut = 18;
        touchPadPositions[s] = {
            x: dist > 1 ? sx + (dx / dist) * pushOut : sx,
            y: dist > 1 ? sy + (dy / dist) * pushOut : sy,
            label: layout.touch_labels[s],
            sensorIdx: s,
        };
    }
}

/**
 * Draw per-badge shape guide outlines on the canvas.
 * Uses actual LED positions to derive the guide geometry.
 */
function drawShapeGuide(layout) {
    if (ledPositions.length < layout.led_count) return;

    if (currentBadge === 'FMAN25') {
        // Outer triangle: D1 (idx 0) = top-left, D12 (idx 11) = bottom, D23 (idx 22) = top-right
        const pTL = ledPositions[0];
        const pB  = ledPositions[11];
        const pTR = ledPositions[22];
        ctx.beginPath();
        ctx.moveTo(pTL.x, pTL.y);
        ctx.lineTo(pTR.x, pTR.y);
        ctx.lineTo(pB.x, pB.y);
        ctx.closePath();
        ctx.stroke();

        // Inner triangle: D33 (idx 32) = top-left, D37 (idx 36) = bottom, D41 (idx 40) = top-right
        const iTL = ledPositions[32];
        const iB  = ledPositions[36];
        const iTR = ledPositions[40];
        ctx.beginPath();
        ctx.moveTo(iTL.x, iTL.y);
        ctx.lineTo(iTR.x, iTR.y);
        ctx.lineTo(iB.x, iB.y);
        ctx.closePath();
        ctx.stroke();

    } else if (currentBadge === 'TRON' || currentBadge === 'REACTOR') {
        // Draw two concentric circles derived from inner and outer ring LED positions
        const innerOffset = layout.inner_ring.offset;
        const innerEnd = innerOffset + layout.inner_ring.count;
        const outerOffset = layout.outer_ring.offset;
        const outerEnd = outerOffset + layout.outer_ring.count;

        // Compute centroid
        let ctrX = 0, ctrY = 0, cnt = 0;
        for (const pos of ledPositions) {
            if (pos) { ctrX += pos.x; ctrY += pos.y; cnt++; }
        }
        ctrX /= cnt; ctrY /= cnt;

        // Average radius for each ring
        let innerR = 0, innerC = 0, outerR = 0, outerC = 0;
        for (let i = 0; i < layout.led_count; i++) {
            if (!ledPositions[i]) continue;
            const dx = ledPositions[i].x - ctrX;
            const dy = ledPositions[i].y - ctrY;
            const d = Math.sqrt(dx * dx + dy * dy);
            if (i >= innerOffset && i < innerEnd) { innerR += d; innerC++; }
            if (i >= outerOffset && i < outerEnd) { outerR += d; outerC++; }
        }
        if (innerC > 0) {
            ctx.beginPath();
            ctx.arc(ctrX, ctrY, innerR / innerC, 0, Math.PI * 2);
            ctx.stroke();
        }
        if (outerC > 0) {
            ctx.beginPath();
            ctx.arc(ctrX, ctrY, outerR / outerC, 0, Math.PI * 2);
            ctx.stroke();
        }

    } else if (currentBadge === 'CREST') {
        // Draw convex hull of outer ring LED positions (irregular crest shape)
        const outerOffset = layout.outer_ring.offset;
        const outerEnd = outerOffset + layout.outer_ring.count;
        const outerPts = [];
        for (let i = outerOffset; i < outerEnd; i++) {
            if (ledPositions[i]) outerPts.push(ledPositions[i]);
        }
        if (outerPts.length > 2) {
            const hull = convexHull(outerPts);
            ctx.beginPath();
            ctx.moveTo(hull[0].x, hull[0].y);
            for (let i = 1; i < hull.length; i++) {
                ctx.lineTo(hull[i].x, hull[i].y);
            }
            ctx.closePath();
            ctx.stroke();
        }
    }
}

/**
 * Compute convex hull of a set of 2D points using Graham scan.
 */
function convexHull(points) {
    if (points.length < 3) return points.slice();

    // Find bottom-most (then left-most) point
    let start = 0;
    for (let i = 1; i < points.length; i++) {
        if (points[i].y > points[start].y ||
            (points[i].y === points[start].y && points[i].x < points[start].x)) {
            start = i;
        }
    }

    const pivot = points[start];
    const sorted = points.slice().sort((a, b) => {
        const angleA = Math.atan2(a.y - pivot.y, a.x - pivot.x);
        const angleB = Math.atan2(b.y - pivot.y, b.x - pivot.x);
        if (angleA !== angleB) return angleA - angleB;
        const distA = (a.x - pivot.x) ** 2 + (a.y - pivot.y) ** 2;
        const distB = (b.x - pivot.x) ** 2 + (b.y - pivot.y) ** 2;
        return distA - distB;
    });

    const hull = [];
    for (const p of sorted) {
        while (hull.length >= 2) {
            const a = hull[hull.length - 2];
            const b = hull[hull.length - 1];
            const cross = (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
            if (cross <= 0) hull.pop();
            else break;
        }
        hull.push(p);
    }
    return hull;
}

function renderFrame() {
    const layout = badgeLayouts[currentBadge];
    if (!layout) {
        requestAnimationFrame(renderFrame);
        return;
    }

    // Recompute positions if badge changed
    if (ledPositions.length !== layout.led_count) {
        ledPositions = computeLedPositions(layout);
        computeTouchPadPositions();
    }

    // Clear canvas
    ctx.fillStyle = '#0a0a0f';
    ctx.fillRect(0, 0, canvas.width, canvas.height);

    // Draw ring/shape guides (subtle)
    ctx.strokeStyle = 'rgba(255,255,255,0.05)';
    ctx.lineWidth = 1;
    drawShapeGuide(layout);

    // Draw LEDs
    const ledRadius = Math.max(4, Math.min(10, 300 / layout.led_count));

    for (let i = 0; i < layout.led_count; i++) {
        const pos = ledPositions[i];
        if (!pos) continue;

        let r = 0, g = 0, b = 0;
        if (pixels[i]) {
            r = pixels[i][0];
            g = pixels[i][1];
            b = pixels[i][2];
        }

        const isLit = r > 0 || g > 0 || b > 0;

        // Real WS2812B/SK6812 LEDs are extremely bright even at low values.
        // Apply an aggressive perceptual boost so dim values appear much
        // closer to full brightness, matching real-life perception.
        let dr = r, dg = g, db = b;
        if (isLit) {
            dr = Math.round(Math.pow(r / 255, 0.35) * 255);
            dg = Math.round(Math.pow(g / 255, 0.35) * 255);
            db = Math.round(Math.pow(b / 255, 0.35) * 255);
        }

        // Glow effect for lit LEDs
        if (isLit) {
            const gradient = ctx.createRadialGradient(pos.x, pos.y, 0, pos.x, pos.y, ledRadius * 4);
            gradient.addColorStop(0, `rgba(${dr},${dg},${db},0.7)`);
            gradient.addColorStop(0.4, `rgba(${dr},${dg},${db},0.25)`);
            gradient.addColorStop(1, `rgba(${dr},${dg},${db},0)`);
            ctx.fillStyle = gradient;
            ctx.beginPath();
            ctx.arc(pos.x, pos.y, ledRadius * 4, 0, Math.PI * 2);
            ctx.fill();
        }

        // LED body
        ctx.beginPath();
        ctx.arc(pos.x, pos.y, ledRadius, 0, Math.PI * 2);

        if (isLit) {
            ctx.fillStyle = `rgb(${dr},${dg},${db})`;
            ctx.shadowColor = `rgb(${dr},${dg},${db})`;
            ctx.shadowBlur = 12;
        } else {
            ctx.fillStyle = 'rgba(30,30,40,0.8)';
            ctx.shadowBlur = 0;
        }
        ctx.fill();
        ctx.shadowBlur = 0;

        // LED border
        ctx.strokeStyle = isLit ? `rgba(${dr},${dg},${db},0.6)` : 'rgba(255,255,255,0.1)';
        ctx.lineWidth = 0.5;
        ctx.stroke();
    }

    // Draw touch pads on canvas
    if (touchPadPositions.length > 0) {
        const padRadius = 14;
        ctx.font = '9px monospace';
        ctx.textAlign = 'center';
        ctx.textBaseline = 'middle';

        for (const pad of touchPadPositions) {
            const btn = touchBtnElements[pad.sensorIdx];
            const isActive = btn && (btn.classList.contains('touched') ||
                btn.classList.contains('short-pressed') ||
                btn.classList.contains('long-pressed') ||
                btn.classList.contains('very-long-pressed'));

            // Pad background
            ctx.beginPath();
            ctx.arc(pad.x, pad.y, padRadius, 0, Math.PI * 2);
            if (isActive) {
                ctx.fillStyle = 'rgba(0,180,255,0.35)';
                ctx.strokeStyle = 'rgba(0,212,255,0.8)';
                ctx.lineWidth = 2;
            } else {
                ctx.fillStyle = 'rgba(40,40,60,0.5)';
                ctx.strokeStyle = 'rgba(255,255,255,0.12)';
                ctx.lineWidth = 1;
            }
            ctx.fill();
            ctx.stroke();

            // Pad label
            ctx.fillStyle = isActive ? 'rgba(255,255,255,0.9)' : 'rgba(255,255,255,0.4)';
            ctx.fillText(pad.label, pad.x, pad.y - 1);

            // Key hint below label
            const key = sensorToKey[pad.sensorIdx];
            if (key) {
                ctx.fillStyle = isActive ? 'rgba(0,212,255,0.8)' : 'rgba(0,212,255,0.3)';
                ctx.font = '7px monospace';
                ctx.fillText(`[${key}]`, pad.x, pad.y + 8);
                ctx.font = '9px monospace';
            }
        }
    }

    // Draw center badge label
    ctx.fillStyle = 'rgba(255,255,255,0.15)';
    ctx.font = '14px monospace';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    ctx.fillText(currentBadge, canvas.width / 2, canvas.height / 2);

    // Update FPS display
    const fps = frameTimestamps.length > 1
        ? Math.round(frameTimestamps.length / 2)
        : 0;
    fpsDisplay.textContent = `${fps} fps`;

    requestAnimationFrame(renderFrame);
}

// ----------------------------------------------------------------
// Canvas Touch Pad Interaction
// ----------------------------------------------------------------

// Track which pads are currently held via canvas mouse
let canvasActivePads = new Set();

canvas.addEventListener('mousedown', (e) => {
    const pad = hitTestTouchPad(e);
    if (pad === null) return;
    canvasActivePads.add(pad);
    const btn = touchBtnElements[pad];
    if (btn) onTouchStart(pad, btn, e);
});

canvas.addEventListener('mouseup', () => {
    for (const pad of canvasActivePads) {
        const btn = touchBtnElements[pad];
        if (btn) onTouchEnd(pad, btn, {});
    }
    canvasActivePads.clear();
});

canvas.addEventListener('mouseleave', () => {
    for (const pad of canvasActivePads) {
        const btn = touchBtnElements[pad];
        if (btn) onTouchEnd(pad, btn, {});
    }
    canvasActivePads.clear();
});

function hitTestTouchPad(e) {
    const rect = canvas.getBoundingClientRect();
    const scaleX = canvas.width / rect.width;
    const scaleY = canvas.height / rect.height;
    const mx = (e.clientX - rect.left) * scaleX;
    const my = (e.clientY - rect.top) * scaleY;
    const padRadius = 14;

    for (const pad of touchPadPositions) {
        const dx = mx - pad.x;
        const dy = my - pad.y;
        if (dx * dx + dy * dy <= padRadius * padRadius) {
            return pad.sensorIdx;
        }
    }
    return null;
}

// ----------------------------------------------------------------
// Badge Variant Selection
// ----------------------------------------------------------------

badgeSelect.addEventListener('change', () => {
    currentBadge = badgeSelect.value;
    ledPositions = []; // Force recompute
    pixels = [];
    buildTouchButtons();
    logEvent(`Badge variant changed to ${currentBadge}`, 'info');
});

// ----------------------------------------------------------------
// Control Panel — Section Toggle
// ----------------------------------------------------------------

function toggleSection(headerEl) {
    headerEl.classList.toggle('collapsed');
    const body = headerEl.nextElementSibling;
    body.classList.toggle('hidden');
}

// ----------------------------------------------------------------
// Control Panel — Mock Server API Helpers
// ----------------------------------------------------------------

async function mockServerPost(path, data = {}) {
    try {
        const resp = await fetch(`${MOCK_SERVER_URL}${path}`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(data),
        });
        const result = await resp.json();
        logEvent(`API ${path}: ${result.status || 'ok'}`, 'info');
        return result;
    } catch (e) {
        logEvent(`API ${path} failed: ${e.message}`, 'error');
        return null;
    }
}

async function mockServerGet(path) {
    try {
        const resp = await fetch(`${MOCK_SERVER_URL}${path}`);
        return await resp.json();
    } catch (e) {
        logEvent(`API GET ${path} failed: ${e.message}`, 'error');
        return null;
    }
}

function triggerHeartbeat() {
    if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({ type: 'inject', command: 'send_heartbeat' }));
        logEvent('Inject: send_heartbeat', 'info');
    } else {
        logEvent('Cannot inject: WebSocket not connected', 'error');
    }
}

// ----------------------------------------------------------------
// Control Panel — Game Events
// ----------------------------------------------------------------

const GAME_PRESETS = {
    join_red_5: {
        event: { event: 'QlgVrlHvkZs=', stoneColor: 1, power: 75.0, msRemaining: 300000, eventComplete: false },
    },
    join_cyan_10: {
        event: { event: 'QlgVrlHvkZs=', stoneColor: 4, power: 50.0, msRemaining: 600000, eventComplete: false },
    },
    event_complete: {
        event: { event: 'QlgVrlHvkZs=', stoneColor: 3, power: 100.0, msRemaining: 0, eventComplete: true },
    },
    clear_event: {
        event: { event: 'AAAAAAAAAAA=', stoneColor: 1, power: 0, msRemaining: 0, eventComplete: false },
    },
    unlock_stones: {
        stones: [1, 2, 3, 4, 5, 6],
    },
    unlock_songs: {
        songs: [1, 2, 3, 4, 5],
    },
};

function getGamePatchFromUI() {
    const patch = {};

    // Event data
    const eventId = document.getElementById('cpEventId').value;
    const stoneColor = parseInt(document.getElementById('cpStoneColor').value);
    const power = parseInt(document.getElementById('cpPowerLevel').value);
    const duration = parseInt(document.getElementById('cpDuration').value) || 5;

    patch.event = {
        event: eventId,
        stoneColor: stoneColor,
        power: power,
        msRemaining: duration * 60 * 1000,
    };

    // Stones
    const stoneChecks = document.querySelectorAll('#cpStonesGrid input:checked');
    if (stoneChecks.length > 0) {
        patch.stones = Array.from(stoneChecks).map(cb => parseInt(cb.value));
    }

    // Songs
    const songChecks = document.querySelectorAll('#cpSongsGrid input:checked');
    if (songChecks.length > 0) {
        patch.songs = Array.from(songChecks).map(cb => parseInt(cb.value));
    }

    return patch;
}

async function applyGameEvent(andTrigger) {
    const presetKey = document.getElementById('cpGamePreset').value;
    const patch = presetKey ? GAME_PRESETS[presetKey] : getGamePatchFromUI();
    if (!patch) return;

    await mockServerPost('/admin/config/patch', patch);
    if (andTrigger) {
        triggerHeartbeat();
    }
}

// Preset dropdown auto-apply
document.getElementById('cpGamePreset').addEventListener('change', () => {
    // Just select — user clicks Apply button
});

// Power slider display
document.getElementById('cpPowerLevel').addEventListener('input', (e) => {
    document.getElementById('cpPowerValue').textContent = `${e.target.value}%`;
});

// Apply buttons
document.getElementById('cpApplyTrigger').addEventListener('click', () => applyGameEvent(true));
document.getElementById('cpApplyWait').addEventListener('click', () => applyGameEvent(false));

// ----------------------------------------------------------------
// Control Panel — OTA
// ----------------------------------------------------------------

document.getElementById('cpOtaEnable1M').addEventListener('click', async () => {
    await mockServerPost('/admin/ota/enable', { dummy_size: 1024 * 1024 });
    updateOtaStatusUI(true);
});

document.getElementById('cpOtaEnable4M').addEventListener('click', async () => {
    await mockServerPost('/admin/ota/enable', { dummy_size: 4 * 1024 * 1024 });
    updateOtaStatusUI(true);
});

document.getElementById('cpOtaDisable').addEventListener('click', async () => {
    await mockServerPost('/admin/ota/disable');
    updateOtaStatusUI(false);
});

document.getElementById('cpOtaEnableCustom').addEventListener('click', async () => {
    const path = document.getElementById('cpOtaBinaryPath').value.trim();
    if (!path) { logEvent('OTA: No binary path specified', 'error'); return; }
    await mockServerPost('/admin/ota/enable', { binary_path: path });
    updateOtaStatusUI(true);
});

function updateOtaStatusUI(enabled) {
    const el = document.getElementById('cpOtaStatus');
    el.textContent = enabled ? 'Enabled' : 'Disabled';
    el.className = `cp-status-value ${enabled ? 'active' : 'off'}`;
}

// ----------------------------------------------------------------
// Control Panel — Peers
// ----------------------------------------------------------------

let peerList = [];

function renderPeerList() {
    const container = document.getElementById('cpPeerList');
    container.innerHTML = '';
    peerList.forEach((uuid, idx) => {
        const div = document.createElement('div');
        div.className = 'peer-entry';

        const input = document.createElement('input');
        input.type = 'text';
        input.className = 'cp-input';
        input.value = uuid;
        input.readOnly = true;

        const btn = document.createElement('button');
        btn.className = 'cp-btn danger';
        btn.style.cssText = 'width:auto;padding:4px 8px';
        btn.textContent = '\u2715';
        btn.addEventListener('click', () => removePeer(idx));

        div.appendChild(input);
        div.appendChild(btn);
        container.appendChild(div);
    });
}

function removePeer(idx) {
    peerList.splice(idx, 1);
    renderPeerList();
    syncPeers();
}

async function syncPeers() {
    await mockServerPost('/admin/peers', { siblings: peerList });
}

function generateUUID() {
    return 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, c => {
        const r = Math.random() * 16 | 0;
        return (c === 'x' ? r : (r & 0x3 | 0x8)).toString(16);
    });
}

document.getElementById('cpAddPeer').addEventListener('click', () => {
    const input = document.getElementById('cpNewPeer');
    const uuid = input.value.trim() || generateUUID();
    peerList.push(uuid);
    input.value = '';
    renderPeerList();
    syncPeers();
});

document.getElementById('cpAdd5Peers').addEventListener('click', () => {
    for (let i = 0; i < 5; i++) peerList.push(generateUUID());
    renderPeerList();
    syncPeers();
});

document.getElementById('cpClearPeers').addEventListener('click', () => {
    peerList = [];
    renderPeerList();
    syncPeers();
});

// ----------------------------------------------------------------
// Control Panel — Touch Combination Reference (Phase 4)
// ----------------------------------------------------------------

const TOUCH_COMBOS = {
    FMAN25: [
        { action: 'Enable Touch',    sensors: 'Center',          keys: '[5] hold 1s' },
        { action: 'Disable Touch',   sensors: 'L4+Ctr+R4',      keys: '[4]+[5]+[6] hold' },
        { action: 'Next Sequence',   sensors: 'Center+R1',       keys: '[5]+[9]' },
        { action: 'Prev Sequence',   sensors: 'L1+Center',       keys: '[1]+[5]' },
        { action: 'Battery Meter',   sensors: 'Center+R2',       keys: '[5]+[8]' },
        { action: 'Enable BLE',      sensors: 'Center+R3',       keys: '[5]+[7]' },
        { action: 'Disable BLE',     sensors: 'L3+Center',       keys: '[3]+[5]' },
        { action: 'Synth Mode',      sensors: 'L1+R1',           keys: '[1]+[9]' },
        { action: 'Network Test',    sensors: 'L2+Center',       keys: '[2]+[5]' },
    ],
    CREST: [
        { action: 'Enable Touch',    sensors: 'Tail',            keys: '[5] hold 1s' },
        { action: 'Disable Touch',   sensors: 'RW3+RW2+RW1',    keys: '[7]+[8]+[9] hold' },
        { action: 'Next Sequence',   sensors: 'LW1+RW1',         keys: '[1]+[9]' },
        { action: 'Prev Sequence',   sensors: 'LW2+RW1',         keys: '[2]+[9]' },
        { action: 'Battery Meter',   sensors: 'Tail+RW1',        keys: '[5]+[9]' },
        { action: 'Enable BLE',      sensors: 'LW1+Tail',        keys: '[1]+[5]' },
        { action: 'Disable BLE',     sensors: 'LW2+Tail',        keys: '[2]+[5]' },
        { action: 'Synth Mode',      sensors: 'LW4+RW4',         keys: '[4]+[6]' },
        { action: 'Network Test',    sensors: 'LW4+Tail+RW4',    keys: '[4]+[5]+[6]' },
    ],
    TRON: [
        { action: 'Battery Meter',   sensors: '8+11 o\'clock',   keys: '[3]+[9]' },
        { action: 'Enable BLE',      sensors: '12+8 o\'clock',   keys: '[1]+[3]' },
        { action: 'Disable BLE',     sensors: '12+11 o\'clock',  keys: '[1]+[9]' },
        { action: 'Next Sequence',   sensors: '2+7 o\'clock',    keys: '[7]+[4]' },
    ],
    REACTOR: [
        { action: 'Enable Touch',    sensors: '2+4+8+10',        keys: '[7]+[6]+[3]+[2] hold' },
        { action: 'Battery Meter',   sensors: '1+11 o\'clock',   keys: '[8]+[9]' },
        { action: 'Next Sequence',   sensors: '2+10 o\'clock',   keys: '[7]+[2]' },
        { action: 'Prev Sequence',   sensors: '4+10 o\'clock',   keys: '[6]+[2]' },
        { action: 'Enable BLE',      sensors: '2+8 o\'clock',    keys: '[7]+[3]' },
        { action: 'Disable BLE',     sensors: '4+8 o\'clock',    keys: '[6]+[3]' },
        { action: 'Synth Mode',      sensors: '4+5+7+8',         keys: '[6]+[5]+[4]+[3]' },
        { action: 'Network Test',    sensors: '5+7 o\'clock',    keys: '[5]+[4]' },
    ],
};

function parseComboKeys(keysStr) {
    // Extract key numbers from strings like "[5]+[9]" or "[4]+[5]+[6] hold"
    const matches = keysStr.match(/\[(\d)\]/g);
    if (!matches) return [];
    return matches.map(m => m.charAt(1)); // ['5', '9']
}

function buildTouchComboTable() {
    const tbody = document.getElementById('cpComboTableBody');
    tbody.innerHTML = '';
    const combos = TOUCH_COMBOS[currentBadge] || [];
    for (const combo of combos) {
        const tr = document.createElement('tr');
        tr.innerHTML = `<td>${combo.action}</td><td>${combo.sensors}</td><td class="keys-col">${combo.keys}</td>`;
        tr._requiredKeys = parseComboKeys(combo.keys);
        tbody.appendChild(tr);
    }
}

function updateComboHighlights() {
    const tbody = document.getElementById('cpComboTableBody');
    if (!tbody) return;
    for (const tr of tbody.children) {
        const required = tr._requiredKeys;
        if (!required || required.length === 0) {
            tr.classList.remove('active-combo');
            continue;
        }
        const allHeld = required.every(k => activeKeys.has(k));
        tr.classList.toggle('active-combo', allHeld);
    }
}

// ----------------------------------------------------------------
// Control Panel — Server State Polling (Phase 5)
// ----------------------------------------------------------------

let serverStatePollTimer = null;

async function refreshServerState() {
    const state = await mockServerGet('/admin/state');
    if (!state) return;

    document.getElementById('cpBadgesOnline').textContent = state.badges_registered || 0;

    const hbEl = document.getElementById('cpLastHeartbeat');
    if (state.last_heartbeat_time) {
        const ago = Math.round((Date.now() / 1000) - state.last_heartbeat_time);
        hbEl.textContent = `${ago}s ago`;
        hbEl.className = 'cp-status-value';
    } else {
        hbEl.textContent = 'Never';
        hbEl.className = 'cp-status-value off';
    }

    const otaEl = document.getElementById('cpServerOta');
    otaEl.textContent = state.ota_enabled ? 'Enabled' : 'Disabled';
    otaEl.className = `cp-status-value ${state.ota_enabled ? 'active' : 'off'}`;
    updateOtaStatusUI(state.ota_enabled);

    const eventEl = document.getElementById('cpServerEvent');
    const tmpl = state.response_template || {};
    if (tmpl.event && tmpl.event.event && tmpl.event.event !== 'AAAAAAAAAAA=') {
        eventEl.textContent = `${tmpl.event.event.substring(0, 8)}... (color=${tmpl.event.stoneColor})`;
        eventEl.className = 'cp-status-value';
    } else {
        eventEl.textContent = 'None';
        eventEl.className = 'cp-status-value off';
    }
}

function startServerStatePolling() {
    if (serverStatePollTimer) clearInterval(serverStatePollTimer);
    serverStatePollTimer = setInterval(refreshServerState, 5000);
    refreshServerState();
}

document.getElementById('cpRefreshState').addEventListener('click', refreshServerState);

// ----------------------------------------------------------------
// Badge Variant Change — rebuild touch combos
// ----------------------------------------------------------------

badgeSelect.addEventListener('change', () => {
    buildTouchComboTable();
});

// ----------------------------------------------------------------
// Initialization
// ----------------------------------------------------------------

async function init() {
    await loadBadgeLayouts();

    // Auto-select badge from URL query param (e.g. ?badge=TRON)
    const urlBadge = new URLSearchParams(window.location.search).get('badge');
    if (urlBadge && badgeLayouts[urlBadge.toUpperCase()]) {
        currentBadge = urlBadge.toUpperCase();
        badgeSelect.value = currentBadge;
    }

    buildTouchButtons();
    buildTouchComboTable();
    connectWebSocket();
    requestAnimationFrame(renderFrame);
    startServerStatePolling();
    logEvent('Visualizer initialized', 'info');
}

init();
