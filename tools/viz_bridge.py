#!/usr/bin/env python3
"""
Visualization Bridge — connects QEMU's UART2 TCP socket to a WebSocket
server for the browser-based badge visualization UI.

LED frames:   TCP (QEMU) → parse binary → JSON → WebSocket → browser
Touch events: WebSocket (browser) → JSON → encode binary → TCP → QEMU

Usage:
    python tools/viz_bridge.py [--qemu-port 1235] [--ws-port 8765] [--http-port 8080]

Dependencies:
    pip install websockets
"""

import argparse
import asyncio
import json
import logging
import os
import re
import struct
import sys
from pathlib import Path

try:
    import websockets
    from websockets.server import serve as ws_serve
except ImportError:
    print("ERROR: 'websockets' package required. Install with: pip install websockets")
    sys.exit(1)

logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(name)s] %(message)s")
log = logging.getLogger("viz_bridge")

# Protocol constants (must match qemu_viz_transport.h)
FRAME_START = 0xAA
FRAME_END = 0x55
MSG_LED_FRAME = 0x01
MSG_TOUCH_EVENT = 0x02
MSG_MODE_CHANGE = 0x03
MSG_TONE_EVENT = 0x04


# Regex to extract ESP-IDF log level from a line like:
#   I (1234) TAG: message
#   E (1234) TAG: message
_ESP_LOG_RE = re.compile(
    r'^([EWIDV])\s*\(\d+\)\s'
)
_LEVEL_MAP = {'E': 'error', 'W': 'warn', 'I': 'info', 'D': 'debug', 'V': 'verbose'}


