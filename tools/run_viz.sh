#!/bin/bash
# tools/run_viz.sh — One-command launch for QEMU + LED/Touch visualization
#
# Usage:
#   ./tools/run_viz.sh [--no-build] [--badge FMAN25|CREST|TRON|REACTOR]
#
# Prerequisites:
#   - ESP-IDF environment (run `get_idf` first)
#   - Python 3 with bumble + websockets: pip install -r requirements-test.txt
#   - QEMU for ESP32: qemu-system-xtensa

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# Defaults
DO_BUILD=true
BADGE_TYPE=""
QEMU_VIZ_PORT=1235
QEMU_HCI_PORT=1234
QEMU_CONSOLE_PORT=1236
WS_PORT=8765
HTTP_PORT=8080
MOCK_SERVER_PORT=9080

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --no-build)
            DO_BUILD=false
            shift
            ;;
        --badge)
            BADGE_TYPE="$2"
            shift 2
            ;;
        --viz-port)
            QEMU_VIZ_PORT="$2"
            shift 2
            ;;
        --ws-port)
            WS_PORT="$2"
            shift 2
            ;;
        --http-port)
            HTTP_PORT="$2"
            shift 2
            ;;
        -h|--help)
            echo "Usage: $0 [--no-build] [--badge FMAN25|CREST|TRON|REACTOR]"
            echo "       [--viz-port PORT] [--ws-port PORT] [--http-port PORT]"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Prompt for badge type if not provided
if [ -z "$BADGE_TYPE" ]; then
    echo "Select badge type (FMAN25, CREST, TRON, REACTOR) [FMAN25]: "
    read -r BADGE_TYPE
    BADGE_TYPE=${BADGE_TYPE:-FMAN25}
fi

case "$BADGE_TYPE" in
    FMAN25|CREST|TRON|REACTOR)
        ;;
    *)
        echo "Error: Unknown badge type '$BADGE_TYPE'. Use FMAN25, CREST, TRON, or REACTOR."
        exit 1
        ;;
esac

# Pre-flight: check required Python packages
for pkg in bumble websockets; do
    if ! python3 -c "import $pkg" 2>/dev/null; then
        echo "Error: Python package '$pkg' is not installed."
        echo "Install with: pip install -r requirements-test.txt"
        exit 1
    fi
done

echo "=== SplinterOps Badge Visualizer ==="
echo "Badge type: $BADGE_TYPE"
echo "QEMU viz port: $QEMU_VIZ_PORT"
echo "WebSocket port: $WS_PORT"
echo "HTTP UI port: $HTTP_PORT"
echo ""

# Kill any stale processes on ports we need
for port in $QEMU_VIZ_PORT $QEMU_HCI_PORT $QEMU_CONSOLE_PORT $MOCK_SERVER_PORT $WS_PORT $HTTP_PORT; do
    pid=$(lsof -ti ":$port" 2>/dev/null || true)
    if [ -n "$pid" ]; then
        echo "Killing stale process on port $port (PID $pid)"
        kill $pid 2>/dev/null || true
    fi
done
sleep 0.5

# Track PIDs for cleanup
PIDS=()
cleanup() {
    echo ""
    echo "=== Shutting down ==="
    for pid in "${PIDS[@]+"${PIDS[@]}"}"; do
        if kill -0 "$pid" 2>/dev/null; then
            echo "Killing PID $pid"
            kill "$pid" 2>/dev/null || true
        fi
    done
    wait 2>/dev/null || true
    echo "Done."
}
trap cleanup EXIT INT TERM

# 1. Build firmware with QEMU mode (if requested)
if [ "$DO_BUILD" = true ]; then
    echo "=== Building firmware (QEMU mode, badge=$BADGE_TYPE) ==="
    cd "$PROJECT_DIR"

    # Generate a temporary sdkconfig fragment for the selected badge type
    BADGE_FRAGMENT="$PROJECT_DIR/build/sdkconfig.badge"
    mkdir -p "$PROJECT_DIR/build"
    cat > "$BADGE_FRAGMENT" <<EOF
CONFIG_BADGE_TYPE_TRON=$([ "$BADGE_TYPE" = "TRON" ] && echo "y" || echo "n")
CONFIG_BADGE_TYPE_REACTOR=$([ "$BADGE_TYPE" = "REACTOR" ] && echo "y" || echo "n")
CONFIG_BADGE_TYPE_CREST=$([ "$BADGE_TYPE" = "CREST" ] && echo "y" || echo "n")
CONFIG_BADGE_TYPE_FMAN25=$([ "$BADGE_TYPE" = "FMAN25" ] && echo "y" || echo "n")
EOF
    echo "Badge config fragment:"
    cat "$BADGE_FRAGMENT"

    # Clean build required when switching modes or badge type
    if [ ! -f build/build.ninja ]; then
        idf.py -DSDKCONFIG_DEFAULTS="sdkconfig.old;sdkconfig.ci.qemu;build/sdkconfig.badge" set-target esp32
    else
        idf.py -DSDKCONFIG_DEFAULTS="sdkconfig.old;sdkconfig.ci.qemu;build/sdkconfig.badge" reconfigure
    fi
    idf.py build
    echo ""
