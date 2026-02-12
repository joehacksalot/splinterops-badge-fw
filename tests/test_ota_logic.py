"""
OTA Update Logic Tests (QEMU OpenCores Ethernet)

Validates:
  - OtaUpdate.c version comparison
  - Download progress reporting
  - Skip when version matches
"""
import pytest


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
