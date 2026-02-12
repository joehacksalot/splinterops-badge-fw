"""
Game Heartbeat End-to-End Tests (QEMU OpenCores Ethernet)

Validates:
  - GameState -> HTTPGameClient -> HTTP POST -> mock server -> JSON parse -> GameState update
  - Stone/song state updates from heartbeat response
  - Event join/end from heartbeat response
  - Time sync from server response timestamp
"""
import pytest


@pytest.mark.esp32
@pytest.mark.qemu
def test_heartbeat_request_response(dut, mock_server):
    """Verify full heartbeat request/response cycle."""
    # Wait for first heartbeat to be sent (GameState periodic timer)
    dut.expect("Heartbeat JSON:", timeout=60)
    dut.expect("HTTP Status = 200", timeout=30)
    dut.expect("Heartbeat Response Sent", timeout=5)


@pytest.mark.esp32
@pytest.mark.qemu
def test_heartbeat_updates_game_state(dut, mock_server):
    """Verify heartbeat response updates stone/song state."""
    # Mock server returns stones=[1,3] and songs=[5,2]
    dut.expect("New status received from cloud", timeout=60)
    dut.expect("stoneBits:", timeout=5)
    dut.expect("songUnlockedBits:", timeout=5)


@pytest.mark.esp32
@pytest.mark.qemu
def test_heartbeat_event_join(dut, mock_server):
    """Verify heartbeat with new event triggers GAME_EVENT_JOINED."""
    # Mock server returns a new event ID
    dut.expect("Game event joined notification", timeout=60)


@pytest.mark.esp32
@pytest.mark.qemu
def test_time_sync_from_heartbeat(dut, mock_server):
    """Verify system time is set from heartbeat response."""
    dut.expect("Successfully set the system time", timeout=60)