fi

# 2. Create merged flash image for QEMU
echo "=== Preparing QEMU flash image ==="
cd "$PROJECT_DIR/build"
if [ -f flash_args ]; then
    esptool.py --chip esp32 merge_bin --output flash_image.bin @flash_args || \
    python -m esptool --chip esp32 merge_bin --output flash_image.bin @flash_args || \
    { echo "Error: Could not create merged flash image. Ensure esptool is available."; exit 1; }
    # QEMU requires flash image to be exactly a power-of-2 MB size
    FLASH_SIZE=$((16 * 1024 * 1024))
    truncate -s $FLASH_SIZE flash_image.bin || \
    dd if=/dev/null of=flash_image.bin bs=1 count=0 seek=$FLASH_SIZE 2>/dev/null
    echo "Flash image padded to 16 MB for QEMU"
fi
cd "$PROJECT_DIR"

# 3. Start mock game server for WiFi emulation
echo "=== Starting mock game server on port $MOCK_SERVER_PORT ==="
python3 "$SCRIPT_DIR/mock_game_server.py" --port "$MOCK_SERVER_PORT" &
MOCK_PID=$!
PIDS+=($MOCK_PID)
echo "Mock game server started (PID $MOCK_PID)"

# 4. Start Bumble virtual BLE controller so it's ready when QEMU connects
echo "=== Starting Bumble virtual BLE controller on port $QEMU_HCI_PORT ==="
python3 "$SCRIPT_DIR/ble_virtual_controller.py" --port "$QEMU_HCI_PORT" &
BUMBLE_PID=$!
PIDS+=($BUMBLE_PID)
echo "Bumble virtual controller started (PID $BUMBLE_PID)"
# Wait for Bumble to open its TCP listener
for i in $(seq 1 10); do
    if lsof -ti :"$QEMU_HCI_PORT" >/dev/null 2>&1; then
        echo "Bumble listening on port $QEMU_HCI_PORT"
        break
    fi
    if ! kill -0 "$BUMBLE_PID" 2>/dev/null; then
        echo "Error: Bumble virtual controller exited unexpectedly."
        exit 1
    fi
    sleep 0.5
done

# 5. Start visualization bridge so it's listening when QEMU starts
echo "=== Starting visualization bridge ==="
python3 "$SCRIPT_DIR/viz_bridge.py" \
    --qemu-port "$QEMU_VIZ_PORT" \
    --console-port "$QEMU_CONSOLE_PORT" \
    --ws-port "$WS_PORT" \
    --http-port "$HTTP_PORT" \
    --badge "$BADGE_TYPE" &
BRIDGE_PID=$!
PIDS+=($BRIDGE_PID)
echo "Viz bridge started (PID $BRIDGE_PID)"

# Give the bridge a moment to open its TCP listener for console logs
sleep 1

# 6. Start QEMU with all serial ports + OpenCores Ethernet
echo "=== Starting QEMU ==="
ESPRESSIF_QEMU="${HOME}/.espressif/tools/qemu-xtensa/esp_develop_9.0.0_20240606/qemu/bin/qemu-system-xtensa"
if [ ! -x "$ESPRESSIF_QEMU" ]; then
    echo "Error: Espressif QEMU fork not found at $ESPRESSIF_QEMU"
    echo "Install it via: python $IDF_PATH/tools/idf_tools.py install qemu-xtensa"
    exit 1
fi
QEMU_CMD="$ESPRESSIF_QEMU \
    -nographic \
    -machine esp32 \
    -m 4M \
    -drive file=build/flash_image.bin,if=mtd,format=raw \
    -serial tcp:localhost:${QEMU_CONSOLE_PORT} \
    -serial tcp:localhost:${QEMU_HCI_PORT},nodelay \
    -serial tcp:localhost:${QEMU_VIZ_PORT},server,nowait \
    -nic user,model=open_eth"

echo "QEMU command: $QEMU_CMD"
$QEMU_CMD &
QEMU_PID=$!
PIDS+=($QEMU_PID)
echo "QEMU started (PID $QEMU_PID)"

sleep 1

# 7. Open browser UI
echo ""
echo "=== Visualization UI available at: http://localhost:${HTTP_PORT} ==="
echo ""

# Try to open browser (macOS)
if command -v open &>/dev/null; then
    open "http://localhost:${HTTP_PORT}" 2>/dev/null || true
elif command -v xdg-open &>/dev/null; then
    xdg-open "http://localhost:${HTTP_PORT}" 2>/dev/null || true
fi

echo "Press Ctrl+C to stop all services."
echo ""

# Wait for QEMU (the primary process)
wait $QEMU_PID 2>/dev/null || true
