# tests/conftest.py
import pytest
import asyncio
import subprocess
import time
import sys
import os

# Add project root and tests dir to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
sys.path.insert(0, os.path.dirname(__file__))


@pytest.fixture(autouse=True)
def qemu_config(request):
    """Configure QEMU-specific test settings."""
    pass


@pytest.fixture(scope="session")
async def viz_client(event_loop):
    """
    Client for the QEMU visualization transport (UART2 TCP).
    Connects directly to QEMU's UART2 TCP port for injecting touch
    events and reading LED frames in tests.
    """
    from test_touch_led import QemuVizClient

    client = QemuVizClient(host="localhost", port=1235)
    try:
        await client.connect(timeout=10.0)
    except Exception as e:
        pytest.skip(f"Cannot connect to QEMU viz port: {e}")
    yield client
    await client.close()


@pytest.fixture(scope="session")
def bumble_link():
    """
    Shared virtual radio medium for all BLE tests.
    All virtual controllers and peer devices attach to this link,
    simulating a shared RF environment.
    """
    from bumble.link import LocalLink
    return LocalLink()


@pytest.fixture(scope="session")
def event_loop():
    """Session-scoped event loop for async Bumble operations."""
    loop = asyncio.new_event_loop()
    yield loop
    loop.close()


@pytest.fixture(scope="session")
def bumble_controller(bumble_link):
    """
    Virtual BLE controller connected to QEMU's HCI port.

    NOTE: This fixture is intended for in-process test setups where no
    standalone ble_virtual_controller.py is running.  When using
    tools/run_ble_tests.sh, the standalone controller process handles the
    HCI bridge and this fixture is NOT needed (tests just use bumble_link
    to attach peer devices to the same virtual radio).

    Not autouse — only injected into tests that explicitly request it.
    """
    from bumble.controller import Controller
    from bumble.transport import open_transport

    transport = None
    controller = None

    async def _setup():
        nonlocal transport, controller
        try:
            transport = await open_transport("tcp-client:localhost:1234")
            controller = Controller(
                "qemu-badge",
                host_source=transport.source,
                host_sink=transport.sink,
                link=bumble_link,
            )
        except Exception:
            # If QEMU HCI port is not available (e.g. standalone controller
            # is handling it), this is expected — skip gracefully.
            pass

    async def _teardown():
        nonlocal transport
        if transport:
            try:
                await transport.close()
            except Exception:
                pass

    loop = asyncio.get_event_loop()
    loop.run_until_complete(_setup())
    yield controller
    loop.run_until_complete(_teardown())


@pytest.fixture(scope="session")
def mock_server():
    """Start mock game server for WiFi emulation tests."""
    project_root = os.path.join(os.path.dirname(__file__), "..")
    proc = subprocess.Popen(
        [sys.executable, os.path.join(project_root, "tools", "mock_game_server.py"), "--port", "9080"],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE
    )
    time.sleep(1)  # Wait for server to start
    yield proc
    proc.terminate()
    proc.wait()


@pytest.fixture(scope="session")
def mock_server_with_ota():
    """Start mock game server with OTA response for WiFi emulation tests."""
    project_root = os.path.join(os.path.dirname(__file__), "..")
    response_file = os.path.join(project_root, "tools", "mock_responses", "heartbeat_event_complete.json")
    proc = subprocess.Popen(
        [sys.executable, os.path.join(project_root, "tools", "mock_game_server.py"),
         "--port", "9080", "--response-file", response_file],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE
    )
    time.sleep(1)  # Wait for server to start
    yield proc
    proc.terminate()
    proc.wait()
