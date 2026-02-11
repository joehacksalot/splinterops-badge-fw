# tests/conftest.py
import pytest
import asyncio
import sys
import os

# Add project root to path so tools/ is importable
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))


@pytest.fixture(autouse=True)
def qemu_config(request):
    """Configure QEMU-specific test settings."""
    pass


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
