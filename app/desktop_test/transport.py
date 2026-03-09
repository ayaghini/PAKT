# transport.py – BLE connection transport layer (APP-001, APP-002)
#
# Implements a bounded reconnect state machine:
#
#   IDLE ──scan──► SCANNING ──done──► IDLE
#   IDLE ──connect──► CONNECTING ──ok──► CONNECTED
#   CONNECTED ──(unexpected drop)──► RECONNECTING ──ok──► CONNECTED
#   RECONNECTING ──(3 failures)──► ERROR
#   CONNECTED / ERROR ──disconnect()──► IDLE
#
# Reconnect policy (architecture contract G):
#   Maximum MAX_RECONNECT_ATTEMPTS attempts, RECONNECT_DELAY_S between each.
#   If all attempts fail, transitions to ERROR and stops retrying.
#   A user-initiated disconnect() always cancels any in-progress reconnect.

from __future__ import annotations

import asyncio
import logging
from enum import Enum, auto
from typing import Callable

from bleak import BleakClient, BleakScanner

logger = logging.getLogger(__name__)

DEVICE_NAME_PREFIX       = "PAKT"
MAX_RECONNECT_ATTEMPTS   = 3
RECONNECT_DELAY_S        = 1.0  # architecture contract G: at least 1 s between attempts


class State(Enum):
    IDLE         = auto()
    SCANNING     = auto()
    CONNECTING   = auto()
    CONNECTED    = auto()
    RECONNECTING = auto()
    ERROR        = auto()


class BleTransport:
    """BLE connection manager for PAKT devices.

    Provides:
    - Filtered scan (PAKT name prefix)
    - Connect / disconnect
    - Automatic reconnect on unexpected disconnect (bounded by MAX_RECONNECT_ATTEMPTS)
    - State-change callbacks for UI updates
    - Access to the underlying BleakClient for GATT operations
    """

    def __init__(
        self,
        on_state: Callable[[State, str], None] | None = None,
        on_reconnected: Callable[[], None] | None = None,
        on_reconnect_failed: Callable[[], None] | None = None,
    ) -> None:
        """
        Parameters
        ----------
        on_state:
            Called on every state transition: (new_state, human_readable_message).
        on_reconnected:
            Called when an automatic reconnect succeeds.  The caller should
            re-subscribe to GATT notifications on this callback.
        on_reconnect_failed:
            Called when all reconnect attempts are exhausted.
        """
        self._on_state           = on_state           or (lambda s, m: None)
        self._on_reconnected     = on_reconnected     or (lambda: None)
        self._on_reconnect_failed= on_reconnect_failed or (lambda: None)

        self._client: BleakClient | None = None
        self._state              = State.IDLE
        self._address: str       = ""
        self._user_disconnected  = False
        self._reconnect_task: asyncio.Task | None = None

    # ── Public API ────────────────────────────────────────────────────────────

    @property
    def state(self) -> State:
        return self._state

    @property
    def is_connected(self) -> bool:
        return (
            self._state == State.CONNECTED
            and self._client is not None
            and self._client.is_connected
        )

    @property
    def client(self) -> BleakClient | None:
        """The underlying BleakClient; None if not connected."""
        return self._client if self.is_connected else None

    @property
    def mtu(self) -> int:
        return self._client.mtu_size if self._client else 23

    @property
    def address(self) -> str:
        return self._address

    async def scan(self, timeout: float = 8.0) -> list[tuple[str, str]]:
        """Scan and return list of (name, address) for PAKT devices."""
        self._set_state(State.SCANNING, f"scanning ({timeout:.0f} s)")
        try:
            devices = await BleakScanner.discover(timeout=timeout)
            found = [
                (d.name or "?", d.address)
                for d in devices
                if d.name and DEVICE_NAME_PREFIX in d.name
            ]
        finally:
            self._set_state(State.IDLE, "scan complete")
        return found

    async def connect(self, address: str) -> None:
        """Connect to *address*.  Raises on failure."""
        if self._reconnect_task and not self._reconnect_task.done():
            self._reconnect_task.cancel()

        self._address           = address
        self._user_disconnected = False
        self._set_state(State.CONNECTING, f"connecting to {address}")

        self._client = BleakClient(
            address, disconnected_callback=self._on_ble_disconnect
        )
        await self._client.connect()
        self._set_state(State.CONNECTED, f"connected (mtu={self._client.mtu_size})")

    async def disconnect(self) -> None:
        """User-initiated disconnect.  Cancels any pending reconnect."""
        self._user_disconnected = True
        if self._reconnect_task and not self._reconnect_task.done():
            self._reconnect_task.cancel()
        if self._client and self._client.is_connected:
            await self._client.disconnect()
        self._client = None
        self._set_state(State.IDLE, "disconnected by user")

    # ── Internals ─────────────────────────────────────────────────────────────

    def _on_ble_disconnect(self, _: BleakClient) -> None:
        """Called by bleak when the connection drops unexpectedly."""
        if self._user_disconnected:
            return
        self._set_state(State.RECONNECTING, "connection lost – reconnecting")
        # Schedule reconnect on the running event loop (bleak calls this from
        # within the event loop's thread context).
        self._reconnect_task = asyncio.ensure_future(self._reconnect())

    async def _reconnect(self) -> None:
        for attempt in range(1, MAX_RECONNECT_ATTEMPTS + 1):
            await asyncio.sleep(RECONNECT_DELAY_S)
            self._set_state(
                State.RECONNECTING,
                f"reconnect attempt {attempt}/{MAX_RECONNECT_ATTEMPTS} → {self._address}",
            )
            try:
                self._client = BleakClient(
                    self._address,
                    disconnected_callback=self._on_ble_disconnect,
                )
                await self._client.connect()
                self._set_state(
                    State.CONNECTED,
                    f"reconnected (mtu={self._client.mtu_size})",
                )
                self._on_reconnected()
                return
            except Exception as exc:
                logger.warning("Reconnect attempt %d failed: %s", attempt, exc)

        self._set_state(
            State.ERROR,
            f"reconnect failed after {MAX_RECONNECT_ATTEMPTS} attempts – device unreachable",
        )
        self._on_reconnect_failed()

    def _set_state(self, state: State, message: str) -> None:
        self._state = state
        self._on_state(state, message)


# ── Auth error classification (APP-008) ──────────────────────────────────────

# Keywords bleak surfaces from OS BLE stacks when the firmware returns
# BLE_ATT_ERR_INSUFFICIENT_AUTHEN.
_AUTH_KEYWORDS = (
    "authentication",
    "authorization",
    "insufficient",
    "access denied",
    "not permitted",
    "0x0005",   # ATT error code 5 = INSUFFICIENT_AUTHENTICATION
    "0x000f",   # ATT error code 15 = INSUFFICIENT_ENCRYPTION
)


def is_auth_error(exc: Exception) -> bool:
    """Return True if *exc* indicates an auth/encryption BLE error."""
    msg = str(exc).lower()
    return any(k in msg for k in _AUTH_KEYWORDS)
