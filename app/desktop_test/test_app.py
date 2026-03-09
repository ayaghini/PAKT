# test_app.py – pytest tests for transport.py and config_store.py
#              (Step 6 / APP-001, APP-002, APP-003, APP-008)
#
# No BLE adapter or bleak runtime required.
# BleakClient and BleakScanner are mocked throughout.
#
# Run: pytest test_app.py -v --tb=short

import asyncio
import json
from pathlib import Path
from unittest.mock import AsyncMock, MagicMock, patch

import pytest

from config_store import ConfigStore
from transport import BleTransport, State, is_auth_error, MAX_RECONNECT_ATTEMPTS

# ── Helpers ───────────────────────────────────────────────────────────────────

def run(coro):
    """Run a coroutine synchronously in a fresh event loop."""
    return asyncio.get_event_loop().run_until_complete(coro)


def _make_client(mtu=247, connected=True):
    c = MagicMock()
    c.is_connected = connected
    c.mtu_size     = mtu
    c.connect      = AsyncMock()
    c.disconnect   = AsyncMock()
    c.address      = "AA:BB:CC:DD:EE:FF"
    return c


class FakeDevice:
    def __init__(self, name, address):
        self.name    = name
        self.address = address


# ── ConfigStore ───────────────────────────────────────────────────────────────

class TestConfigStore:
    def test_save_and_load(self, tmp_path):
        store = ConfigStore(tmp_path / "cfg.json")
        store.save('{"callsign":"N0CALL","ssid":7}', "AA:BB:CC:DD:EE:FF", "read")
        data = store.load()
        assert data["config"]["callsign"] == "N0CALL"
        assert data["config"]["ssid"] == 7
        assert data["device_address"] == "AA:BB:CC:DD:EE:FF"
        assert data["source"] == "read"
        assert "updated_utc" in data

    def test_exists_false_before_save(self, tmp_path):
        assert not ConfigStore(tmp_path / "cfg.json").exists()

    def test_exists_true_after_save(self, tmp_path):
        store = ConfigStore(tmp_path / "cfg.json")
        store.save('{"callsign":"K1ABC"}')
        assert store.exists()

    def test_load_returns_none_when_missing(self, tmp_path):
        assert ConfigStore(tmp_path / "cfg.json").load() is None

    def test_load_returns_none_on_corrupt_file(self, tmp_path):
        path = tmp_path / "cfg.json"
        path.write_text("NOT_JSON", encoding="utf-8")
        assert ConfigStore(path).load() is None

    def test_load_config_returns_dict(self, tmp_path):
        store = ConfigStore(tmp_path / "cfg.json")
        store.save('{"callsign":"VE3XYZ"}')
        cfg = store.load_config()
        assert cfg["callsign"] == "VE3XYZ"

    def test_load_config_returns_none_when_missing(self, tmp_path):
        assert ConfigStore(tmp_path / "cfg.json").load_config() is None

    def test_load_config_json_roundtrips(self, tmp_path):
        store = ConfigStore(tmp_path / "cfg.json")
        store.save('{"callsign":"W1AW","ssid":0}')
        raw = store.load_config_json()
        assert json.loads(raw)["callsign"] == "W1AW"

    def test_load_config_json_returns_none_when_missing(self, tmp_path):
        assert ConfigStore(tmp_path / "cfg.json").load_config_json() is None

    def test_overwrite_on_second_save(self, tmp_path):
        store = ConfigStore(tmp_path / "cfg.json")
        store.save('{"callsign":"K1ABC"}')
        store.save('{"callsign":"W1AW"}')
        assert store.load_config()["callsign"] == "W1AW"

    def test_source_write_is_stored(self, tmp_path):
        store = ConfigStore(tmp_path / "cfg.json")
        store.save('{"callsign":"N0CALL"}', source="write")
        assert store.load()["source"] == "write"

    # ── validate() ────────────────────────────────────────────────────────────

    def test_validate_valid_json(self):
        ok, msg = ConfigStore.validate('{"callsign":"N0CALL"}')
        assert ok
        assert msg == ""

    def test_validate_invalid_json(self):
        ok, msg = ConfigStore.validate("not json")
        assert not ok
        assert "Invalid JSON" in msg

    def test_validate_empty_object(self):
        ok, _ = ConfigStore.validate("{}")
        assert ok

    # ── diff() ────────────────────────────────────────────────────────────────

    def test_diff_no_changes(self):
        j = '{"callsign":"N0CALL","ssid":7}'
        assert ConfigStore.diff(j, j) == []

    def test_diff_changed_value(self):
        old = '{"callsign":"N0CALL","ssid":7}'
        new = '{"callsign":"W1AW","ssid":7}'
        diffs = ConfigStore.diff(old, new)
        assert len(diffs) == 1
        assert "N0CALL" in diffs[0]
        assert "W1AW"   in diffs[0]

    def test_diff_added_key(self):
        old = '{"callsign":"N0CALL"}'
        new = '{"callsign":"N0CALL","ssid":7}'
        diffs = ConfigStore.diff(old, new)
        assert len(diffs) == 1
        assert "ssid" in diffs[0]

    def test_diff_removed_key(self):
        old = '{"callsign":"N0CALL","ssid":7}'
        new = '{"callsign":"N0CALL"}'
        diffs = ConfigStore.diff(old, new)
        assert len(diffs) == 1
        assert "ssid" in diffs[0]

    def test_diff_invalid_json_returns_message(self):
        diffs = ConfigStore.diff("bad", '{"a":1}')
        assert len(diffs) == 1
        assert "cannot diff" in diffs[0]


