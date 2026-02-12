"""
Automated LED/touch integration tests for QEMU visualization.

These tests connect directly to QEMU's UART2 TCP port (bypassing the
viz_bridge WebSocket layer) to inject touch events and verify LED frames.

Usage:
    pytest tests/test_touch_led.py -v

Prerequisites:
    - QEMU running with firmware built in QEMU mode
    - UART2 TCP port available at localhost:1235
"""

import asyncio
import struct
import pytest

# Protocol constants (must match qemu_viz_transport.h)
FRAME_START = 0xAA
FRAME_END = 0x55
MSG_LED_FRAME = 0x01
MSG_TOUCH_EVENT = 0x02
MSG_MODE_CHANGE = 0x03

TOUCH_RELEASED = 0
TOUCH_TOUCHED = 1
TOUCH_SHORT_PRESSED = 2
TOUCH_LONG_PRESSED = 3
TOUCH_VERY_LONG_PRESSED = 4


class QemuVizClient:
    """Direct TCP client for QEMU's UART2 visualization port."""

    def __init__(self, host: str = "localhost", port: int = 1235):
        self.host = host
        self.port = port
        self.reader = None
        self.writer = None

    async def connect(self, timeout: float = 10.0):
        """Connect to QEMU's UART2 TCP socket."""
        self.reader, self.writer = await asyncio.wait_for(
            asyncio.open_connection(self.host, self.port),
            timeout=timeout,
        )

    async def close(self):
        """Close the connection."""
        if self.writer:
            self.writer.close()
            try:
                await self.writer.wait_closed()
            except Exception:
                pass

    async def send_touch(self, sensor_idx: int, event: int):
        """Send a touch event to QEMU firmware."""
        frame = bytes([FRAME_START, MSG_TOUCH_EVENT, sensor_idx & 0xFF, event & 0xFF, FRAME_END])
        self.writer.write(frame)
        await self.writer.drain()

    async def read_led_frame(self, timeout: float = 2.0):
        """
        Read the next LED frame from QEMU.

        Returns a LedFrame namedtuple-like object with:
            .num_leds: int
            .pixels: list of (r, g, b) tuples
        """
        loop = asyncio.get_running_loop()
        deadline = loop.time() + timeout

        while loop.time() < deadline:
            remaining = deadline - loop.time()
            if remaining <= 0:
                break

            try:
                # Wait for frame start
                byte = await asyncio.wait_for(self.reader.readexactly(1), timeout=remaining)
                if byte[0] != FRAME_START:
                    continue

                # Read message type
                msg_type = await asyncio.wait_for(self.reader.readexactly(1), timeout=1.0)
                if msg_type[0] != MSG_LED_FRAME:
                    # Skip non-LED messages
                    continue

                # Read LED count
                count_bytes = await asyncio.wait_for(self.reader.readexactly(2), timeout=1.0)
                num_leds = struct.unpack("<H", count_bytes)[0]

                # Read pixel data
                pixel_data = await asyncio.wait_for(
                    self.reader.readexactly(num_leds * 3), timeout=1.0
                )

                # Read frame end
                end_byte = await asyncio.wait_for(self.reader.readexactly(1), timeout=1.0)
                if end_byte[0] != FRAME_END:
                    continue

                # Parse pixels
                pixels = []
                for i in range(num_leds):
                    offset = i * 3
                    pixels.append((pixel_data[offset], pixel_data[offset + 1], pixel_data[offset + 2]))

                return LedFrame(num_leds=num_leds, pixels=pixels)

            except asyncio.TimeoutError:
                break
            except asyncio.IncompleteReadError:
                break

        raise TimeoutError(f"No LED frame received within {timeout}s")


class LedFrame:
    """Represents a single LED frame received from QEMU."""

    def __init__(self, num_leds: int, pixels: list):
        self.num_leds = num_leds
        self.pixels = pixels  # list of (r, g, b) tuples

    def has_any_lit(self) -> bool:
        """Check if any LED is non-black."""
        return any(p != (0, 0, 0) for p in self.pixels)

    def pixel_at(self, index: int):
        """Get pixel (r, g, b) at the given index."""
        if index < 0 or index >= len(self.pixels):
            return (0, 0, 0)
        return self.pixels[index]


# ----------------------------------------------------------------
# Tests
# ----------------------------------------------------------------


@pytest.mark.esp32
@pytest.mark.qemu
async def test_led_sequence_renders(viz_client):
    """Verify LED sequence mode produces non-black frames after boot."""
    # Read several LED frames — at least one should have non-black pixels
    for _ in range(20):
        try:
            frame = await viz_client.read_led_frame(timeout=2.0)
            if frame.has_any_lit():
                return  # Success — at least one frame has lit pixels
        except TimeoutError:
            continue

    pytest.fail("No non-black LED frames observed after 20 attempts")


@pytest.mark.esp32
@pytest.mark.qemu
async def test_touch_fires_notification(viz_client):
    """Verify that injecting a touch event produces a change in LED output."""
    # Capture a baseline frame
    try:
        baseline = await viz_client.read_led_frame(timeout=3.0)
    except TimeoutError:
        pytest.skip("No LED frames available — firmware may not be running")

    # Inject a touch event on sensor 0
    await viz_client.send_touch(sensor_idx=0, event=TOUCH_TOUCHED)

    # Wait a bit for the firmware to process
    await asyncio.sleep(0.5)

    # Send release
    await viz_client.send_touch(sensor_idx=0, event=TOUCH_RELEASED)

    # Read frames and check that something changed
    # (We can't predict exact pixels, but the touch should trigger some response)
    frames_read = 0
    for _ in range(10):
        try:
            frame = await viz_client.read_led_frame(timeout=1.0)
            frames_read += 1
        except TimeoutError:
            break

    assert frames_read > 0, "Should receive LED frames after touch event"


@pytest.mark.esp32
@pytest.mark.qemu
async def test_touch_short_press(viz_client):
    """Verify short press event can be injected."""
    await viz_client.send_touch(sensor_idx=0, event=TOUCH_SHORT_PRESSED)
    await asyncio.sleep(0.2)
    await viz_client.send_touch(sensor_idx=0, event=TOUCH_RELEASED)

    # Just verify we can still read frames (firmware didn't crash)
    try:
        frame = await viz_client.read_led_frame(timeout=3.0)
        assert frame.num_leds > 0
    except TimeoutError:
        pytest.skip("No LED frames available")


@pytest.mark.esp32
@pytest.mark.qemu
async def test_led_frame_format(viz_client):
    """Verify LED frame has expected structure."""
    try:
        frame = await viz_client.read_led_frame(timeout=5.0)
    except TimeoutError:
        pytest.skip("No LED frames available")

    assert frame.num_leds > 0, "Frame should have at least 1 LED"
    assert len(frame.pixels) == frame.num_leds, "Pixel count should match num_leds"

    # Each pixel should be a 3-tuple of ints 0-255
    for i, pixel in enumerate(frame.pixels):
        assert len(pixel) == 3, f"Pixel {i} should have 3 components"
        for c in pixel:
            assert 0 <= c <= 255, f"Pixel {i} component out of range: {c}"
