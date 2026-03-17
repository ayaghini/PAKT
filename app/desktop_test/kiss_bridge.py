# kiss_bridge.py – KISS-over-BLE desktop bridge and test harness (APP-013, INT-003)
#
# Two roles in one file:
#
#   KissPacketizer  – framing only; pure Python, no BLE.
#                     Handles FEND-delimited KISS packet boundaries from a
#                     byte stream (e.g. virtual COM port or pipe).
#
#   KissBridge      – asyncio bridge that:
#                     * connects to a PAKT device over BLE
#                     * forwards serial KISS frames to the KISS TX characteristic
#                     * subscribes to KISS RX characteristic and delivers frames
#                       to the serial side
#
# Standalone harness usage (no serial port):
#   python kiss_bridge.py --scan            – find PAKT devices
#   python kiss_bridge.py --test            – run a self-contained loopback test
#
# Bridge usage (requires device + serial KISS client such as APRSdroid/Xastir):
#   python kiss_bridge.py --port COM3       – bridge COM3 to first PAKT found
#   python kiss_bridge.py --port /dev/ttyS0 --device PAKT-TNC
#
# UUIDs (frozen in docs/16_kiss_over_ble_spec.md §3.3):
#   KISS Service: 544e4332-8a48-4328-9844-3f5ca0500000
#   KISS RX:      544e4332-8a48-4328-9844-3f5ca0510000  (notify, Device→App)
#   KISS TX:      544e4332-8a48-4328-9844-3f5ca0520000  (write, App→Device)
#
# KISS TX requires encrypted + bonded link (pair with device first).
#
# Chunking: KISS frames are chunked using the INT-002 BleChunker protocol
#   (3-byte header: msg_id | chunk_idx | chunk_total) before writing to KISS TX.
#
# Thread safety: not thread-safe beyond the asyncio event loop; run from a
#   single asyncio task.
#
# Wire format reference: docs/16_kiss_over_ble_spec.md

from __future__ import annotations

import asyncio
import logging
import struct
from dataclasses import dataclass, field
from typing import Callable, List, Optional

logger = logging.getLogger("kiss_bridge")

# ── KISS framing constants ────────────────────────────────────────────────────

FEND = 0xC0   # Frame End / delimiter
FESC = 0xDB   # Frame Escape
TFEND = 0xDC  # Transposed FEND (after FESC)
TFESC = 0xDD  # Transposed FESC (after FESC)

CMD_DATA = 0x00          # Data frame, port 0
CMD_RETURN_FROM_KISS = 0x0F

KISS_MAX_FRAME = 330     # MVP logical KISS frame size limit (after reassembly)

# BLE UUIDs (canonical lowercase)
UUID_KISS_SERVICE = "544e4332-8a48-4328-9844-3f5ca0500000"
UUID_KISS_RX      = "544e4332-8a48-4328-9844-3f5ca0510000"
UUID_KISS_TX      = "544e4332-8a48-4328-9844-3f5ca0520000"

# BleChunker header size (INT-002)
CHUNK_HEADER_SIZE = 3   # msg_id | chunk_idx | chunk_total


# ── KissPacketizer ────────────────────────────────────────────────────────────