class VizBridge:
    """Bridges QEMU UART2 TCP ↔ WebSocket for LED/touch visualization."""

    def __init__(self, qemu_host: str, qemu_port: int, ws_port: int, http_port: int,
                 console_port: int | None = None):
        self.qemu_host = qemu_host
        self.qemu_port = qemu_port
        self.console_port = console_port
        self.ws_port = ws_port
        self.http_port = http_port
        self.ws_clients: set = set()
        self.qemu_reader: asyncio.StreamReader | None = None
        self.qemu_writer: asyncio.StreamWriter | None = None
        self.connected = False
        self.last_frame: dict | None = None
        self.console_connected = False

    # ----------------------------------------------------------------
    # QEMU TCP connection
    # ----------------------------------------------------------------

    async def connect_qemu(self):
        """Connect to QEMU's UART2 TCP socket with retry."""
        while True:
            try:
                self.qemu_reader, self.qemu_writer = await asyncio.open_connection(
                    self.qemu_host, self.qemu_port
                )
                self.connected = True
                log.info(f"Connected to QEMU UART2 at {self.qemu_host}:{self.qemu_port}")
                await self.broadcast_json({"type": "status", "connected": True})
                return
            except (ConnectionRefusedError, OSError) as e:
                log.warning(f"QEMU not ready ({e}), retrying in 2s...")
                await asyncio.sleep(2)

    async def qemu_rx_loop(self):
        """Read binary frames from QEMU and forward as JSON to WebSocket clients."""
        while True:
            if not self.connected:
                await self.connect_qemu()

            try:
                await self._process_qemu_frames()
            except (ConnectionResetError, asyncio.IncompleteReadError, OSError) as e:
                log.warning(f"QEMU connection lost ({e}), reconnecting...")
                self.connected = False
                await self.broadcast_json({"type": "status", "connected": False})
                await asyncio.sleep(1)

    async def _process_qemu_frames(self):
        """Parse framed binary messages from QEMU UART2."""
        while True:
            # Wait for frame start marker
            byte = await self.qemu_reader.readexactly(1)
            if byte[0] != FRAME_START:
                continue

            # Read message type
            msg_type_byte = await self.qemu_reader.readexactly(1)
            msg_type = msg_type_byte[0]

            if msg_type == MSG_LED_FRAME:
                await self._handle_led_frame()
            elif msg_type == MSG_MODE_CHANGE:
                await self._handle_mode_change()
            elif msg_type == MSG_TONE_EVENT:
                await self._handle_tone_event()
            else:
                log.warning(f"Unknown message type from QEMU: 0x{msg_type:02x}")

    async def _handle_led_frame(self):
        """Parse LED frame and broadcast to WebSocket clients."""
        # Read LED count (uint16_t little-endian)
        count_bytes = await self.qemu_reader.readexactly(2)
        num_leds = struct.unpack("<H", count_bytes)[0]

        # Read pixel data (RGB × num_leds)
        pixel_data = await self.qemu_reader.readexactly(num_leds * 3)

        # Read frame end marker
        end_byte = await self.qemu_reader.readexactly(1)
        if end_byte[0] != FRAME_END:
            log.warning("Missing frame end marker on LED frame")
            return

        # Convert to list of [r, g, b] arrays
        pixels = []
        for i in range(num_leds):
            offset = i * 3
            pixels.append([pixel_data[offset], pixel_data[offset + 1], pixel_data[offset + 2]])

        msg = {"type": "led_frame", "num_leds": num_leds, "pixels": pixels}
        self.last_frame = msg
        await self.broadcast_json(msg)

    async def _handle_mode_change(self):
        """Parse mode change notification and broadcast."""
        # Read 3 bytes: mode, innerState, outerState
        data = await self.qemu_reader.readexactly(3)
        end_byte = await self.qemu_reader.readexactly(1)
        if end_byte[0] != FRAME_END:
            log.warning("Missing frame end marker on mode change")
            return

        msg = {
            "type": "mode_change",
            "mode": data[0],
            "inner_state": data[1],
            "outer_state": data[2],
        }
        await self.broadcast_json(msg)

    async def _handle_tone_event(self):
        """Parse tone event and broadcast to WebSocket clients."""
        # Read 3 bytes: action (uint8), frequency (uint16 LE)
        data = await self.qemu_reader.readexactly(3)
        end_byte = await self.qemu_reader.readexactly(1)
        if end_byte[0] != FRAME_END:
            log.warning("Missing frame end marker on tone event")
            return

        action = data[0]  # 0=stop, 1=start
        frequency = struct.unpack("<H", data[1:3])[0]

        msg = {
            "type": "tone",
            "action": "start" if action == 1 else "stop",
            "frequency": frequency,
        }
        await self.broadcast_json(msg)

    # ----------------------------------------------------------------
    # QEMU Console log reader
    # ----------------------------------------------------------------

    async def console_rx_loop(self):
        """Listen for QEMU console serial connection and read log lines."""
        if self.console_port is None:
            return

        # Start a TCP server that QEMU connects to (QEMU serial in client mode)
        server = await asyncio.start_server(
            self._handle_console_client, "0.0.0.0", self.console_port
        )
        log.info(f"Console log TCP server listening on port {self.console_port}")
        async with server:
            await server.serve_forever()

    async def _handle_console_client(self, reader: asyncio.StreamReader,
                                      writer: asyncio.StreamWriter):
        """Handle a QEMU console serial connection."""
        peer = writer.get_extra_info('peername')
        log.info(f"QEMU console connected from {peer}")
        self.console_connected = True
        try:
            await self._process_console_lines(reader)
        except (ConnectionResetError, asyncio.IncompleteReadError, OSError) as e:
            log.warning(f"QEMU console connection lost ({e})")
        finally:
            self.console_connected = False
            writer.close()

    async def _process_console_lines(self, reader: asyncio.StreamReader):
        """Read lines from the console stream, parse log level, broadcast."""
        while True:
            raw = await reader.readline()
            if not raw:
                raise ConnectionResetError("EOF on console stream")

            line = raw.decode("utf-8", errors="replace").rstrip("\r\n")
            if not line:
                continue

            # Echo to local terminal so the user still sees logs
            print(line, flush=True)

            # Parse ESP-IDF log level
            level = "info"  # default
            m = _ESP_LOG_RE.match(line)
            if m:
                level = _LEVEL_MAP.get(m.group(1), "info")

            msg = {"type": "console_log", "level": level, "text": line}
            await self.broadcast_json(msg)

    # ----------------------------------------------------------------
    # WebSocket server
    # ----------------------------------------------------------------

    async def ws_handler(self, websocket):
        """Handle a WebSocket client connection."""
        self.ws_clients.add(websocket)
        log.info(f"WebSocket client connected ({len(self.ws_clients)} total)")

        # Send current connection status
        await websocket.send(json.dumps({"type": "status", "connected": self.connected}))

        # Send last known LED frame if available
        if self.last_frame:
            await websocket.send(json.dumps(self.last_frame))

        try:
            async for message in websocket:
                await self._handle_ws_message(message)
        except websockets.exceptions.ConnectionClosed:
            pass
        finally:
            self.ws_clients.discard(websocket)
            log.info(f"WebSocket client disconnected ({len(self.ws_clients)} total)")

    async def _handle_ws_message(self, message: str):
        """Handle incoming WebSocket message (touch events from browser)."""
        try:
            data = json.loads(message)
        except json.JSONDecodeError:
            log.warning(f"Invalid JSON from WebSocket: {message}")
            return

        if data.get("type") == "touch_event":
            sensor_idx = data.get("sensor_idx", 0)
            event_type = data.get("event_type", 0)
            await self._send_touch_to_qemu(sensor_idx, event_type)

    async def _send_touch_to_qemu(self, sensor_idx: int, event_type: int):
        """Encode and send a touch event to QEMU via TCP."""
        if not self.connected or not self.qemu_writer:
            log.warning("Cannot send touch event — not connected to QEMU")
            return

        # Touch Event Protocol (UI → Firmware):
        # Byte 0: 0xAA (start), Byte 1: 0x02 (TOUCH_EVENT),
        # Byte 2: sensor_idx, Byte 3: event_type, Byte 4: 0x55 (end)
        frame = bytes([FRAME_START, MSG_TOUCH_EVENT, sensor_idx & 0xFF, event_type & 0xFF, FRAME_END])
        try:
            self.qemu_writer.write(frame)
            await self.qemu_writer.drain()
            log.debug(f"Sent touch event: sensor={sensor_idx} event={event_type}")
        except (ConnectionResetError, OSError) as e:
            log.warning(f"Failed to send touch event: {e}")

    async def broadcast_json(self, msg: dict):
        """Send a JSON message to all connected WebSocket clients."""
        if not self.ws_clients:
            return
        text = json.dumps(msg)
        # Use gather to send to all clients concurrently
        await asyncio.gather(
            *[self._safe_send(ws, text) for ws in self.ws_clients.copy()],
            return_exceptions=True,
        )

    @staticmethod
    async def _safe_send(ws, text: str):
        try:
            await ws.send(text)
        except websockets.exceptions.ConnectionClosed:
            pass

    # ----------------------------------------------------------------
    # Simple HTTP server for the visualization UI
    # ----------------------------------------------------------------

    async def http_handler(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter):
        """Minimal HTTP server to serve the viz UI static files."""
        try:
            request_line = await asyncio.wait_for(reader.readline(), timeout=5.0)
            request_str = request_line.decode("utf-8", errors="replace").strip()

            # Read remaining headers (discard)
            while True:
                line = await asyncio.wait_for(reader.readline(), timeout=5.0)
                if line == b"\r\n" or line == b"\n" or line == b"":
                    break

            # Parse path from request
            parts = request_str.split(" ")
            if len(parts) < 2:
                writer.close()
                return

            path = parts[1]
            if path == "/" or path == "":
                path = "/index.html"

            # Serve from tools/viz_ui/
            viz_ui_dir = Path(__file__).parent / "viz_ui"
            file_path = (viz_ui_dir / path.lstrip("/")).resolve()

            # Security: ensure we're still within viz_ui_dir
            if not str(file_path).startswith(str(viz_ui_dir.resolve())):
                await self._send_http_response(writer, 403, "text/plain", b"Forbidden")
                return

            if file_path.is_file():
                content = file_path.read_bytes()
                content_type = self._guess_content_type(file_path.name)
                await self._send_http_response(writer, 200, content_type, content)
            else:
                await self._send_http_response(writer, 404, "text/plain", b"Not Found")

        except (asyncio.TimeoutError, ConnectionResetError, OSError):
            pass
        finally:
            try:
                writer.close()
                await writer.wait_closed()
            except Exception:
                pass

    @staticmethod
    async def _send_http_response(writer, status: int, content_type: str, body: bytes):
        status_text = {200: "OK", 403: "Forbidden", 404: "Not Found"}.get(status, "Error")
        header = (
            f"HTTP/1.1 {status} {status_text}\r\n"
            f"Content-Type: {content_type}\r\n"
            f"Content-Length: {len(body)}\r\n"
            f"Access-Control-Allow-Origin: *\r\n"
            f"Connection: close\r\n"
            f"\r\n"
        )
        writer.write(header.encode("utf-8") + body)
        await writer.drain()

    @staticmethod
    def _guess_content_type(filename: str) -> str:
        ext = filename.rsplit(".", 1)[-1].lower() if "." in filename else ""
        return {
            "html": "text/html; charset=utf-8",
            "js": "application/javascript; charset=utf-8",
            "css": "text/css; charset=utf-8",
            "json": "application/json; charset=utf-8",
            "png": "image/png",
            "svg": "image/svg+xml",
            "ico": "image/x-icon",
        }.get(ext, "application/octet-stream")

    # ----------------------------------------------------------------
    # Main entry point
    # ----------------------------------------------------------------

    async def run(self):
        """Start all services."""
        # Start WebSocket server
        ws_server = await ws_serve(self.ws_handler, "0.0.0.0", self.ws_port)
        log.info(f"WebSocket server listening on ws://localhost:{self.ws_port}")

        # Start HTTP server for UI
        http_server = await asyncio.start_server(
            self.http_handler, "0.0.0.0", self.http_port
        )
        log.info(f"HTTP server listening on http://localhost:{self.http_port}")

        # Start QEMU connection (runs forever with reconnect)
        qemu_task = asyncio.create_task(self.qemu_rx_loop())

        # Start console log reader if port is configured
        console_task = asyncio.create_task(self.console_rx_loop())

        log.info("Viz bridge running. Press Ctrl+C to stop.")

        try:
            await asyncio.gather(
                ws_server.wait_closed() if hasattr(ws_server, 'wait_closed') else asyncio.sleep(float('inf')),
                qemu_task,
                console_task,
                return_exceptions=True,
            )
        except asyncio.CancelledError:
            pass


def main():
    parser = argparse.ArgumentParser(description="QEMU Badge Visualization Bridge")
    parser.add_argument("--qemu-host", default="localhost", help="QEMU TCP host")
    parser.add_argument("--qemu-port", type=int, default=1235, help="QEMU UART2 TCP port")
    parser.add_argument("--console-port", type=int, default=None, help="QEMU console serial TCP port")
    parser.add_argument("--ws-port", type=int, default=8765, help="WebSocket server port")
    parser.add_argument("--http-port", type=int, default=8080, help="HTTP server port for UI")
    parser.add_argument("--verbose", "-v", action="store_true", help="Enable debug logging")
    args = parser.parse_args()

    if args.verbose:
        logging.getLogger().setLevel(logging.DEBUG)

    bridge = VizBridge(args.qemu_host, args.qemu_port, args.ws_port, args.http_port,
                        console_port=args.console_port)

    try:
        asyncio.run(bridge.run())
    except KeyboardInterrupt:
        log.info("Shutting down...")


if __name__ == "__main__":
    main()
