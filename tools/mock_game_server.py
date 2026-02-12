#!/usr/bin/env python3
"""
Mock game server for QEMU WiFi emulation testing.
Provides /heartbeat endpoint with configurable responses.

Supports concurrent multi-badge use:
  - ThreadingHTTPServer handles parallel requests
  - Per-request deep copies prevent shared-state mutation
  - Badge registry tracks connected badges for sibling reporting
  - Thread-safe access to all shared state
"""
import copy
import json
import os
import threading
import time
import argparse
from http.server import HTTPServer, BaseHTTPRequestHandler
from socketserver import ThreadingMixIn


def deep_merge(base, override):
    """Recursively merge `override` into `base` (mutates base)."""
    for key, value in override.items():
        if key in base and isinstance(base[key], dict) and isinstance(value, dict):
            deep_merge(base[key], value)
        else:
            base[key] = value
    return base


DEFAULT_RESPONSE = {
    "stones": [1, 3],
    "songs": [5, 2],
    "event": {
        "event": "AAAAAAAAAAA=",
        "stoneColor": 0,
        "power": 0,
        "msRemaining": 0
    },
    "badgeRequestTime": 0,
    "serverResponseTime": {
        "tv_sec": int(time.time()),
        "tv_nsec": 0
    },
    "siblings": []
}

# How long (seconds) before a badge is considered stale and removed from the registry
BADGE_STALE_TIMEOUT = 120


class BadgeRegistry:
    """Thread-safe registry of badges that have sent heartbeats."""

    def __init__(self):
        self._lock = threading.Lock()
        self._badges = {}  # uuid -> {"last_seen": float, "event": str}

    def update(self, uuid, event_uuid=""):
        """Record a heartbeat from a badge."""
        with self._lock:
            self._badges[uuid] = {
                "last_seen": time.time(),
                "event": event_uuid,
            }
            self._prune_stale()

    def get_siblings(self, exclude_uuid):
        """Return list of other badge UUIDs seen recently."""
        with self._lock:
            self._prune_stale()
            return [uid for uid in self._badges if uid != exclude_uuid]

    def _prune_stale(self):
        """Remove badges not seen within the timeout. Caller must hold lock."""
        now = time.time()
        stale = [uid for uid, info in self._badges.items()
                 if now - info["last_seen"] > BADGE_STALE_TIMEOUT]
        for uid in stale:
            del self._badges[uid]

    def count(self):
        with self._lock:
            self._prune_stale()
            return len(self._badges)


class ThreadingHTTPServer(ThreadingMixIn, HTTPServer):
    """HTTPServer that handles each request in a new thread."""
    daemon_threads = True