class KissPacketizer:
    """Encode and decode KISS frames from/to raw byte streams.

    This is the serial-side framing layer. It is pure Python with no BLE
    dependency and can be unit-tested without hardware.

    Encode path (serial → BLE direction):
        encode(ax25_bytes) → KISS frame bytes suitable for BLE transmission.

    Decode path (BLE → serial direction):
        feed(byte_or_bytes) → deliver complete frames via on_frame callback.
    """

    def __init__(self, on_frame: Optional[Callable[[bytes], None]] = None) -> None:
        self._on_frame = on_frame
        self._buf: bytearray = bytearray()
        self._in_frame: bool = False
        self._error_count: int = 0

    @property
    def error_count(self) -> int:
        return self._error_count

    # ── encode ────────────────────────────────────────────────────────────────

    @staticmethod
    def encode(ax25_bytes: bytes) -> bytes:
        """Wrap `ax25_bytes` in a KISS port-0 data frame.

        Output format: FEND 0x00 <escaped AX.25 bytes> FEND

        Returns raw bytes suitable for sending to the KISS TX BLE characteristic
        (after chunking by the bridge layer).
        """
        out = bytearray()
        out.append(FEND)
        out.append(CMD_DATA)
        for b in ax25_bytes:
            if b == FEND:
                out.append(FESC)
                out.append(TFEND)
            elif b == FESC:
                out.append(FESC)
                out.append(TFESC)
            else:
                out.append(b)
        out.append(FEND)
        return bytes(out)

    @staticmethod
    def decode(kiss_frame: bytes) -> Optional[bytes]:
        """Decode a complete KISS frame into the inner AX.25 payload.

        `kiss_frame` may include or omit surrounding FEND bytes.

        Returns:
            bytes   – the AX.25 payload (may be empty for an empty data frame)
            None    – malformed frame (bad escape sequence, oversize, etc.)

        Non-data-frame commands (e.g. 0x0F) return an empty bytes object b'' so
        that callers can distinguish "valid non-data frame" from "malformed".
        The cmd byte is not returned; callers that need it should use
        decode_with_cmd().
        """
        result = KissPacketizer.decode_with_cmd(kiss_frame)
        if result is None:
            return None
        _cmd, payload = result
        return payload

    @staticmethod
    def decode_with_cmd(kiss_frame: bytes) -> Optional[tuple[int, bytes]]:
        """Decode a KISS frame and return (cmd_byte, ax25_payload).

        Returns None on malformed input.
        Returns (cmd, b'') for valid non-data frames.
        Returns (0x00, payload) for data frames.
        """
        # Strip leading/trailing FENDs
        data = kiss_frame
        start = 0
        while start < len(data) and data[start] == FEND:
            start += 1
        end = len(data)
        while end > start and data[end - 1] == FEND:
            end -= 1

        if start >= end:
            return None  # empty or all-FEND frame

        cmd = data[start]

        # Return-from-KISS and extended commands: valid but no payload
        if cmd != CMD_DATA:
            return (cmd, b'')

        # Check port: high nibble of cmd must be 0 for port 0
        if (cmd >> 4) != 0:
            return None  # non-port-0 frame not supported in MVP

        body = data[start + 1:end]

        # Enforce MVP maximum frame size
        if len(body) > KISS_MAX_FRAME:
            return None

        # Unescape body
        ax25 = bytearray()
        i = 0
        while i < len(body):
            b = body[i]
            if b == FESC:
                i += 1
                if i >= len(body):
                    return None  # trailing FESC — malformed
                nb = body[i]
                if nb == TFEND:
                    ax25.append(FEND)
                elif nb == TFESC:
                    ax25.append(FESC)
                else:
                    return None  # unknown escape sequence — malformed
            elif b == FEND:
                return None  # raw FEND inside body — malformed
            else:
                ax25.append(b)
            i += 1

        return (cmd, bytes(ax25))

    # ── stream framing (serial → on_frame callback) ───────────────────────────

    def feed(self, data: bytes) -> None:
        """Feed raw serial bytes into the packetizer.

        Complete frames (FEND-delimited) are delivered to the on_frame callback
        as raw KISS frame bytes (including surrounding FENDs).
        """
        for byte in data:
            if byte == FEND:
                if self._in_frame and len(self._buf) > 0:
                    # Complete frame: deliver with surrounding FENDs stripped
                    frame = bytes(self._buf)
                    self._buf.clear()
                    self._in_frame = False
                    if self._on_frame:
                        self._on_frame(frame)
                else:
                    # Leading FEND or inter-frame FEND — start of new frame
                    self._buf.clear()
                    self._in_frame = True
            elif self._in_frame:
                if len(self._buf) > KISS_MAX_FRAME + 4:
                    # Oversize frame: discard and reset
                    self._error_count += 1
                    self._buf.clear()
                    self._in_frame = False
                else:
                    self._buf.append(byte)

    def reset(self) -> None:
        """Discard any partially-assembled frame."""
        self._buf.clear()
        self._in_frame = False


