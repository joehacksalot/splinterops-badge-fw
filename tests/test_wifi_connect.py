"""
WiFi Connect / Disconnect Lifecycle Tests (QEMU OpenCores Ethernet)

Validates:
  - WifiClient_QemuEth.c state machine and event handling
  - NOTIFICATION_EVENTS_NETWORK_TEST_COMPLETE
"""
import pytest


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
