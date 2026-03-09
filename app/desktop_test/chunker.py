# chunker.py – Client-side BLE chunker (INT-002)
#
# Mirrors BleChunker C++ semantics exactly.
#
# Wire format per chunk:
#   [msg_id : 1 byte][chunk_idx : 1 byte][chunk_total : 1 byte][payload ...]
#
# A single-chunk message uses chunk_idx=0, chunk_total=1.
# chunk_total=0 is invalid.
# chunk_idx must be < chunk_total.

from __future__ import annotations

import time
from typing import Callable

HEADER_SIZE = 3
MAX_CHUNKS  = 64


def split(payload: bytes, msg_id: int, chunk_payload_max: int) -> list[bytes]:
    """Split *payload* into wire frames (header + payload slice).

    Parameters
    ----------
    payload:
        Raw application bytes to transmit.
    msg_id:
        Logical message identifier (0-255). The caller is responsible for
        uniqueness within the reassembly timeout window.
    chunk_payload_max:
        Maximum bytes of *application payload* per chunk (i.e. not counting
        the 3-byte chunk header).  For a 23-byte default MTU:
            chunk_payload_max = mtu - ATT_header(3) - chunk_header(3) = 17

    Returns
    -------
    List of complete wire frames ready to pass to write_gatt_char().
    Returns an empty list if payload is empty.

    Raises
    ------
    ValueError
        If the payload would require more than MAX_CHUNKS chunks.
    """
    if not payload or chunk_payload_max <= 0:
        return []

    slices = [
        payload[i : i + chunk_payload_max]
        for i in range(0, len(payload), chunk_payload_max)
    ]
    if len(slices) > MAX_CHUNKS:
        raise ValueError(
            f"Payload requires {len(slices)} chunks; MAX_CHUNKS={MAX_CHUNKS}. "
            "Increase chunk_payload_max or reduce the payload."
        )

    total = len(slices)
    return [bytes([msg_id & 0xFF, idx, total]) + sl for idx, sl in enumerate(slices)]


class Reassembler:
    """Reassemble chunked BLE notifications/indications into full payloads.

    Handles out-of-order delivery, duplicates, and stale-message timeouts.
    Thread-safety: not thread-safe; call from a single async task or protect
    externally.
    """

    def __init__(
        self,
        callback: Callable[[bytes], None],
        timeout_s: float = 5.0,
    ) -> None:
        """
        Parameters
        ----------
        callback:
            Called with the fully reassembled payload when all chunks arrive.
        timeout_s:
            In-progress messages older than this are silently discarded on
            the next feed() call.
        """
        self._cb         = callback
        self._timeout_s  = timeout_s
        # msg_id -> {"chunk_total": int, "chunks": dict[int, bytes], "start": float}
        self._slots: dict[int, dict] = {}

    def feed(self, chunk: bytes) -> bool:
        """Feed one raw wire chunk.

        Returns False if the chunk is malformed (too short, chunk_total=0,
        chunk_idx out of range, or inconsistent chunk_total).
        Returns True otherwise (including duplicates, which are silently ignored).
        Calls *callback* synchronously when the message is complete.
        """
        if len(chunk) < HEADER_SIZE:
            return False

        msg_id      = chunk[0]
        chunk_idx   = chunk[1]
        chunk_total = chunk[2]
        payload     = chunk[HEADER_SIZE:]

        if chunk_total == 0 or chunk_idx >= chunk_total or chunk_total > MAX_CHUNKS:
            return False

        self._expire()

        slot = self._slots.get(msg_id)
        if slot is None:
            self._slots[msg_id] = slot = {
                "chunk_total": chunk_total,
                "chunks":      {},
                "start":       time.monotonic(),
            }
        elif slot["chunk_total"] != chunk_total:
            return False  # inconsistent chunk_total for this msg_id

        if chunk_idx in slot["chunks"]:
            return True  # duplicate – silently ignore

        slot["chunks"][chunk_idx] = payload

        if len(slot["chunks"]) == chunk_total:
            assembled = b"".join(slot["chunks"][i] for i in range(chunk_total))
            del self._slots[msg_id]
            self._cb(assembled)

        return True

    def reset(self) -> None:
        """Discard all in-progress messages (e.g. on disconnect)."""
        self._slots.clear()

    def _expire(self) -> None:
        now = time.monotonic()
        expired = [
            mid for mid, s in self._slots.items()
            if now - s["start"] > self._timeout_s
        ]
        for mid in expired:
            del self._slots[mid]