# ── BLE chunking helpers ──────────────────────────────────────────────────────

def _chunk_kiss_frame(kiss_frame: bytes, mtu: int = 23, msg_id: int = 0) -> List[bytes]:
    """Split a KISS frame into BleChunker chunks (INT-002).

    Chunk payload capacity: mtu - CHUNK_HEADER_SIZE bytes per chunk.
    Returns a list of chunk byte strings each ready for a BLE write.
    """
    chunk_payload_max = mtu - CHUNK_HEADER_SIZE
    if chunk_payload_max <= 0:
        return []

    chunks = []
    total = (len(kiss_frame) + chunk_payload_max - 1) // chunk_payload_max
    if total == 0:
        total = 1
    if total > 255:
        return []  # too many chunks

    for idx in range(total):
        start = idx * chunk_payload_max
        payload = kiss_frame[start:start + chunk_payload_max]
        header = bytes([msg_id & 0xFF, idx & 0xFF, total & 0xFF])
        chunks.append(header + payload)

    return chunks


def _reassemble_kiss_frame(chunks: List[bytes]) -> Optional[bytes]:
    """Reassemble BleChunker chunks back into a KISS frame (INT-002).

    Returns the reassembled bytes, or None if the chunk list is inconsistent.
    """
    if not chunks:
        return None

    total = chunks[0][2]
    if total == 0 or len(chunks) != total:
        return None

    # Sort by chunk_idx (byte 1 of header)
    try:
        sorted_chunks = sorted(chunks, key=lambda c: c[1])
    except (IndexError, TypeError):
        return None

    out = bytearray()
    for i, chunk in enumerate(sorted_chunks):
        if len(chunk) < CHUNK_HEADER_SIZE:
            return None
        if chunk[1] != i:
            return None  # gap or duplicate
        out.extend(chunk[CHUNK_HEADER_SIZE:])

    return bytes(out)


# ── KissBridge ────────────────────────────────────────────────────────────────

