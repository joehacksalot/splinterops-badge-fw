# tests/test_ble_interactive_game.py
"""
BLE interactive game tests.

Validates:
  - GATT write → _BleControl_BleReceiveInteractiveGameDataAction()
  - NotificationDispatcher INTERACTIVE_GAME_ACTION events
  - GATT read of touch sensor state
"""
import pytest
import struct


@pytest.mark.esp32
@pytest.mark.qemu
@pytest.mark.ble
def test_interactive_game_write(dut, bumble_link):
    """Verify interactive game BLE command triggers notification."""
    import asyncio
    from tools.ble_test_peers import (
        create_gatt_client,
        INTERACTIVE_GAME_CHR_UUID,
    )
    from bumble.device import Peer

    async def _run():
        client_device = await create_gatt_client(bumble_link, name="GameClient")

        target = await client_device.find_device_by_name("IWCv4")
        if target is None:
            await asyncio.sleep(3)
            target = await client_device.find_device_by_name("IWCv4")

        assert target is not None, "Could not find badge device"
        connection = await client_device.connect(target.address)
        peer = Peer(connection)
        await peer.discover_services()

        game_chars = peer.get_characteristics_by_uuid(INTERACTIVE_GAME_CHR_UUID)
        assert len(game_chars) > 0, "Interactive game characteristic not found"
        game_char = game_chars[0]

        # Write interactive game command: light feathers 0-3 (bits 0-3 set)
        game_data = struct.pack("<H", 0x000F)
        await peer.write_value(game_char, game_data)

    asyncio.get_event_loop().run_until_complete(_run())

    dut.expect("NOTIFICATION_EVENTS_INTERACTIVE_GAME_ACTION", timeout=10)


@pytest.mark.esp32
@pytest.mark.qemu
@pytest.mark.ble
def test_interactive_game_read(dut, bumble_link):
    """Verify interactive game GATT read returns touch sensor state."""
    import asyncio
    from tools.ble_test_peers import (
        create_gatt_client,
        INTERACTIVE_GAME_CHR_UUID,
    )
    from bumble.device import Peer

    read_result = {}

    async def _run():
        client_device = await create_gatt_client(bumble_link, name="GameReader")

        target = await client_device.find_device_by_name("IWCv4")
        if target is None:
            await asyncio.sleep(3)
            target = await client_device.find_device_by_name("IWCv4")

        assert target is not None, "Could not find badge device"
        connection = await client_device.connect(target.address)
        peer = Peer(connection)
        await peer.discover_services()

        game_chars = peer.get_characteristics_by_uuid(INTERACTIVE_GAME_CHR_UUID)
        assert len(game_chars) > 0, "Interactive game characteristic not found"
        game_char = game_chars[0]

        # Read the interactive game characteristic
        value = await peer.read_value(game_char)
        read_result["value"] = value

    asyncio.get_event_loop().run_until_complete(_run())

    # Verify we got a response (InteractiveGameData is uint16_t = 2 bytes)
    assert "value" in read_result
    assert len(read_result["value"]) == 2


@pytest.mark.esp32
@pytest.mark.qemu
@pytest.mark.ble
def test_interactive_game_invalid_size(dut, bumble_link):
    """Verify badge rejects interactive game data with wrong size."""
    import asyncio
    from tools.ble_test_peers import (
        create_gatt_client,
        INTERACTIVE_GAME_CHR_UUID,
    )
    from bumble.device import Peer

    async def _run():
        client_device = await create_gatt_client(bumble_link, name="BadGameClient")

        target = await client_device.find_device_by_name("IWCv4")
        if target is None:
            await asyncio.sleep(3)
            target = await client_device.find_device_by_name("IWCv4")

        assert target is not None, "Could not find badge device"
        connection = await client_device.connect(target.address)
        peer = Peer(connection)
        await peer.discover_services()

        game_chars = peer.get_characteristics_by_uuid(INTERACTIVE_GAME_CHR_UUID)
        assert len(game_chars) > 0, "Interactive game characteristic not found"
        game_char = game_chars[0]

        # Write wrong-sized data (3 bytes instead of 2)
        bad_data = bytes([0x01, 0x02, 0x03])
        await peer.write_value(game_char, bad_data)

    asyncio.get_event_loop().run_until_complete(_run())

    dut.expect("InteractiveGameDataAction Invalid size", timeout=10)
