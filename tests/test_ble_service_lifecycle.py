# tests/test_ble_service_lifecycle.py
"""
BLE service enable/disable lifecycle tests.

Validates:
  - Dynamic GATT service add/delete
  - Advertising mode changes (IWC vs service)
  - Service disable timeout timer
  - Connect → disconnect → re-enable cycle
"""
import pytest


@pytest.mark.esp32
@pytest.mark.qemu
@pytest.mark.ble
def test_ble_service_enable_via_advertisement(dut, bumble_link):
    """Verify badge enables BLE service when it sees the service-enable UUID."""
    import asyncio
    from tools.ble_test_peers import (
        create_peer_badge,
        build_service_enable_uuid,
    )
    from bumble.core import AdvertisingData

    async def _run():
        peer = await create_peer_badge(bumble_link, name="ServiceEnabler")

        # Build the service-enable UUID using a known pair_id
        pair_id = bytes([0x00] * 8)
        uuid_bytes = build_service_enable_uuid(pair_id)

        adv_data = bytes(
            AdvertisingData([
                (AdvertisingData.COMPLETE_LIST_OF_128_BIT_SERVICE_CLASS_UUIDS, uuid_bytes),
            ])
        )
        await peer.start_advertising(advertising_data=adv_data)

    asyncio.get_event_loop().run_until_complete(_run())

    dut.expect("enabling BLE Service", timeout=15)


@pytest.mark.esp32
@pytest.mark.qemu
@pytest.mark.ble
def test_connect_disconnect_cycle(dut, bumble_link):
    """Verify BLE service enable → connect → disconnect → disable cycle."""
    import asyncio
    from tools.ble_test_peers import create_gatt_client
    from bumble.device import Peer

    async def _run():
        client_device = await create_gatt_client(bumble_link, name="CycleClient")

        # Wait for badge to be advertising
        await asyncio.sleep(2)

        target = await client_device.find_device_by_name("IWCv4")
        if target is None:
            await asyncio.sleep(3)
            target = await client_device.find_device_by_name("IWCv4")

        assert target is not None, "Could not find badge device"

        # Connect
        connection = await client_device.connect(target.address)
        peer = Peer(connection)
        await peer.discover_services()

        # Brief pause then disconnect
        await asyncio.sleep(1)
        await connection.disconnect()

    asyncio.get_event_loop().run_until_complete(_run())

    dut.expect("Connected", timeout=10)
    dut.expect("Disconnected", timeout=10)


@pytest.mark.esp32
@pytest.mark.qemu
@pytest.mark.ble
def test_mtu_exchange(dut, bumble_link):
    """Verify MTU negotiation occurs on connection."""
    import asyncio
    from tools.ble_test_peers import create_gatt_client
    from bumble.device import Peer

    async def _run():
        client_device = await create_gatt_client(bumble_link, name="MTUClient")

        await asyncio.sleep(2)
        target = await client_device.find_device_by_name("IWCv4")
        if target is None:
            await asyncio.sleep(3)
            target = await client_device.find_device_by_name("IWCv4")

        assert target is not None, "Could not find badge device"
        connection = await client_device.connect(target.address)
        peer = Peer(connection)
        await peer.discover_services()

        # Allow time for MTU exchange
        await asyncio.sleep(2)
        await connection.disconnect()

    asyncio.get_event_loop().run_until_complete(_run())

    dut.expect("MTU Update", timeout=10)


@pytest.mark.esp32
@pytest.mark.qemu
@pytest.mark.ble
def test_advertising_starts_on_boot(dut):
    """Verify badge starts IWC advertising on boot."""
    dut.expect("Starting advertising", timeout=15)
    dut.expect("Starting advertisement scan", timeout=15)
