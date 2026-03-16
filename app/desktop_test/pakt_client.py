# pakt_client.py – PAKT APRS TNC GATT service layer (APP-001, APP-002, APP-003)
#
# Layered on top of BleTransport (connection/reconnect) and ConfigStore
# (local config persistence).  Handles GATT reads, chunked writes, and
# notification subscriptions with transparent reassembly.
#
# Architecture:
#   main.py
#     └── PaktClient (this file)
#           ├── BleTransport   – scan / connect / reconnect FSM
#           ├── ConfigStore    – local config cache
#           └── chunker        – payload split / reassemble

from __future__ import annotations

import asyncio
from pathlib import Path
from typing import Callable

from bleak.backends.characteristic import BleakGATTCharacteristic

from capability import CapabilityNegotiator, DeviceCapabilities
from chunker import split, Reassembler
from config_store import ConfigStore
from diagnostics import DiagnosticsStore
from message_tracker import MessageTracker, TrackedMessage
from telemetry import parse_notify
from transport import BleTransport, State, is_auth_error

# ── UUID map ──────────────────────────────────────────────────────────────────
# Rule: 544e4332-8a48-4328-9844-3f5c{id:08x}

_B = "544e4332-8a48-4328-9844-3f5c{:08x}"

UUID_DEV_CONFIG  = _B.format(0xA0010000)
UUID_DEV_COMMAND = _B.format(0xA0020000)
UUID_DEV_STATUS  = _B.format(0xA0030000)
UUID_DEV_CAPS    = _B.format(0xA0040000)
UUID_RX_PACKET   = _B.format(0xA0100000)
UUID_TX_REQUEST  = _B.format(0xA0110000)
UUID_TX_RESULT   = _B.format(0xA0120000)
UUID_GPS_TELEM   = _B.format(0xA0210000)
UUID_POWER_TELEM = _B.format(0xA0220000)
UUID_SYS_TELEM   = _B.format(0xA0230000)

# Standard DIS UUIDs (Bluetooth SIG 16-bit)
UUID_MANUFACTURER = "00002a29-0000-1000-8000-00805f9b34fb"
UUID_MODEL_NUM    = "00002a24-0000-1000-8000-00805f9b34fb"
UUID_FW_REV       = "00002a26-0000-1000-8000-00805f9b34fb"

NOTIFY_CHARS: dict[str, str] = {
    "device_status": UUID_DEV_STATUS,
    "rx_packet":     UUID_RX_PACKET,
    "tx_result":     UUID_TX_RESULT,
    "gps_telem":     UUID_GPS_TELEM,
    "power_telem":   UUID_POWER_TELEM,
    "system_telem":  UUID_SYS_TELEM,
}


