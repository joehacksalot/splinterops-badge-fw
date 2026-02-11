#!/usr/bin/env python3
"""
Virtual BLE controller for QEMU testing.

Connects to QEMU's HCI TCP port and provides a full BLE controller
using Google's Bumble library. The controller simulates a complete
BLE link layer, allowing the NimBLE host stack running inside QEMU
to perform advertising, scanning, connections, and GATT operations.

Usage:
    # Single badge (QEMU connects as TCP client):
    python tools/ble_virtual_controller.py

    # With custom port:
    python tools/ble_virtual_controller.py --port 1234

    # With a simulated peer badge for IWC testing:
    python tools/ble_virtual_controller.py --with-peer
"""

import argparse
import asyncio
import logging
import struct
import sys

from bumble.controller import Controller
from bumble.link import LocalLink
from bumble.transport import open_transport

logger = logging.getLogger(__name__)


async def run_controller(port: int, with_peer: bool):
    """Start the virtual BLE controller and optionally a peer device."""

    # Create a local link (virtual radio medium shared by all controllers)
    link = LocalLink()

    # Create the virtual controller that bridges to QEMU via TCP.
    # QEMU's UART1 is mapped to a TCP chardev; Bumble acts as the TCP server.
    transport_spec = f"tcp-server:0.0.0.0:{port}"
    logger.info("Waiting for QEMU HCI connection on port %d ...", port)

    transport = await open_transport(transport_spec)
    controller = Controller(
        "badge-controller",
        host_source=transport.source,
        host_sink=transport.sink,
        link=link,
    )
    logger.info("QEMU HCI transport connected — controller ready")

    if with_peer:
        await _start_peer_badge(link)

    # Keep running until interrupted
    try:
        await asyncio.get_event_loop().create_future()
    except asyncio.CancelledError:
        pass
    finally:
        await transport.close()
        logger.info("Controller shut down")


async def _start_peer_badge(link: LocalLink):
    """
    Attach a simulated peer badge to the same virtual radio.
    This peer advertises with the SplinterOps IWC manufacturer payload
    so the QEMU badge can discover it via BLE scanning.
    """
    from bumble.device import Device
    from bumble.host import Host

    peer_controller = Controller("peer-badge", link=link)
    peer_device = Device(name="PeerBadge")
    peer_device.host = Host(peer_controller, peer_controller)

    await peer_device.power_on()

    # Build IWC advertising payload matching IwcAdvertisingPayload struct:
    #   uint16_t magicNum   = 0x1337
    #   uint8_t  badgeType  = 0x01
    #   uint8_t  badgeId[8] = 0x01..0x08
    #   uint8_t  eventId[8] = 0x00 * 8
    iwc_payload = struct.pack(
        "<HB8s8s",
        0x1337,                             # magicNum
        0x01,                               # badgeType
        bytes([0x01, 0x02, 0x03, 0x04,
               0x05, 0x06, 0x07, 0x08]),    # badgeId
        bytes(8),                           # eventId (zeroed)
    )

    # Advertise with manufacturer-specific data
    from bumble.core import AdvertisingData
    adv_data = bytes(
        AdvertisingData([
            (AdvertisingData.COMPLETE_LOCAL_NAME, b"PeerBadge"),
            (AdvertisingData.MANUFACTURER_SPECIFIC_DATA, iwc_payload),
        ])
    )

    await peer_device.start_advertising(advertising_data=adv_data)
    logger.info("Peer badge advertising with IWC payload")


def main():
    parser = argparse.ArgumentParser(
        description="Virtual BLE controller for QEMU badge testing"
    )
    parser.add_argument(
        "--port", type=int, default=1234,
        help="TCP port for HCI transport (default: 1234)",
    )
    parser.add_argument(
        "--with-peer", action="store_true",
        help="Also start a simulated peer badge for IWC testing",
    )
    parser.add_argument(
        "--log-level", default="INFO",
        choices=["DEBUG", "INFO", "WARNING", "ERROR"],
        help="Logging level (default: INFO)",
    )
    args = parser.parse_args()

    logging.basicConfig(
        level=getattr(logging, args.log_level),
        format="%(asctime)s [%(name)s] %(levelname)s: %(message)s",
    )

    try:
        asyncio.run(run_controller(args.port, args.with_peer))
    except KeyboardInterrupt:
        logger.info("Interrupted — exiting")
        sys.exit(0)


if __name__ == "__main__":
    main()