class GameServerHandler(BaseHTTPRequestHandler):
    # Set by main() before server starts — treated as read-only template
    response_template = DEFAULT_RESPONSE
    badge_registry = BadgeRegistry()
    ota_enabled = False
    ota_binary_path = None
    ota_dummy_size = 0  # bytes for dummy OTA payload
    last_heartbeat_time = None
    _lock = threading.Lock()

    def _send_cors_headers(self):
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")

    def _json_response(self, data, status=200):
        body = json.dumps(data).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self._send_cors_headers()
        self.end_headers()
        self.wfile.write(body)

    def _read_body(self):
        content_length = int(self.headers.get("Content-Length", 0))
        return self.rfile.read(content_length)

    def do_OPTIONS(self):
        """Handle CORS preflight requests."""
        self.send_response(204)
        self._send_cors_headers()
        self.end_headers()

    def do_POST(self):
        if self.path == "/heartbeat":
            self._handle_heartbeat()
        elif self.path == "/admin/config":
            self._handle_admin_config_set()
        elif self.path == "/admin/config/patch":
            self._handle_admin_config_patch()
        elif self.path == "/admin/peers":
            self._handle_admin_peers()
        elif self.path == "/admin/ota/enable":
            self._handle_admin_ota_enable()
        elif self.path == "/admin/ota/disable":
            self._handle_admin_ota_disable()
        elif self.path == "/admin/trigger/heartbeat":
            self._handle_admin_trigger_heartbeat()
        else:
            self.send_response(404)
            self._send_cors_headers()
            self.end_headers()

    def _handle_heartbeat(self):
        body = self._read_body()

        # Parse request
        badge_uuid = "unknown"
        badge_request_time = 0
        enrolled_event = ""
        try:
            req = json.loads(body)
            badge_uuid = req.get("uuid", "unknown")
            badge_request_time = req.get("badgeRequestTime", 0)
            enrolled_event = req.get("enrolledEvent", "")
        except json.JSONDecodeError:
            pass

        print(f"[heartbeat] badge={badge_uuid} "
              f"body={body.decode('utf-8', errors='replace')[:200]}")

        # Register this badge and get siblings
        self.badge_registry.update(badge_uuid, enrolled_event)
        siblings = self.badge_registry.get_siblings(badge_uuid)

        # Build per-request response (deep copy — no shared mutation)
        with self._lock:
            resp = copy.deepcopy(self.response_template)
        resp["badgeRequestTime"] = badge_request_time
        resp["serverResponseTime"]["tv_sec"] = int(time.time())
        # Merge auto-discovered siblings with template siblings
        template_siblings = resp.get("siblings", [])
        all_siblings = list(set(siblings + template_siblings))
        resp["siblings"] = all_siblings

        GameServerHandler.last_heartbeat_time = time.time()

        response = json.dumps(resp).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(response)))
        self._send_cors_headers()
        self.end_headers()
        self.wfile.write(response)

        print(f"[heartbeat] -> badge={badge_uuid} "
              f"siblings={all_siblings} "
              f"badges_online={self.badge_registry.count()}")

    def _handle_admin_config_set(self):
        body = self._read_body()
        try:
            with self._lock:
                GameServerHandler.response_template = json.loads(body)
            self._json_response({"status": "ok"})
        except json.JSONDecodeError as e:
            self._json_response({"status": "error", "message": str(e)}, 400)

    def _handle_admin_config_patch(self):
        body = self._read_body()
        try:
            patch = json.loads(body)
            with self._lock:
                deep_merge(GameServerHandler.response_template, patch)
                config = copy.deepcopy(GameServerHandler.response_template)
            self._json_response({"status": "ok", "config": config})
        except json.JSONDecodeError as e:
            self._json_response({"status": "error", "message": str(e)}, 400)

    def _handle_admin_peers(self):
        body = self._read_body()
        try:
            data = json.loads(body)
            with self._lock:
                GameServerHandler.response_template["siblings"] = data.get("siblings", [])
            self._json_response({"status": "ok"})
        except json.JSONDecodeError as e:
            self._json_response({"status": "error", "message": str(e)}, 400)

    def _handle_admin_ota_enable(self):
        body = self._read_body()
        try:
            data = json.loads(body)
            GameServerHandler.ota_enabled = True
            GameServerHandler.ota_binary_path = data.get("binary_path", None)
            GameServerHandler.ota_dummy_size = data.get("dummy_size", 0)
            print(f"[admin] OTA enabled: binary_path={GameServerHandler.ota_binary_path} "
                  f"dummy_size={GameServerHandler.ota_dummy_size}")
            self._json_response({"status": "ok", "ota_enabled": True})
        except json.JSONDecodeError as e:
            self._json_response({"status": "error", "message": str(e)}, 400)

    def _handle_admin_ota_disable(self):
        GameServerHandler.ota_enabled = False
        GameServerHandler.ota_binary_path = None
        GameServerHandler.ota_dummy_size = 0
        print("[admin] OTA disabled")
        self._json_response({"status": "ok", "ota_enabled": False})

    def _handle_admin_trigger_heartbeat(self):
        # This is a signal for the viz UI — the actual relay happens
        # via WebSocket → viz_bridge → QEMU UART
        print("[admin] Heartbeat trigger requested")
        self._json_response({"status": "ok", "note": "trigger relayed"})

    def do_GET(self):
        if self.path.startswith("/update"):
            self._handle_ota_get()
        elif self.path == "/admin/config":
            with self._lock:
                config = copy.deepcopy(GameServerHandler.response_template)
            self._json_response(config)
        elif self.path == "/admin/state":
            self._handle_admin_state()
        else:
            self.send_response(404)
            self._send_cors_headers()
            self.end_headers()

    def _handle_ota_get(self):
        if not GameServerHandler.ota_enabled:
            self.send_response(404)
            self._send_cors_headers()
            self.end_headers()
            return

        # Serve real binary or dummy payload
        if GameServerHandler.ota_binary_path and os.path.isfile(GameServerHandler.ota_binary_path):
            with open(GameServerHandler.ota_binary_path, "rb") as f:
                data = f.read()
        elif GameServerHandler.ota_dummy_size > 0:
            data = b'\xff' * GameServerHandler.ota_dummy_size
        else:
            data = b'\xff' * (1024 * 1024)  # Default 1MB dummy

        self.send_response(200)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Content-Length", str(len(data)))
        self._send_cors_headers()
        self.end_headers()
        self.wfile.write(data)
        print(f"[ota] Served {len(data)} bytes")

    def _handle_admin_state(self):
        with self._lock:
            config = copy.deepcopy(GameServerHandler.response_template)
        state = {
            "response_template": config,
            "badges_registered": self.badge_registry.count(),
            "last_heartbeat_time": GameServerHandler.last_heartbeat_time,
            "ota_enabled": GameServerHandler.ota_enabled,
            "ota_binary_path": GameServerHandler.ota_binary_path,
            "ota_dummy_size": GameServerHandler.ota_dummy_size,
        }
        self._json_response(state)

    def log_message(self, format, *args):
        """Override to prefix with thread name for debugging concurrency."""
        print(f"[{threading.current_thread().name}] {self.address_string()} - {format % args}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Mock game server for QEMU WiFi emulation testing")
    parser.add_argument("--port", type=int, default=9080, help="Port to listen on")
    parser.add_argument("--response-file", help="JSON file with custom response template")
    args = parser.parse_args()

    if args.response_file:
        with open(args.response_file) as f:
            GameServerHandler.response_template = json.load(f)

    server = ThreadingHTTPServer(("0.0.0.0", args.port), GameServerHandler)
    print(f"Mock game server running on port {args.port} (threaded, multi-badge)")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down mock server")
        server.shutdown()
