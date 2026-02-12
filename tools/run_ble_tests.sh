#!/bin/bash
# tools/run_ble_tests.sh
#
# End-to-end BLE test runner for QEMU.
#
# This script:
#   1. Builds firmware with QEMU+BLE mode
#   2. Starts the Bumble virtual controller in the background
#   3. Starts QEMU with HCI serial port mapped to TCP
#   4. Runs the BLE pytest suite
#   5. Cleans up all background processes
#
# Prerequisites:
#   - ESP-IDF environment sourced (run `get_idf` first)
#   - Python with bumble installed: pip install bumble
#   - pytest-embedded installed: pip install -r requirements-test.txt
#
# Usage:
#   ./tools/run_ble_tests.sh
#   ./tools/run_ble_tests.sh --with-peer   # Also start a simulated peer badge

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
HCI_PORT=1234
BUMBLE_PID=""
QEMU_PID=""

cleanup() {
    echo "Cleaning up..."
    [ -n "$BUMBLE_PID" ] && kill "$BUMBLE_PID" 2>/dev/null || true
    [ -n "$QEMU_PID" ]   && kill "$QEMU_PID"   2>/dev/null || true
    wait 2>/dev/null || true
    echo "Done."
}
trap cleanup EXIT

# Parse arguments
WITH_PEER=""
for arg in "$@"; do
    case "$arg" in
        --with-peer) WITH_PEER="--with-peer" ;;
        *) echo "Unknown argument: $arg"; exit 1 ;;
    esac
done

echo "=== Step 1: Build firmware with QEMU+BLE mode ==="
cd "$PROJECT_DIR"
rm -rf build sdkconfig
idf.py -DSDKCONFIG_DEFAULTS="sdkconfig;sdkconfig.ci.qemu" build

echo ""
echo "=== Step 2: Start Bumble virtual controller (port $HCI_PORT) ==="
python "${SCRIPT_DIR}/ble_virtual_controller.py" --port "$HCI_PORT" $WITH_PEER &
BUMBLE_PID=$!
echo "Bumble PID: $BUMBLE_PID"
sleep 2  # Give Bumble time to start listening

echo ""
echo "=== Step 3: Start QEMU with HCI serial port ==="
# QEMU's second serial port (UART1) is mapped to TCP for HCI transport.
# The first -serial is UART0 (console), the second is UART1 (HCI).
idf.py qemu \
    --qemu-extra-args="-serial tcp:localhost:${HCI_PORT},nodelay" \
    monitor &
QEMU_PID=$!
echo "QEMU PID: $QEMU_PID"
sleep 5  # Give QEMU time to boot

echo ""
echo "=== Step 4: Run BLE tests ==="
cd "${PROJECT_DIR}/tests"
pytest \
    --target esp32 \
    --embedded-services idf,qemu \
    -m "ble" \
    -v \
    test_ble_iwc.py \
    test_ble_file_transfer.py \
    test_ble_interactive_game.py \
    test_ble_service_lifecycle.py
TEST_EXIT=$?

echo ""
echo "=== Step 5: Cleanup ==="
# cleanup is handled by the trap

exit $TEST_EXIT
