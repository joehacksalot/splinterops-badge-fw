# tests/test_ble_file_transfer.py
"""
BLE file transfer tests.

Validates:
  - GATT write → frame reassembly → JSON parse → NotificationDispatcher event
  - Config frame processing
  - Data frame reassembly
  - Settings update via BLE
  - LED sequence upload via BLE
"""
import pytest
import json
import struct


@pytest.mark.esp32
@pytest.mark.qemu
@pytest.mark.ble
def test_settings_file_transfer(dut, bumble_link):
    """Verify settings update via BLE file transfer."""
    import asyncio
    from tools.ble_test_peers import (
        create_gatt_client,
        build_config_frame,
        split_into_data_frames,
        FILE_TRANSFER_CHR_UUID,
    )

    settings_json = json.dumps({
        "soundEnabled": True,
        "vibrationEnabled": False,
    })

    frame_len = 200
    data_frames = split_into_data_frames(settings_json.encode("utf-8"), frame_len)
    num_data_frames = len(data_frames)

    from bumble.device import Peer

    async def _run():
        client_device = await create_gatt_client(bumble_link, name="SettingsClient")

        # Scan for the badge and connect
        target = await client_device.find_device_by_name("IWCv4")
        if target is None:
            # Wait for badge to start advertising
            await asyncio.sleep(3)
            target = await client_device.find_device_by_name("IWCv4")

        assert target is not None, "Could not find badge device"
        connection = await client_device.connect(target.address)
        peer = Peer(connection)
        await peer.discover_services()

        # Find the file transfer characteristic
        ft_chars = peer.get_characteristics_by_uuid(FILE_TRANSFER_CHR_UUID)
        assert len(ft_chars) > 0, "File transfer characteristic not found"
        ft_char = ft_chars[0]

        # Write config frame (frame 0)
        config = build_config_frame(
            num_frames=num_data_frames,
            frame_len=frame_len,
            file_type=2,  # FILE_TYPE_SETTINGS_FILE
        )
        await peer.write_value(ft_char, config)

        # Write data frames
        for frame in data_frames:
            await peer.write_value(ft_char, frame)

    asyncio.get_event_loop().run_until_complete(_run())

    # Verify badge processed the settings
    dut.expect("Updating settings", timeout=10)
    dut.expect("NOTIFICATION_EVENTS_BLE_FILE_SETTINGS_RECVD", timeout=5)


@pytest.mark.esp32
@pytest.mark.qemu
@pytest.mark.ble
def test_led_sequence_file_transfer(dut, bumble_link):
    """Verify LED sequence upload via BLE file transfer."""
    import asyncio
    from tools.ble_test_peers import (
        create_gatt_client,
        build_config_frame,
        split_into_data_frames,
        FILE_TRANSFER_CHR_UUID,
    )

    led_json = json.dumps({
        "name": "test_sequence",
        "frames": [
            {"r": 255, "g": 0, "b": 0, "duration": 100},
            {"r": 0, "g": 255, "b": 0, "duration": 100},
        ],
    })

    frame_len = 200
    data_frames = split_into_data_frames(led_json.encode("utf-8"), frame_len)
    num_data_frames = len(data_frames)

    from bumble.device import Peer

    async def _run():
        client_device = await create_gatt_client(bumble_link, name="LEDClient")

        target = await client_device.find_device_by_name("IWCv4")
        if target is None:
            await asyncio.sleep(3)
            target = await client_device.find_device_by_name("IWCv4")

        assert target is not None, "Could not find badge device"
        connection = await client_device.connect(target.address)
        peer = Peer(connection)
        await peer.discover_services()

        ft_chars = peer.get_characteristics_by_uuid(FILE_TRANSFER_CHR_UUID)
        assert len(ft_chars) > 0, "File transfer characteristic not found"
        ft_char = ft_chars[0]

        # Write config frame (frame 0)
        config = build_config_frame(
            num_frames=num_data_frames,
            frame_len=frame_len,
            file_type=1,  # FILE_TYPE_LED_SEQUENCE
        )
        await peer.write_value(ft_char, config)

        # Write data frames
        for frame in data_frames:
            await peer.write_value(ft_char, frame)

    asyncio.get_event_loop().run_until_complete(_run())

    dut.expect("Updating custom led sequence", timeout=10)
    dut.expect("NOTIFICATION_EVENTS_BLE_FILE_LEDJSON_RECVD", timeout=5)
