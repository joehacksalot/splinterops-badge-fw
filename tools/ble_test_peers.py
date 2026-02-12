#!/usr/bin/env python3
"""
Scriptable BLE peer badges for QEMU testing.

Provides helper classes and functions to create virtual BLE peers that
interact with the badge firmware running in QEMU.  These peers can:

  - Advertise with IWC manufacturer payloads (peer discovery)
  - Advertise with the service-enable UUID (trigger BLE service)
  - Act as GATT clients (file transfer, interactive game)
  - Connect, read, write, and subscribe to notifications

Used by the pytest test suite in tests/test_ble_*.py.
"""

import struct
import asyncio
import logging
from typing import Optional

from bumble.controller import Controller
from bumble.core import AdvertisingData
from bumble.device import Device, Connection, Peer
from bumble.host import Host
from bumble.link import LocalLink
from bumble.gatt import Service, Characteristic
from bumble.transport import open_transport

logger = logging.getLogger(__name__)

# --- Constants matching firmware definitions ---

EVENT_ADV_MAGIC_NUMBER = 0x1337
BADGE_ID_SIZE = 8
EVENT_ID_SIZE = 8
PAIR_ID_SIZE = 8
CONFIG_FRAME_HEADER_SIZE = 15
DATA_FRAME_HEADER_SIZE = 2
DATA_FRAME_MAX_SIZE = 500

# GATT UUIDs matching BleControl_Service.c
FILE_TRANSFER_CHR_UUID = "4c3d5364-2bda-a28c-f84d-c7d1868a4e77"
INTERACTIVE_GAME_CHR_UUID = "4c3d5364-2bda-a28c-f84d-c7d1868a4f77"

# Service UUID base from BleControl_Service.c
SERVICE_UUID = "0000ff8b-0000-1000-8000-00805f9b34fb"

# Badge BLE device names (from Utilities.c GetBadgeBleDeviceName)
# Default QEMU build uses FMAN25_BADGE → "IWCv4"
BADGE_BLE_NAMES = ["IWCv1", "IWCv2", "IWCv3", "IWCv4"]
DEFAULT_BADGE_BLE_NAME = "IWCv4"


def build_iwc_advertising_payload(
    magic: int = EVENT_ADV_MAGIC_NUMBER,
    badge_type: int = 0x01,
    badge_id: Optional[bytes] = None,
    event_id: Optional[bytes] = None,
) -> bytes:
    """
    Build an IWC advertising payload matching the IwcAdvertisingPayload struct.

    Layout (little-endian):
        uint16_t magicNum
        uint8_t  badgeType
        uint8_t  badgeId[8]
        uint8_t  eventId[8]
    """
    if badge_id is None:
        badge_id = bytes([0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08])
    if event_id is None:
        event_id = bytes(EVENT_ID_SIZE)

    assert len(badge_id) == BADGE_ID_SIZE
    assert len(event_id) == EVENT_ID_SIZE

    return struct.pack(
        "<HB8s8s",
        magic,
        badge_type,
        badge_id,
        event_id,
    )


def build_iwc_advertising_data(
    name: str = "PeerBadge",
    **iwc_kwargs,
) -> bytes:
    """Build complete advertising data with IWC manufacturer payload."""
    iwc_payload = build_iwc_advertising_payload(**iwc_kwargs)
    return bytes(
        AdvertisingData([
            (AdvertisingData.COMPLETE_LOCAL_NAME, name.encode("utf-8")),
            (AdvertisingData.MANUFACTURER_SPECIFIC_DATA, iwc_payload),
        ])
    )


def build_service_enable_uuid(pair_id: bytes) -> bytes:
    """
    Build the 128-bit UUID that triggers BLE service enable on the badge.

    The UUID is constructed as:
        base zeros + reversed pair_id + magic bytes [0x38, 0x13]
    Matching _BleControl_ParseEnableBleServiceAdvertisingPacket logic.
    """
    assert len(pair_id) == PAIR_ID_SIZE
    uuid_bytes = bytearray(16)
    # Place reversed pair_id at offset (16 - PAIR_ID_SIZE - 2) = 6
    offset = 16 - PAIR_ID_SIZE - 2
    for i in range(PAIR_ID_SIZE):
        uuid_bytes[offset + i] = pair_id[PAIR_ID_SIZE - 1 - i]
    # Magic bytes at the end
    uuid_bytes[14] = 0x38
    uuid_bytes[15] = 0x13
    return bytes(uuid_bytes)


def build_config_frame(
    num_frames: int,
    frame_len: int,
    file_type: int,
    pair_id: Optional[bytes] = None,
) -> bytes:
    """
    Build a BLE file transfer config frame (frame 0).

    Layout (big-endian where noted):
        uint16_t curFrame   = 0 (BE)
        uint16_t numFrames  (BE)
        uint16_t frameLen   (BE)
        uint8_t  fileType
        uint8_t  pairId[8]
    Total: 15 bytes = CONFIG_FRAME_HEADER_SIZE
    """
    if pair_id is None:
        pair_id = bytes(PAIR_ID_SIZE)
    assert len(pair_id) == PAIR_ID_SIZE

    return struct.pack(
        ">HHHB",
        0,              # curFrame
        num_frames,     # numFrames (0-based count of data frames)
        frame_len,      # frameLen
        file_type,      # fileType (uint8_t — unsigned)
    ) + pair_id


def split_into_data_frames(data: bytes, frame_len: int) -> list[bytes]:
    """
    Split data into BLE file transfer data frames.

    Each frame has a 2-byte big-endian frame number header followed by
    payload bytes. frame_len is the total frame size including the header.
    """
    payload_len = frame_len - DATA_FRAME_HEADER_SIZE
    frames = []
    offset = 0
    frame_num = 1  # Data frames start at 1 (frame 0 is config)

    while offset < len(data):
        chunk = data[offset : offset + payload_len]
        # Pad last frame if needed
        if len(chunk) < payload_len:
            chunk = chunk + bytes(payload_len - len(chunk))
        header = struct.pack(">H", frame_num)
        frames.append(header + chunk)
        offset += payload_len
        frame_num += 1

    return frames


async def create_peer_badge(
    link: LocalLink,
    name: str = "PeerBadge",
    badge_type: int = 0x01,
    badge_id: Optional[bytes] = None,
    event_id: Optional[bytes] = None,
) -> Device:
    """
    Create and power on a virtual peer badge attached to the given link.
    Returns the Bumble Device (not yet advertising).
    """
    controller = Controller(name, link=link)
    device = Device(name=name)
    device.host = Host(controller, controller)
    await device.power_on()
    return device


async def start_iwc_advertising(
    device: Device,
    badge_type: int = 0x01,
    badge_id: Optional[bytes] = None,
    event_id: Optional[bytes] = None,
):
    """Start advertising with an IWC manufacturer payload."""
    adv_data = build_iwc_advertising_data(
        name=device.name,
        badge_type=badge_type,
        badge_id=badge_id,
        event_id=event_id,
    )
    await device.start_advertising(advertising_data=adv_data)
    logger.info("Peer '%s' advertising with IWC payload", device.name)


async def create_gatt_client(
    link: LocalLink,
    name: str = "GATTClient",
) -> Device:
    """
    Create a Bumble device that can act as a GATT client.
    Returns the powered-on Device.
    """
    controller = Controller(name, link=link)
    device = Device(name=name)
    device.host = Host(controller, controller)
    await device.power_on()
    return device