class KissBridge:
    """Async KISS-over-BLE bridge.

    Bridges a serial-side KISS byte stream to a PAKT device's KISS BLE service.

    Usage::

        bridge = KissBridge()
        async with bridge.connect("PAKT-TNC") as b:
            # Write raw serial KISS bytes (will be chunked + sent to KISS TX)
            await b.write_serial(frame_bytes)
            # Received KISS RX frames are available via on_rx_frame callback

    Or use the standalone test harness (no serial port required):
        await bridge.run_test()
    """

    def __init__(self,
                 on_rx_frame: Optional[Callable[[bytes], None]] = None,
                 mtu: int = 23) -> None:
        """
        on_rx_frame  – called with raw KISS frame bytes when the device
                       sends a KISS RX notification.
        mtu          – ATT MTU for chunking (default 23, the BLE minimum).
        """
        self._on_rx_frame = on_rx_frame
        self._mtu = mtu
        self._msg_id: int = 0
        self._client = None
        self._rx_packetizer = KissPacketizer(on_frame=self._on_ble_rx_frame)
        # Multi-chunk RX reassembly: msg_id -> list of raw chunks (with INT-002 header).
        # Max 4 in-progress messages; oldest is evicted when the table is full.
        self._rx_pending: dict = {}

    # ── connection ────────────────────────────────────────────────────────────

    async def connect(self, device_name: str, timeout: float = 10.0):
        """Connect to a named PAKT device.  Returns self for use as context manager."""
        try:
            import bleak
        except ImportError:
            raise RuntimeError(
                "bleak is required for BLE connectivity: pip install bleak"
            )

        logger.info("Scanning for %s ...", device_name)
        devices = await bleak.BleakScanner.discover(timeout=timeout)
        target = None
        for d in devices:
            if d.name and device_name.lower() in d.name.lower():
                target = d
                break

        if target is None:
            raise RuntimeError(f"Device '{device_name}' not found in scan")

        logger.info("Connecting to %s (%s)", target.name, target.address)
        self._client = bleak.BleakClient(target.address)
        await self._client.connect()
        logger.info("Connected")

        # Subscribe to KISS RX notifications
        await self._client.start_notify(UUID_KISS_RX, self._on_kiss_rx_notify)
        logger.info("Subscribed to KISS RX")
        return self

    async def disconnect(self) -> None:
        self._rx_pending.clear()
        if self._client and self._client.is_connected:
            await self._client.stop_notify(UUID_KISS_RX)
            await self._client.disconnect()
            self._client = None

    async def __aenter__(self):
        return self

    async def __aexit__(self, *args):
        await self.disconnect()

    # ── write path (serial → BLE KISS TX) ────────────────────────────────────

    async def write_kiss_frame(self, kiss_frame: bytes) -> bool:
        """Send a complete KISS frame to the device's KISS TX characteristic.

        The frame is split into INT-002 chunks and written one chunk at a time.
        Returns True on success.

        Requires an encrypted + bonded link — pair with the device first.
        """
        if not self._client or not self._client.is_connected:
            logger.warning("write_kiss_frame: not connected")
            return False

        if len(kiss_frame) > KISS_MAX_FRAME + 10:
            logger.warning("write_kiss_frame: frame too large (%d bytes)", len(kiss_frame))
            return False

        self._msg_id = (self._msg_id + 1) & 0xFF
        chunks = _chunk_kiss_frame(kiss_frame, mtu=self._mtu, msg_id=self._msg_id)
        if not chunks:
            logger.warning("write_kiss_frame: chunking failed")
            return False

        try:
            for chunk in chunks:
                await self._client.write_gatt_char(
                    UUID_KISS_TX, chunk, response=True
                )
            return True
        except Exception as exc:
            logger.error("write_kiss_frame error: %s", exc)
            return False

    async def write_ax25_as_kiss(self, ax25_bytes: bytes) -> bool:
        """Encode `ax25_bytes` as a KISS frame and send to the device."""
        kiss_frame = KissPacketizer.encode(ax25_bytes)
        return await self.write_kiss_frame(kiss_frame)

    # ── receive path (BLE KISS RX → serial) ──────────────────────────────────

    def _on_kiss_rx_notify(self, _handle, data: bytes) -> None:
        """Called by bleak when a KISS RX notification arrives.

        `data` is a single INT-002 BLE chunk: [msg_id:1][chunk_idx:1][chunk_total:1][payload...].
        Single-chunk frames are delivered immediately.  Multi-chunk frames are accumulated
        per msg_id and delivered once all chunks have arrived.

        Up to 4 in-progress messages are held concurrently; the oldest is evicted
        when the table is full (guards against lost final chunks holding memory).
        """
        if len(data) < CHUNK_HEADER_SIZE:
            logger.warning("KISS RX: chunk too short (%d bytes)", len(data))
            return

        msg_id      = data[0]
        chunk_idx   = data[1]
        chunk_total = data[2]

        if chunk_total == 0:
            logger.warning("KISS RX: invalid chunk_total=0, dropping")
            return

        if chunk_total == 1 and chunk_idx == 0:
            # Single-chunk fast path — deliver immediately.
            self._deliver_kiss_frame(bytes(data[CHUNK_HEADER_SIZE:]))
            return

        # Multi-chunk path: accumulate chunks per msg_id.
        if msg_id not in self._rx_pending:
            if len(self._rx_pending) >= 4:
                oldest_id = next(iter(self._rx_pending))
                logger.warning("KISS RX: evicting stale msg_id=%d (table full)", oldest_id)
                del self._rx_pending[oldest_id]
            self._rx_pending[msg_id] = []

        chunk_list = self._rx_pending[msg_id]

        # Ignore duplicate chunks (same chunk_idx already received).
        for existing in chunk_list:
            if existing[1] == chunk_idx:
                logger.debug("KISS RX: duplicate chunk idx=%d for msg_id=%d, ignoring",
                             chunk_idx, msg_id)
                return

        chunk_list.append(bytes(data))  # store full chunk with INT-002 header

        if len(chunk_list) == chunk_total:
            result = _reassemble_kiss_frame(chunk_list)
            del self._rx_pending[msg_id]
            if result is None:
                logger.warning("KISS RX: reassembly failed for msg_id=%d (%d chunks)",
                               msg_id, chunk_total)
            else:
                self._deliver_kiss_frame(result)

    def _on_ble_rx_frame(self, frame: bytes) -> None:
        """Called by the RX packetizer when a complete KISS frame is assembled."""
        if self._on_rx_frame:
            self._on_rx_frame(frame)

    def _deliver_kiss_frame(self, kiss_payload: bytes) -> None:
        """Deliver a reassembled KISS frame payload to the on_rx_frame callback.

        kiss_payload is the raw KISS frame content after INT-002 header stripping.
        The firmware includes FEND delimiters in the payload (output of KissFramer::encode),
        so this method delivers the bytes as-is without adding extra delimiters.
        Serial-side KISS clients tolerate leading/trailing FENDs per the KISS spec.
        """
        if self._on_rx_frame:
            self._on_rx_frame(kiss_payload)

    # ── standalone test harness ───────────────────────────────────────────────

    async def run_test(self, device_name: str = "PAKT-TNC") -> bool:
        """Run a basic connectivity and frame validation test.

        Connects to the device, sends a known KISS-encoded AX.25 frame to
        KISS TX, listens for KISS RX notifications for 5 seconds, then
        disconnects and reports results.

        Returns True if the TX write succeeded without error.
        This is the minimum evidence required for INT-003 MVP acceptance §6.
        """
        logger.info("=== KISS bridge self-test ===")

        # A minimal valid AX.25 UI frame: destination W6XYZ-0, source N0CALL-0
        # This is not a real over-the-air frame but exercises the KISS path.
        ax25_test_frame = bytes([
            # Destination: W6XYZ (AX.25 address, shifted left by 1)
            0xAE, 0x6C, 0xB0, 0xB8, 0xB4, 0x40, 0xE0,
            # Source: N0CALL
            0x9C, 0x60, 0x86, 0x82, 0x98, 0x98, 0x61,
            # Control: UI frame 0x03, PID: no layer 3 0xF0
            0x03, 0xF0,
            # Info: minimal APRS-like payload
            0x3E, 0x54, 0x45, 0x53, 0x54,  # ">TEST"
        ])

        rx_frames: list = []

        def on_rx(frame: bytes) -> None:
            rx_frames.append(frame)
            result = KissPacketizer.decode_with_cmd(frame)
            if result:
                cmd, payload = result
                logger.info("KISS RX: cmd=0x%02X payload_len=%d", cmd, len(payload))
            else:
                logger.warning("KISS RX: malformed frame (%d bytes)", len(frame))

        self._on_rx_frame = on_rx

        try:
            await self.connect(device_name)
        except RuntimeError as exc:
            logger.error("Connect failed: %s", exc)
            return False

        logger.info("Sending test KISS TX frame (%d AX.25 bytes) ...",
                    len(ax25_test_frame))
        ok = await self.write_ax25_as_kiss(ax25_test_frame)
        if ok:
            logger.info("KISS TX write accepted")
        else:
            logger.error("KISS TX write FAILED")

        logger.info("Listening for KISS RX for 5 seconds ...")
        await asyncio.sleep(5)

        await self.disconnect()

        logger.info("Test complete: tx_ok=%s rx_frames=%d", ok, len(rx_frames))
        return ok