class PaktClient:
    """Async GATT client for PAKT APRS TNC.

    Sits above BleTransport (connection management) and exposes high-level
    operations: config read/write, commands, TX requests, and notifications.
    Config reads are automatically cached to disk via ConfigStore.
    """

    def __init__(
        self,
        on_event: Callable[[str, str, str], None],
        config_cache_path: Path = Path("pakt_config_cache.json"),
    ) -> None:
        self._log          = on_event
        self._config_store = ConfigStore(config_cache_path)
        self._msg_id: int  = 0
        self._reassemblers: dict[str, Reassembler] = {}
        self._msg_tracker  = MessageTracker(on_update=self._on_msg_update)
        self._diagnostics  = DiagnosticsStore()
        self._cap_neg      = CapabilityNegotiator(on_caps=self._on_caps)

        self._transport = BleTransport(
            on_state          = self._on_transport_state,
            on_reconnected    = lambda: asyncio.ensure_future(self._resubscribe()),
            on_reconnect_failed = lambda: self._log(
                "ERROR", "transport", "reconnect exhausted – manual reconnect required"
            ),
        )

    # ── Connection API ────────────────────────────────────────────────────────

    @property
    def is_connected(self) -> bool:
        return self._transport.is_connected

    @property
    def state(self) -> State:
        return self._transport.state

    @property
    def mtu(self) -> int:
        return self._transport.mtu

    async def scan(self, timeout: float = 8.0) -> list[tuple[str, str]]:
        self._log("SCAN", "start", f"timeout={timeout:.0f}s")
        found = await self._transport.scan(timeout)
        self._log("SCAN", "done", f"{len(found)} PAKT device(s) found")
        return found

    async def connect(self, address: str) -> None:
        await self._transport.connect(address)
        self._log("CONNECT", address, f"mtu={self.mtu}")
        # Read capability record before subscribing so feature flags are
        # available during _subscribe_all.
        client = self._transport.client
        if client:
            await self._cap_neg.read(client)
        await self._subscribe_all()

    async def disconnect(self) -> None:
        await self._transport.disconnect()
        for r in self._reassemblers.values():
            r.reset()
        self._reassemblers.clear()
        self._cap_neg.reset()

    # ── GATT reads ────────────────────────────────────────────────────────────

    async def read_device_info(self) -> dict[str, str]:
        result: dict[str, str] = {}
        for name, uuid in [
            ("manufacturer", UUID_MANUFACTURER),
            ("model",        UUID_MODEL_NUM),
            ("firmware_rev", UUID_FW_REV),
        ]:
            try:
                client = self._transport.client
                if client is None:
                    break
                raw  = await client.read_gatt_char(uuid)
                text = raw.decode("utf-8", errors="replace")
                result[name] = text
                self._log("READ", name, text)
            except Exception as exc:
                self._log("READ_ERR", name, str(exc))
        return result

    async def read_config(self) -> str:
        """Read Device Config, cache it locally, and return as UTF-8 string."""
        client = self._transport.client
        if client is None:
            raise RuntimeError("Not connected")
        raw  = await client.read_gatt_char(UUID_DEV_CONFIG)
        text = raw.decode("utf-8", errors="replace")
        self._log("READ", "device_config", text)
        # Auto-cache (APP-003)
        self._config_store.save(text, self._transport.address, source="read")
        return text

    # ── GATT writes ───────────────────────────────────────────────────────────

    async def write_config(self, json_str: str) -> None:
        """Write Device Config (chunked, with response).

        Raises RuntimeError if not connected.
        Raises PermissionError with pairing guidance if the link is not bonded.
        """
        await self._write_chunked(UUID_DEV_CONFIG, "device_config",
                                  json_str.encode(), response=True)
        # Cache the written config as the new authoritative local copy.
        self._config_store.save(json_str, self._transport.address, source="write")

    async def send_command(self, json_str: str) -> None:
        """Write Device Command (≤64 B, write-without-response)."""
        client = self._transport.client
        if client is None:
            raise RuntimeError("Not connected")
        try:
            await client.write_gatt_char(UUID_DEV_COMMAND,
                                         json_str.encode(), response=False)
            self._log("WRITE", "device_command", json_str)
        except Exception as exc:
            self._handle_write_error("device_command", exc)
            raise

    async def send_tx_request(self, json_str: str) -> int:
        """Write TX Request (chunked, with response).

        Parses 'dest' and 'text' fields from json_str to register the message
        with MessageTracker. Legacy payloads using 'to' are accepted as a
        compatibility fallback. Returns the opaque client_id for cancellation.
        Raises RuntimeError if not connected or json_str is invalid JSON.
        """
        import json as _json
        try:
            data = _json.loads(json_str)
        except _json.JSONDecodeError as exc:
            raise RuntimeError(f"Invalid TX request JSON: {exc}") from exc

        dest = str(data.get("dest", data.get("to", "")))
        text = str(data.get("text", ""))

        await self._write_chunked(UUID_TX_REQUEST, "tx_request",
                                  json_str.encode(), response=True)

        # The firmware echoes the msg_id it assigned in the first TX result
        # notify.  Until then, use a local placeholder key.
        # We register after the write succeeds so failed writes don't pollute
        # the tracker.
        local_id = f"local:{self._msg_id}"
        client_id = self._msg_tracker.on_sent(local_id, dest, text)
        return client_id

    def cancel_message(self, client_id: int) -> bool:
        """Cancel a pending outbound message by client_id."""
        return self._msg_tracker.cancel(client_id)

    @property
    def message_tracker(self) -> MessageTracker:
        return self._msg_tracker

    @property
    def diagnostics(self) -> DiagnosticsStore:
        return self._diagnostics

    @property
    def capabilities(self) -> DeviceCapabilities:
        return self._cap_neg.caps

    # ── Notifications ─────────────────────────────────────────────────────────

    async def listen(self, duration_s: float = 30.0) -> None:
        await asyncio.sleep(duration_s)

    # ── Config store pass-throughs ────────────────────────────────────────────

    @property
    def config_store(self) -> ConfigStore:
        return self._config_store

    # ── Internals ─────────────────────────────────────────────────────────────

    async def _subscribe_all(self) -> None:
        client = self._transport.client
        if client is None:
            return
        for name, uuid in NOTIFY_CHARS.items():
            try:
                _name = name
                self._reassemblers[uuid] = Reassembler(
                    lambda data, n=_name: self._on_reassembled(n, data)
                )
                await client.start_notify(uuid, self._on_notify)
                self._log("SUBSCRIBE", name, "ok")
            except Exception as exc:
                self._log("SUBSCRIBE_ERR", name, str(exc))

    async def _resubscribe(self) -> None:
        """Re-register notification handlers after a successful reconnect."""
        self._log("RECONNECT", "ok", f"mtu={self.mtu} – re-subscribing")
        for r in self._reassemblers.values():
            r.reset()
        self._reassemblers.clear()
        await self._subscribe_all()

    def _on_notify(self, char: BleakGATTCharacteristic, data: bytearray) -> None:
        uuid = char.uuid.lower()
        r = self._reassemblers.get(uuid)
        if r:
            r.feed(bytes(data))
        else:
            name = next((n for n, u in NOTIFY_CHARS.items() if u == uuid), uuid)
            self._on_reassembled(name, bytes(data))

    def _on_reassembled(self, name: str, data: bytes) -> None:
        try:
            text = data.decode("utf-8", errors="replace")
        except Exception:
            text = data.hex()
        self._log("NOTIFY", name, text)
        # Route TX result notifications into the message tracker.
        if name == "tx_result":
            self._msg_tracker.on_tx_result(text)
        # Route RX packet frames into diagnostics.
        elif name == "rx_packet":
            self._diagnostics.add_rx_frame(text)
        # Route all telemetry notifies into diagnostics store.
        telem_obj = parse_notify(name, text)
        if telem_obj is not None:
            self._diagnostics.ingest(name, telem_obj)

    def _on_caps(self, caps: DeviceCapabilities) -> None:
        tag = f"protocol={caps.protocol}  fw={caps.fw_ver}  hw={caps.hw_rev}  [{caps.source}]"
        if not caps.is_compatible():
            missing = caps.missing_mvp_features()
            self._log("CAPS_WARN", "capability",
                      f"Missing MVP features: {missing}  — {tag}")
        else:
            self._log("CAPS", "capability", tag)

    def _on_msg_update(self, msg: TrackedMessage) -> None:
        state_str = msg.state.name
        detail    = f"msg_id={msg.msg_id} dest={msg.dest} attempts={msg.tx_attempts}"
        self._log("MSG_STATE", state_str, detail)

    def _on_transport_state(self, state: State, message: str) -> None:
        self._log("TRANSPORT", state.name, message)

    def _next_msg_id(self) -> int:
        self._msg_id = (self._msg_id + 1) & 0xFF
        return self._msg_id

    async def _write_chunked(
        self, uuid: str, name: str, payload: bytes, response: bool
    ) -> None:
        client = self._transport.client
        if client is None:
            raise RuntimeError("Not connected")
        chunk_payload_max = max(1, self.mtu - 3 - 3)
        chunks = split(payload, self._next_msg_id(), chunk_payload_max)
        self._log(
            "WRITE", name,
            f"{len(payload)} B → {len(chunks)} chunk(s) "
            f"(mtu={self.mtu}, payload/chunk={chunk_payload_max})",
        )
        try:
            for chunk in chunks:
                await client.write_gatt_char(uuid, chunk, response=response)
        except Exception as exc:
            self._handle_write_error(name, exc)
            raise

    def _handle_write_error(self, name: str, exc: Exception) -> None:
        if is_auth_error(exc):
            self._log(
                "AUTH_ERR", name,
                "Write rejected: link not encrypted+bonded. "
                "Pair the device via OS Bluetooth settings, then retry.",
            )
        else:
            self._log("WRITE_ERR", name, str(exc))
