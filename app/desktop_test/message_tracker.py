# message_tracker.py – Client-side APRS message state tracker (APP-006)
#
# Mirrors the firmware TxScheduler state machine on the desktop side.
# Receives TX result notifications from the device and updates per-message state.
#
# Wire protocol for TX result characteristic (JSON, UTF-8):
#   {"msg_id":"1","status":"acked"}
#   {"msg_id":"1","status":"timeout"}
#   {"msg_id":"1","status":"tx"}          # intermediate: transmission attempt
#
# Thread safety: not thread-safe; call from asyncio task only.

from __future__ import annotations

import json
from datetime import datetime, timezone
from enum import Enum, auto
from dataclasses import dataclass, field
from typing import Callable, Optional


class MsgState(Enum):
    PENDING  = auto()   # TX request sent; waiting for ack or timeout
    ACKED    = auto()   # Device received ack from remote station  (terminal)
    TIMEOUT  = auto()   # Device gave up retrying                  (terminal)
    ERROR    = auto()   # Protocol/transport error                  (terminal)
    CANCELLED = auto()  # Cancelled by user                         (terminal)

    def is_terminal(self) -> bool:
        return self in (MsgState.ACKED, MsgState.TIMEOUT,
                        MsgState.ERROR, MsgState.CANCELLED)


@dataclass
class TrackedMessage:
    client_id:   int
    msg_id:      str          # Numeric string assigned by firmware
    dest:        str          # "CALLSIGN-SSID" or just "CALLSIGN"
    text:        str
    state:       MsgState     = MsgState.PENDING
    tx_attempts: int          = 0
    queued_at:   datetime     = field(default_factory=lambda: datetime.now(timezone.utc))
    resolved_at: Optional[datetime] = None

    def resolve(self, state: MsgState) -> None:
        if not self.state.is_terminal():
            self.state = state
            self.resolved_at = datetime.now(timezone.utc)


class MessageTracker:
    """Track the lifecycle of outbound APRS messages.

    Usage::

        tracker = MessageTracker(on_update=my_callback)
        client_id = tracker.on_sent("1", "W1AW-9", "Hello")
        # … later, when TX result notify fires:
        tracker.on_tx_result('{"msg_id":"1","status":"acked"}')
    """

    def __init__(
        self,
        on_update: Optional[Callable[[TrackedMessage], None]] = None,
    ) -> None:
        self._messages: dict[str, TrackedMessage] = {}   # keyed by msg_id
        self._by_client: dict[int, str] = {}             # client_id → msg_id
        self._next_client_id: int = 1
        self._on_update = on_update

    # ── Sending ───────────────────────────────────────────────────────────────

    def on_sent(self, msg_id: str, dest: str, text: str) -> int:
        """Register a newly sent message.

        Returns the opaque client_id assigned to this message.
        """
        client_id = self._next_client_id
        self._next_client_id = (self._next_client_id + 1) & 0xFF or 1

        msg = TrackedMessage(
            client_id=client_id,
            msg_id=msg_id,
            dest=dest,
            text=text,
        )
        self._messages[msg_id] = msg
        self._by_client[client_id] = msg_id
        self._notify(msg)
        return client_id

    # ── Result handling ───────────────────────────────────────────────────────

    def on_tx_result(self, json_payload: str) -> Optional[TrackedMessage]:
        """Process a TX result notification from the device.

        Expected JSON: {"msg_id":"<id>","status":"acked"|"timeout"|"tx"|"error"}
        Returns the updated TrackedMessage or None if msg_id not found.
        """
        try:
            data = json.loads(json_payload)
        except json.JSONDecodeError:
            return None

        msg_id = str(data.get("msg_id", ""))
        status = str(data.get("status", "")).lower()

        msg = self._messages.get(msg_id)
        if msg is None:
            # The firmware assigned a new numeric ID; remap the oldest pending
            # local placeholder to this firmware-assigned ID so subsequent
            # result notifications (acked/timeout) can find the message.
            msg = self._remap_placeholder(msg_id)
            if msg is None:
                return None

        if status == "acked":
            msg.resolve(MsgState.ACKED)
        elif status == "timeout":
            msg.resolve(MsgState.TIMEOUT)
        elif status == "error":
            msg.resolve(MsgState.ERROR)
        elif status == "tx":
            msg.tx_attempts += 1
            # Not terminal; notify update only

        self._notify(msg)
        return msg

    # ── Cancellation ──────────────────────────────────────────────────────────

    def cancel(self, client_id: int) -> bool:
        """Cancel a pending message by client_id.

        Returns True if found and not already terminal.
        """
        msg_id = self._by_client.get(client_id)
        if msg_id is None:
            return False
        msg = self._messages.get(msg_id)
        if msg is None or msg.state.is_terminal():
            return False
        msg.resolve(MsgState.CANCELLED)
        self._notify(msg)
        return True

    # ── Inspection ────────────────────────────────────────────────────────────

    def pending(self) -> list[TrackedMessage]:
        """All non-terminal messages, oldest first."""
        return [m for m in self._messages.values()
                if not m.state.is_terminal()]

    def recent(self, n: int = 20) -> list[TrackedMessage]:
        """Most recent n messages (any state), newest first."""
        msgs = list(self._messages.values())
        # Secondary sort by client_id breaks ties when queued_at is identical
        # (common in fast test execution and rapid burst sends).
        msgs.sort(key=lambda m: (m.queued_at, m.client_id), reverse=True)
        return msgs[:n]

    def clear_resolved(self) -> int:
        """Remove all terminal messages.  Returns count removed."""
        resolved_ids = [mid for mid, m in self._messages.items()
                        if m.state.is_terminal()]
        for mid in resolved_ids:
            msg = self._messages.pop(mid, None)
            if msg:
                self._by_client.pop(msg.client_id, None)
        return len(resolved_ids)

    def get_by_msg_id(self, msg_id: str) -> Optional[TrackedMessage]:
        return self._messages.get(msg_id)

    def get_by_client_id(self, client_id: int) -> Optional[TrackedMessage]:
        msg_id = self._by_client.get(client_id)
        return self._messages.get(msg_id) if msg_id else None

    # ── Internal ──────────────────────────────────────────────────────────────

    def _remap_placeholder(self, firmware_msg_id: str) -> Optional[TrackedMessage]:
        """Remap the oldest pending local-placeholder entry to a firmware-assigned ID.

        When a TX request is sent, the message is stored under a ``local:N``
        placeholder because the firmware-assigned ``msg_id`` is not known until
        the first TX result notification arrives.  This method re-keys the
        message so all subsequent lookups succeed.
        """
        placeholder = next(
            (mid for mid, m in self._messages.items()
             if mid.startswith("local:") and not m.state.is_terminal()),
            None,
        )
        if placeholder is None:
            return None
        msg = self._messages.pop(placeholder)
        msg.msg_id = firmware_msg_id
        self._messages[firmware_msg_id] = msg
        self._by_client[msg.client_id] = firmware_msg_id
        return msg

    def _notify(self, msg: TrackedMessage) -> None:
        if self._on_update:
            self._on_update(msg)
