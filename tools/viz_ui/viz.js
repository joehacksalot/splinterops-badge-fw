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
const TOUCH_EVENT = {
    RELEASED: 0,
    TOUCHED: 1,
    SHORT_PRESSED: 2,
    LONG_PRESSED: 3,
    VERY_LONG_PRESSED: 4,
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
        btn.innerHTML = `<span class="label">${layout.touch_labels[i]}</span><span class="index">Sensor ${i}</span>${keyHint}`;

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
    const btn = touchBtnElements[sensorIdx];
    if (btn) onTouchStart(sensorIdx, btn, e);
});

document.addEventListener('keyup', (e) => {
    const sensorIdx = keyToSensor[e.key];
    if (sensorIdx === undefined) return;
    if (!activeKeys.has(e.key)) return;

    activeKeys.delete(e.key);
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
    if (currentBadge === 'FMAN25') {
        return computeFman25Positions(layout);
    }

    const positions = [];
    const cx = canvas.width / 2;
    const cy = canvas.height / 2;

    const innerCount = layout.inner_ring.count;
    const outerCount = layout.outer_ring.count;
    const innerOffset = layout.inner_ring.offset;
    const outerOffset = layout.outer_ring.offset;

    // Compute ring radii based on canvas size
    const maxRadius = Math.min(cx, cy) - 30;
    const outerRadius = maxRadius * 0.85;
    const innerRadius = maxRadius * 0.45;

    // Inner ring LEDs
    for (let i = 0; i < innerCount; i++) {
        const angle = (2 * Math.PI * i / innerCount) - Math.PI / 2;
        positions[innerOffset + i] = {
            x: cx + innerRadius * Math.cos(angle),
            y: cy + innerRadius * Math.sin(angle),
            ring: 'inner',
        };
    }

    // Outer ring LEDs
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

/**
 * Compute LED positions for the FMAN25 badge using exact PCB pick-and-place
 * coordinates from PCBWay_positions.csv.
 *
 * Outer ring (indices 0-31, D1-D32): SK6812SIDE LEDs around the triangle perimeter.
 * Inner ring (indices 32-44, D33-D45): SK6812MINI LEDs around the central IC.
 */
function computeFman25Positions(layout) {
    const positions = [];

    // Exact PCB coordinates from PCBWay_positions.csv (pos_x, pos_y).
    // PCB Y-axis is negative (increases downward on screen).
    // Indexed by diode number: pcbCoords[0] = D1, pcbCoords[44] = D45.
    const pcbCoords = [
        // D1-D12: outer ring (left edge + bottom vertex)
        [118.641587, -33.867092],  // D1
        [122.622622, -41.027948],  // D2
        [126.220880, -48.244993],  // D3
        [129.819145, -55.462037],  // D4
        [133.417411, -62.679090],  // D5
        [137.015677, -69.896141],  // D6
        [140.613938, -77.113188],  // D7
        [144.212199, -84.330237],  // D8
        [147.810467, -91.547289],  // D9
        [151.408726, -98.764341],  // D10
        [155.006996, -105.981384], // D11
        [157.641377, -112.267203], // D12
        // D13-D23: outer ring (right edge + top-right corner)
        [160.316706, -105.935978], // D13
        [163.863324, -98.822609],  // D14
        [167.409942, -91.709246],  // D15
        [170.956561, -84.595877],  // D16
        [174.503177, -77.482516],  // D17
        [178.049799, -70.369148],  // D18
        [181.596415, -63.255783],  // D19
        [185.143027, -56.142414],  // D20
        [188.689649, -49.029053],  // D21
        [192.236191, -41.915629],  // D22
        [196.641590, -33.867305],  // D23
        // D24-D32: outer ring (top edge, right-to-left)
        [190.641591, -34.167288],  // D24
        [182.391577, -34.167261],  // D25
        [174.141571, -34.167243],  // D26
        [165.891558, -34.167218],  // D27
        [157.641545, -34.167193],  // D28
        [149.391534, -34.167176],  // D29
        [141.141549, -34.167107],  // D30
        [132.891577, -34.167085],  // D31
        [124.641588, -34.167111],  // D32
        // D33-D45: inner ring (SK6812MINI)
        [135.641559, -43.967135],  // D33
        [141.141563, -54.060939],  // D34
        [146.641523, -64.154691],  // D35
        [152.141489, -74.248440],  // D36
        [157.641452, -84.342201],  // D37
        [163.141476, -74.248471],  // D38
        [168.641491, -64.154749],  // D39
        [174.141510, -54.061023],  // D40
        [179.641563, -43.967258],  // D41
        [168.641558, -43.967227],  // D42
        [157.641561, -43.967195],  // D43
        [146.641563, -43.967166],  // D44
        [157.641537, -54.060985],  // D45
    ];

    // Find bounding box of all LED coordinates
    let minX = Infinity, maxX = -Infinity, minY = Infinity, maxY = -Infinity;
    for (const [px, py] of pcbCoords) {
        if (px < minX) minX = px;
        if (px > maxX) maxX = px;
        if (py < minY) minY = py;
        if (py > maxY) maxY = py;
    }

    const pcbW = maxX - minX;
    const pcbH = maxY - minY; // Note: py values are negative, so this is positive range

    // Map PCB coords to canvas with padding
    const margin = 50;
    const availW = canvas.width - margin * 2;
    const availH = canvas.height - margin * 2;
    const scale = Math.min(availW / pcbW, availH / pcbH);

    // Center the layout on the canvas
    const offsetX = (canvas.width - pcbW * scale) / 2;
    const offsetY = (canvas.height - pcbH * scale) / 2;

    for (let i = 0; i < pcbCoords.length; i++) {
        const [px, py] = pcbCoords[i];
        // Map PCB coords to canvas: X maps directly, Y is negated (PCB Y is negative-down)
        const screenX = (px - minX) * scale + offsetX;
        const screenY = (-py - (-maxY)) * scale + offsetY; // flip Y: most-negative → top
        const ring = i < 32 ? 'outer' : 'inner';
        positions[i] = { x: screenX, y: screenY, ring };
    }

    return positions;
}

let ledPositions = [];
let touchPadPositions = []; // Computed touch pad positions on the canvas

// FMAN25 touch sensor → LED index mapping (from firmware touchMap).
// Each sensor maps to a cluster of outer-ring LEDs.
const FMAN25_TOUCH_LED_MAP = [
    [21, 22, 23], // sensor 0 (R1) — top-right corner
    [18, 19, 20], // sensor 1 (R2) — right edge upper
    [15, 16, 17], // sensor 2 (R3) — right edge mid
    [13, 14, 15], // sensor 3 (R4) — right edge lower
    [10, 11, 12], // sensor 4 (Center) — bottom vertex
    [7, 8, 9],    // sensor 5 (L4) — left edge lower
    [5, 6, 7],    // sensor 6 (L3) — left edge mid
    [2, 3, 4],    // sensor 7 (L2) — left edge upper
    [0, 1, 31],   // sensor 8 (L1) — top-left corner
];

/**
 * Compute touch pad positions from LED positions.
 * Each pad is placed at the centroid of its mapped LED cluster,
 * pushed slightly outward from the triangle center.
 */
function computeTouchPadPositions() {
    if (currentBadge !== 'FMAN25' || ledPositions.length < 45) {
        touchPadPositions = [];
        return;
    }

    const layout = badgeLayouts[currentBadge];
    if (!layout) return;

    // Triangle centroid (average of corner LEDs)
    const ctrX = (ledPositions[0].x + ledPositions[11].x + ledPositions[22].x) / 3;
    const ctrY = (ledPositions[0].y + ledPositions[11].y + ledPositions[22].y) / 3;

    touchPadPositions = [];
    for (let s = 0; s < FMAN25_TOUCH_LED_MAP.length; s++) {
        const ledIdxs = FMAN25_TOUCH_LED_MAP[s];
        // Centroid of mapped LEDs
        let sx = 0, sy = 0;
        for (const idx of ledIdxs) {
            sx += ledPositions[idx].x;
            sy += ledPositions[idx].y;
        }
        sx /= ledIdxs.length;
        sy /= ledIdxs.length;

        // Push outward from triangle center by a fixed amount
        const dx = sx - ctrX;
        const dy = sy - ctrY;
        const dist = Math.sqrt(dx * dx + dy * dy);
        const pushOut = 18;
        touchPadPositions[s] = {
            x: sx + (dx / dist) * pushOut,
            y: sy + (dy / dist) * pushOut,
            label: layout.touch_labels[s],
            sensorIdx: s,
        };
    }
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
    const cx = canvas.width / 2;
    const cy = canvas.height / 2;
    const maxRadius = Math.min(cx, cy) - 30;

    ctx.strokeStyle = 'rgba(255,255,255,0.05)';
    ctx.lineWidth = 1;

    if (currentBadge === 'FMAN25' && ledPositions.length >= 45) {
        // Draw outer triangle guide using corner LED positions:
        // D1 (idx 0) = top-left, D12 (idx 11) = bottom, D23 (idx 22) = top-right
        const pTL = ledPositions[0];
        const pB  = ledPositions[11];
        const pTR = ledPositions[22];

        ctx.beginPath();
        ctx.moveTo(pTL.x, pTL.y);
        ctx.lineTo(pTR.x, pTR.y);
        ctx.lineTo(pB.x, pB.y);
        ctx.closePath();
        ctx.stroke();

        // Draw inner triangle guide using corner inner LEDs:
        // D33 (idx 32) = inner top-left, D37 (idx 36) = inner bottom, D41 (idx 40) = inner top-right
        const iTL = ledPositions[32];
        const iB  = ledPositions[36];
        const iTR = ledPositions[40];

        ctx.beginPath();
        ctx.moveTo(iTL.x, iTL.y);
        ctx.lineTo(iTR.x, iTR.y);
        ctx.lineTo(iB.x, iB.y);
        ctx.closePath();
        ctx.stroke();
    } else if (currentBadge !== 'FMAN25') {
        ctx.beginPath();
        ctx.arc(cx, cy, maxRadius * 0.85, 0, Math.PI * 2);
        ctx.stroke();
        ctx.beginPath();
        ctx.arc(cx, cy, maxRadius * 0.45, 0, Math.PI * 2);
        ctx.stroke();
    }

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
    ctx.fillText(currentBadge, cx, cy);

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
// Initialization
// ----------------------------------------------------------------

async function init() {
    await loadBadgeLayouts();
    buildTouchButtons();
    connectWebSocket();
    requestAnimationFrame(renderFrame);
    logEvent('Visualizer initialized', 'info');
}

init();
