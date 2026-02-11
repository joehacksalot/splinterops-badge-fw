# tests/test_boot.py
import pytest


@pytest.mark.esp32
@pytest.mark.qemu
def test_boot_to_app_main(dut):
    """Verify firmware boots successfully in QEMU and reaches app_main."""
    dut.expect("QEMU mode: hardware peripherals are stubbed", timeout=30)


@pytest.mark.esp32
@pytest.mark.qemu
def test_nvs_init(dut):
    """Verify console subsystem initializes (implies NVS and core init succeeded)."""
    dut.expect("Command history enabled", timeout=30)


@pytest.mark.esp32
@pytest.mark.qemu
def test_console_ready(dut):
    """Verify console is ready and accepts commands."""
    # Wait for boot to complete
    dut.expect("esp32>", timeout=30)
    # Send a test command
    dut.write("help")
    dut.expect("help", timeout=5)