# ── BleTransport state machine ────────────────────────────────────────────────

class TestBleTransport:
    def test_initial_state_idle(self):
        assert BleTransport().state == State.IDLE

    def test_not_connected_initially(self):
        assert not BleTransport().is_connected

    def test_client_none_initially(self):
        assert BleTransport().client is None

    def test_default_mtu_is_23(self):
        assert BleTransport().mtu == 23

    # ── scan ──────────────────────────────────────────────────────────────────

    def test_scan_returns_to_idle_with_no_results(self):
        t = BleTransport()
        with patch("transport.BleakScanner.discover", AsyncMock(return_value=[])):
            found = run(t.scan())
        assert found == []
        assert t.state == State.IDLE

    def test_scan_filters_non_pakt_devices(self):
        t = BleTransport()
        devices = [FakeDevice("PAKT-TNC", "AA:BB:CC:DD:EE:FF"),
                   FakeDevice("Other",    "11:22:33:44:55:66")]
        with patch("transport.BleakScanner.discover", AsyncMock(return_value=devices)):
            found = run(t.scan())
        assert len(found) == 1
        assert found[0][0] == "PAKT-TNC"

    def test_scan_passes_through_multiple_pakt_devices(self):
        t = BleTransport()
        devices = [FakeDevice("PAKT-A", "AA:BB:CC:DD:EE:01"),
                   FakeDevice("PAKT-B", "AA:BB:CC:DD:EE:02")]
        with patch("transport.BleakScanner.discover", AsyncMock(return_value=devices)):
            found = run(t.scan())
        assert len(found) == 2

    # ── connect ───────────────────────────────────────────────────────────────

    def test_connect_reaches_connected(self):
        states = []
        t = BleTransport(on_state=lambda s, _: states.append(s))
        mock = _make_client()
        with patch("transport.BleakClient", return_value=mock):
            run(t.connect("AA:BB:CC:DD:EE:FF"))
        assert State.CONNECTING in states
        assert State.CONNECTED  in states
        assert t.state == State.CONNECTED

    def test_connect_stores_address(self):
        t = BleTransport()
        mock = _make_client()
        with patch("transport.BleakClient", return_value=mock):
            run(t.connect("AA:BB:CC:DD:EE:FF"))
        assert t.address == "AA:BB:CC:DD:EE:FF"

    def test_connect_exposes_mtu(self):
        t = BleTransport()
        mock = _make_client(mtu=247)
        with patch("transport.BleakClient", return_value=mock):
            run(t.connect("AA:BB:CC:DD:EE:FF"))
        assert t.mtu == 247

    def test_connect_exposes_client(self):
        t = BleTransport()
        mock = _make_client()
        with patch("transport.BleakClient", return_value=mock):
            run(t.connect("AA:BB:CC:DD:EE:FF"))
        assert t.client is mock

    # ── disconnect ────────────────────────────────────────────────────────────

    def test_user_disconnect_returns_to_idle(self):
        t = BleTransport()
        mock = _make_client()
        with patch("transport.BleakClient", return_value=mock):
            run(t.connect("AA:BB:CC:DD:EE:FF"))
            run(t.disconnect())
        assert t.state == State.IDLE
        assert not t.is_connected

    def test_user_disconnect_sets_flag(self):
        t = BleTransport()
        mock = _make_client()
        with patch("transport.BleakClient", return_value=mock):
            run(t.connect("AA:BB:CC:DD:EE:FF"))
            run(t.disconnect())
        assert t._user_disconnected is True

    # ── reconnect ─────────────────────────────────────────────────────────────

    def test_reconnect_success_on_first_attempt(self):
        states = []
        t = BleTransport(on_state=lambda s, _: states.append(s))
        t._address = "AA:BB:CC:DD:EE:FF"
        mock = _make_client()
        with patch("transport.BleakClient", return_value=mock), \
             patch("transport.asyncio.sleep", AsyncMock()):
            run(t._reconnect())
        assert State.CONNECTED in states
        assert State.ERROR not in states

    def test_reconnect_all_attempts_fail_reaches_error(self):
        states = []
        t = BleTransport(on_state=lambda s, _: states.append(s))
        t._address = "AA:BB:CC:DD:EE:FF"
        mock = _make_client()
        mock.connect = AsyncMock(side_effect=Exception("timeout"))
        with patch("transport.BleakClient", return_value=mock), \
             patch("transport.asyncio.sleep", AsyncMock()):
            run(t._reconnect())
        assert State.ERROR in states
        assert State.CONNECTED not in states

    def test_reconnect_succeeds_on_second_attempt(self):
        states = []
        t = BleTransport(on_state=lambda s, _: states.append(s))
        t._address = "AA:BB:CC:DD:EE:FF"
        attempts = [0]

        async def flaky_connect():
            attempts[0] += 1
            if attempts[0] < 2:
                raise Exception("not yet")

        mock = _make_client()
        mock.connect = AsyncMock(side_effect=flaky_connect)
        with patch("transport.BleakClient", return_value=mock), \
             patch("transport.asyncio.sleep", AsyncMock()):
            run(t._reconnect())
        assert State.CONNECTED in states
        assert State.ERROR not in states
        assert attempts[0] == 2

    def test_reconnect_calls_exactly_max_attempts_on_failure(self):
        t = BleTransport()
        t._address = "AA:BB:CC:DD:EE:FF"
        mock = _make_client()
        mock.connect = AsyncMock(side_effect=Exception("fail"))
        with patch("transport.BleakClient", return_value=mock), \
             patch("transport.asyncio.sleep", AsyncMock()):
            run(t._reconnect())
        assert mock.connect.call_count == MAX_RECONNECT_ATTEMPTS

    def test_reconnect_fires_on_reconnected_callback(self):
        cb_called = [False]
        t = BleTransport(on_reconnected=lambda: cb_called.__setitem__(0, True))
        t._address = "AA:BB:CC:DD:EE:FF"
        mock = _make_client()
        with patch("transport.BleakClient", return_value=mock), \
             patch("transport.asyncio.sleep", AsyncMock()):
            run(t._reconnect())
        assert cb_called[0]

    def test_reconnect_fires_on_reconnect_failed_callback(self):
        cb_called = [False]
        t = BleTransport(on_reconnect_failed=lambda: cb_called.__setitem__(0, True))
        t._address = "AA:BB:CC:DD:EE:FF"
        mock = _make_client()
        mock.connect = AsyncMock(side_effect=Exception("fail"))
        with patch("transport.BleakClient", return_value=mock), \
             patch("transport.asyncio.sleep", AsyncMock()):
            run(t._reconnect())
        assert cb_called[0]

    def test_user_disconnect_flag_prevents_ble_disconnect_callback(self):
        """If _user_disconnected is True, _on_ble_disconnect should be a no-op."""
        reconnect_started = [False]

        async def fake_reconnect(self):
            reconnect_started[0] = True

        t = BleTransport()
        t._user_disconnected = True
        mock_client = _make_client()
        t._on_ble_disconnect(mock_client)
        # Reconnect must not have been triggered.
        assert not reconnect_started[0]
        assert t.state == State.IDLE  # state unchanged


# ── is_auth_error() (APP-008) ─────────────────────────────────────────────────

class TestIsAuthError:
    def test_authentication_keyword(self):
        assert is_auth_error(Exception("Authentication failed"))

    def test_insufficient_keyword(self):
        assert is_auth_error(Exception("Insufficient encryption (0x000f)"))

    def test_access_denied_keyword(self):
        assert is_auth_error(Exception("Access denied"))

    def test_att_error_code_5(self):
        assert is_auth_error(Exception("ATT error 0x0005"))

    def test_att_error_code_15(self):
        assert is_auth_error(Exception("ATT error 0x000f"))

    def test_non_auth_error_returns_false(self):
        assert not is_auth_error(Exception("Connection timeout"))

    def test_case_insensitive(self):
        assert is_auth_error(Exception("AUTHENTICATION REQUIRED"))

    def test_generic_ble_error_returns_false(self):
        assert not is_auth_error(Exception("Device not found"))
