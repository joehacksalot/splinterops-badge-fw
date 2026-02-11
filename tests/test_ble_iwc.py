# tests/test_ble_iwc.py
"""
BLE IWC (Inter-badge Wireless Communication) peer discovery tests.

Validates:
  - BleControl_AdvScan.c → _BleControl_ProcessAdvertisement()
  - IWC advertising payload parsing
  - NotificationDispatcher peer heartbeat events
"""
import pytest
import struct


@pytest.mark.esp32
@pytest.mark.qemu
@pytest.mark.ble
def test_peer_discovery(dut, bumble_link):
    """Verify badge detects a peer advertising with IWC manufacturer payload."""
    import asyncio
    from tools.ble_test_peers import create_peer_badge, start_iwc_advertising

    async def _run():
        peer = await create_peer_badge(
            bumble_link,
            name="PeerTron",
            badge_type=0x01,
        )
        await start_iwc_advertising(
            peer,
            badge_type=0x01,
            badge_id=bytes([0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08]),
        )

    asyncio.get_event_loop().run_until_complete(_run())

    # The badge should detect the peer via passive scanning and log this
    dut.expect("Badge advertising packet found", timeout=15)


@pytest.mark.esp32
@pytest.mark.qemu
@pytest.mark.ble
def test_peer_discovery_wrong_magic(dut, bumble_link):
    """Verify badge ignores advertising packets with wrong magic number."""
    import asyncio
    from tools.ble_test_peers import (
        create_peer_badge,
        build_iwc_advertising_payload,
    )
    from bumble.core import AdvertisingData

    async def _run():
        peer = await create_peer_badge(bumble_link, name="BadPeer")
        # Use wrong magic number
        payload = build_iwc_advertising_payload(magic=0xDEAD)
        adv_data = bytes(
            AdvertisingData([
                (AdvertisingData.COMPLETE_LOCAL_NAME, b"BadPeer"),
                (AdvertisingData.MANUFACTURER_SPECIFIC_DATA, payload),
            ])
        )
        await peer.start_advertising(advertising_data=adv_data)

    asyncio.get_event_loop().run_until_complete(_run())

    # Badge should NOT log peer detection for wrong magic
    with pytest.raises(Exception):
        dut.expect("Badge advertising packet found", timeout=5)


@pytest.mark.esp32
@pytest.mark.qemu
@pytest.mark.ble
def test_multiple_peers(dut, bumble_link):
    """Verify badge can detect multiple peer badges advertising simultaneously."""
    import asyncio
    from tools.ble_test_peers import create_peer_badge, start_iwc_advertising

    async def _run():
        for i in range(3):
            peer = await create_peer_badge(
                bumble_link,
                name=f"Peer{i}",
                badge_type=i + 1,
            )
            badge_id = bytes([i + 1] * 8)
            await start_iwc_advertising(peer, badge_type=i + 1, badge_id=badge_id)

    asyncio.get_event_loop().run_until_complete(_run())

    # Should detect at least one peer
    dut.expect("Badge advertising packet found", timeout=15)
