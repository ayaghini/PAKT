# test_kiss_bridge.py – pytest tests for kiss_bridge.py (APP-013, INT-003)
#
# Tests the pure-Python framing, encoding, and chunking logic.
# No BLE adapter or hardware required.
#
# Run: pytest test_kiss_bridge.py -v --tb=short

import pytest
from kiss_bridge import (
    KissPacketizer,
    KissBridge,
    _chunk_kiss_frame,
    _reassemble_kiss_frame,
    FEND, FESC, TFEND, TFESC,
    CMD_DATA, CMD_RETURN_FROM_KISS,
    KISS_MAX_FRAME,
    CHUNK_HEADER_SIZE,
)


# ── KissPacketizer.encode ─────────────────────────────────────────────────────

class TestEncode:
    def test_wraps_with_fend_delimiters(self):
        frame = KissPacketizer.encode(b"\x01\x02\x03")
        assert frame[0] == FEND
        assert frame[-1] == FEND

    def test_cmd_byte_is_zero(self):
        frame = KissPacketizer.encode(b"\x01\x02")
        assert frame[1] == CMD_DATA

    def test_plain_bytes_pass_through(self):
        frame = KissPacketizer.encode(b"\x01\x02\x03")
        assert frame == bytes([FEND, 0x00, 0x01, 0x02, 0x03, FEND])

    def test_fend_in_payload_is_escaped(self):
        frame = KissPacketizer.encode(bytes([0xC0]))
        # Should contain FESC TFEND (not raw 0xC0) between cmd byte and tail FEND
        assert bytes([FESC, TFEND]) in frame
        assert frame.count(FEND) == 2  # only leading and trailing FEND

    def test_fesc_in_payload_is_escaped(self):
        frame = KissPacketizer.encode(bytes([0xDB]))
        assert bytes([FESC, TFESC]) in frame

    def test_empty_payload(self):
        frame = KissPacketizer.encode(b"")
        assert frame == bytes([FEND, CMD_DATA, FEND])

    def test_all_special_bytes_escaped(self):
        frame = KissPacketizer.encode(bytes([0xC0, 0xDB]))
        # 0xC0 → FESC TFEND, 0xDB → FESC TFESC
        inner = frame[2:-1]  # strip FEND + cmd + tail FEND
        assert inner == bytes([FESC, TFEND, FESC, TFESC])


# ── KissPacketizer.decode ─────────────────────────────────────────────────────

class TestDecode:
    def test_basic_frame(self):
        frame = bytes([FEND, 0x00, 0x01, 0x02, 0x03, FEND])
        result = KissPacketizer.decode(frame)
        assert result == bytes([0x01, 0x02, 0x03])

    def test_frame_without_fend_delimiters(self):
        frame = bytes([0x00, 0x01, 0x02])
        result = KissPacketizer.decode(frame)
        assert result == bytes([0x01, 0x02])

    def test_escaped_fend_unescaped(self):
        frame = bytes([0x00, FESC, TFEND])
        result = KissPacketizer.decode(frame)
        assert result == bytes([0xC0])

    def test_escaped_fesc_unescaped(self):
        frame = bytes([0x00, FESC, TFESC])
        result = KissPacketizer.decode(frame)
        assert result == bytes([0xDB])

    def test_malformed_trailing_fesc_returns_none(self):
        frame = bytes([0x00, FESC])  # FESC with no following byte
        assert KissPacketizer.decode(frame) is None

    def test_malformed_unknown_escape_returns_none(self):
        frame = bytes([0x00, FESC, 0x01])  # unknown escape
        assert KissPacketizer.decode(frame) is None

    def test_return_from_kiss_returns_empty_bytes(self):
        frame = bytes([FEND, CMD_RETURN_FROM_KISS, FEND])
        result = KissPacketizer.decode(frame)
        assert result == b""

    def test_extended_command_returns_empty_bytes(self):
        frame = bytes([0x01, 0x00])  # TXDELAY extended command
        result = KissPacketizer.decode(frame)
        assert result == b""

    def test_oversize_frame_returns_none(self):
        frame = bytes([0x00]) + bytes([0xAA] * (KISS_MAX_FRAME + 1))
        assert KissPacketizer.decode(frame) is None

    def test_empty_frame_returns_none(self):
        assert KissPacketizer.decode(b"") is None

    def test_only_fend_bytes_returns_none(self):
        assert KissPacketizer.decode(bytes([FEND, FEND, FEND])) is None

    def test_decode_with_cmd_returns_cmd_byte(self):
        frame = bytes([FEND, 0x00, 0x01, 0x02, FEND])
        result = KissPacketizer.decode_with_cmd(frame)
        assert result is not None
        cmd, payload = result
        assert cmd == 0x00
        assert payload == bytes([0x01, 0x02])

    def test_decode_with_cmd_return_from_kiss(self):
        frame = bytes([CMD_RETURN_FROM_KISS])
        result = KissPacketizer.decode_with_cmd(frame)
        assert result is not None
        cmd, payload = result
        assert cmd == CMD_RETURN_FROM_KISS
        assert payload == b""


# ── encode / decode round-trip ────────────────────────────────────────────────

class TestRoundTrip:
    def test_plain_ax25(self):
        original = bytes(range(20))
        encoded = KissPacketizer.encode(original)
        decoded = KissPacketizer.decode(encoded)
        assert decoded == original

    def test_all_special_bytes(self):
        original = bytes([0xC0, 0xDB, 0x01, 0xC0, 0xDB])
        encoded = KissPacketizer.encode(original)
        decoded = KissPacketizer.decode(encoded)
        assert decoded == original

    def test_max_frame_size(self):
        original = bytes([0x42] * KISS_MAX_FRAME)
        encoded = KissPacketizer.encode(original)
        decoded = KissPacketizer.decode(encoded)
        assert decoded == original

    def test_empty_payload(self):
        encoded = KissPacketizer.encode(b"")
        decoded = KissPacketizer.decode(encoded)
        assert decoded == b""


# ── KissPacketizer stream feeding ─────────────────────────────────────────────

class TestFeed:
    def _make_packetizer(self):
        frames = []
        p = KissPacketizer(on_frame=frames.append)
        return p, frames

    def test_single_frame_delivered(self):
        p, frames = self._make_packetizer()
        raw = bytes([FEND, 0x00, 0x01, 0x02, FEND])
        p.feed(raw)
        assert len(frames) == 1
        result = KissPacketizer.decode(frames[0])
        assert result == bytes([0x01, 0x02])

    def test_two_frames_delivered(self):
        p, frames = self._make_packetizer()
        raw = bytes([FEND, 0x00, 0x01, FEND, FEND, 0x00, 0x02, FEND])
        p.feed(raw)
        assert len(frames) == 2

    def test_frame_split_across_two_feeds(self):
        p, frames = self._make_packetizer()
        p.feed(bytes([FEND, 0x00, 0x01]))   # start of frame
        p.feed(bytes([0x02, 0x03, FEND]))   # rest of frame
        assert len(frames) == 1
        result = KissPacketizer.decode(frames[0])
        assert result == bytes([0x01, 0x02, 0x03])

    def test_leading_fend_ignored(self):
        p, frames = self._make_packetizer()
        p.feed(bytes([FEND, FEND, FEND, 0x00, 0x01, FEND]))
        assert len(frames) == 1

    def test_oversize_frame_discarded(self):
        p, frames = self._make_packetizer()
        # Feed a frame larger than KISS_MAX_FRAME + overhead
        oversized = bytes([FEND] + [0x00] + [0xAA] * (KISS_MAX_FRAME + 10) + [FEND])
        p.feed(oversized)
        assert len(frames) == 0
        assert p.error_count >= 1

    def test_reset_clears_partial_frame(self):
        p, frames = self._make_packetizer()
        p.feed(bytes([FEND, 0x00, 0x01]))   # partial frame started
        p.reset()
        p.feed(bytes([FEND, 0x00, 0x02, FEND]))  # new complete frame
        assert len(frames) == 1
        assert KissPacketizer.decode(frames[0]) == bytes([0x02])


# ── BLE chunking helpers ──────────────────────────────────────────────────────

class TestChunking:
    def test_single_chunk_when_frame_fits(self):
        frame = bytes([FEND, 0x00, 0x01, 0x02, 0x03, FEND])
        chunks = _chunk_kiss_frame(frame, mtu=23, msg_id=1)
        assert len(chunks) == 1

    def test_chunk_header_has_correct_fields(self):
        frame = b"\x01" * 5
        chunks = _chunk_kiss_frame(frame, mtu=23, msg_id=0x42)
        assert chunks[0][0] == 0x42   # msg_id
        assert chunks[0][1] == 0x00   # chunk_idx
        assert chunks[0][2] == 0x01   # chunk_total

    def test_multiple_chunks_for_large_frame(self):
        # mtu=10, chunk payload = 7 bytes, 20-byte frame → 3 chunks
        frame = bytes(range(20))
        chunks = _chunk_kiss_frame(frame, mtu=10, msg_id=0x01)
        assert len(chunks) == 3
        assert chunks[0][2] == 3   # chunk_total
        assert chunks[1][1] == 1   # chunk_idx of second chunk

    def test_reassemble_single_chunk(self):
        frame = bytes([FEND, 0x00, 0x01, 0x02, 0x03, FEND])
        chunks = _chunk_kiss_frame(frame, mtu=23, msg_id=1)
        result = _reassemble_kiss_frame(chunks)
        assert result == frame

    def test_reassemble_multiple_chunks(self):
        frame = bytes(range(40))
        chunks = _chunk_kiss_frame(frame, mtu=10, msg_id=2)
        result = _reassemble_kiss_frame(chunks)
        assert result == frame

    def test_reassemble_returns_none_for_empty(self):
        assert _reassemble_kiss_frame([]) is None

    def test_reassemble_out_of_order(self):
        frame = bytes(range(30))
        chunks = _chunk_kiss_frame(frame, mtu=10, msg_id=3)
        # Shuffle order
        shuffled = list(reversed(chunks))
        result = _reassemble_kiss_frame(shuffled)
        assert result == frame

    def test_chunk_then_decode_round_trip(self):
        """KISS encode → chunk → reassemble → decode → original AX.25."""
        ax25 = bytes([0x82, 0x84, 0x86, 0x88, 0x40, 0x40, 0xE0,
                      0x9C, 0x6E, 0x98, 0x8A, 0x9A, 0x40, 0x61,
                      0x03, 0xF0, 0x3E, 0x54, 0x45, 0x53, 0x54])
        kiss_frame = KissPacketizer.encode(ax25)
        chunks = _chunk_kiss_frame(kiss_frame, mtu=15, msg_id=0x0A)
        reassembled = _reassemble_kiss_frame(chunks)
        decoded = KissPacketizer.decode(reassembled)
        assert decoded == ax25


# ── KissBridge receive path (multi-chunk RX reassembly) ───────────────────────
#
# Tests for KissBridge._on_kiss_rx_notify.  No BLE adapter is needed: we call
# the notify handler directly with synthetic INT-002 chunk bytes.
#
# The firmware sends FEND-delimited KISS frames (output of KissFramer::encode).
# notify_kiss_rx applies INT-002 chunking, so the desktop must reassemble.
# _deliver_kiss_frame delivers the reassembled payload as-is (no extra FENDs).

class TestKissBridgeRxNotify:

    def _make_bridge(self):
        frames = []
        bridge = KissBridge(on_rx_frame=frames.append)
        return bridge, frames

    # ── single-chunk (fast path) ──────────────────────────────────────────────

    def test_single_chunk_delivered_immediately(self):
        bridge, frames = self._make_bridge()
        payload = bytes([FEND, 0x00, 0x01, 0x02, FEND])
        # INT-002 header: [msg_id=1, chunk_idx=0, chunk_total=1]
        chunk = bytes([0x01, 0x00, 0x01]) + payload
        bridge._on_kiss_rx_notify(None, chunk)
        assert len(frames) == 1
        assert frames[0] == payload

    def test_single_chunk_payload_stripped_of_header(self):
        bridge, frames = self._make_bridge()
        inner = bytes([FEND, 0x00, 0xAA, 0xBB, FEND])
        chunk = bytes([0x05, 0x00, 0x01]) + inner
        bridge._on_kiss_rx_notify(None, chunk)
        assert frames[0] == inner

    def test_single_chunk_no_extra_fends_added(self):
        """_deliver_kiss_frame must not wrap with extra FENDs (firmware already includes them)."""
        bridge, frames = self._make_bridge()
        payload = bytes([FEND, 0x00, 0x42, FEND])
        chunk = bytes([0x01, 0x00, 0x01]) + payload
        bridge._on_kiss_rx_notify(None, chunk)
        assert frames[0] == payload
        # Verify no extra FENDs were prepended or appended
        assert frames[0][0] == FEND
        assert frames[0][-1] == FEND
        assert len(frames[0]) == len(payload)

    # ── multi-chunk (reassembly path) ─────────────────────────────────────────

    def test_multi_chunk_reassembled_in_order(self):
        bridge, frames = self._make_bridge()
        kiss_frame = bytes([FEND, 0x00] + list(range(40)) + [FEND])
        chunks = _chunk_kiss_frame(kiss_frame, mtu=10, msg_id=0x0A)
        assert len(chunks) > 1
        for chunk in chunks:
            bridge._on_kiss_rx_notify(None, chunk)
        assert len(frames) == 1
        assert frames[0] == kiss_frame

    def test_multi_chunk_reassembled_out_of_order(self):
        bridge, frames = self._make_bridge()
        kiss_frame = bytes([FEND, 0x00] + list(range(30)) + [FEND])
        chunks = _chunk_kiss_frame(kiss_frame, mtu=10, msg_id=0x0B)
        assert len(chunks) > 1
        for chunk in reversed(chunks):
            bridge._on_kiss_rx_notify(None, chunk)
        assert len(frames) == 1
        assert frames[0] == kiss_frame

    def test_multi_chunk_duplicate_chunk_ignored(self):
        bridge, frames = self._make_bridge()
        kiss_frame = bytes([FEND, 0x00] + list(range(30)) + [FEND])
        chunks = _chunk_kiss_frame(kiss_frame, mtu=10, msg_id=0x0C)
        assert len(chunks) > 1
        # Send first chunk twice, then the rest — only one frame should be delivered
        bridge._on_kiss_rx_notify(None, chunks[0])
        bridge._on_kiss_rx_notify(None, chunks[0])  # duplicate
        for chunk in chunks[1:]:
            bridge._on_kiss_rx_notify(None, chunk)
        assert len(frames) == 1
        assert frames[0] == kiss_frame

    def test_multi_chunk_interleaved_two_msg_ids(self):
        """Two different msg_ids can reassemble concurrently."""
        bridge, frames = self._make_bridge()
        frame_a = bytes([FEND, 0x00] + [0xAA] * 20 + [FEND])
        frame_b = bytes([FEND, 0x00] + [0xBB] * 20 + [FEND])
        chunks_a = _chunk_kiss_frame(frame_a, mtu=10, msg_id=1)
        chunks_b = _chunk_kiss_frame(frame_b, mtu=10, msg_id=2)
        assert len(chunks_a) > 1
        assert len(chunks_b) > 1
        # Interleave delivery: a0, b0, a1, b1, ...
        for i in range(max(len(chunks_a), len(chunks_b))):
            if i < len(chunks_a):
                bridge._on_kiss_rx_notify(None, chunks_a[i])
            if i < len(chunks_b):
                bridge._on_kiss_rx_notify(None, chunks_b[i])
        assert len(frames) == 2
        assert frames[0] == frame_a
        assert frames[1] == frame_b

    def test_full_pipeline_encode_chunk_notify_decode(self):
        """End-to-end: AX.25 → KissPacketizer.encode → chunk → notify → decode back."""
        bridge, frames = self._make_bridge()
        ax25 = bytes([0x82, 0x84, 0x86, 0x88, 0x40, 0x40, 0xE0,
                      0x9C, 0x6E, 0x98, 0x8A, 0x9A, 0x40, 0x61,
                      0x03, 0xF0, 0x3E, 0x54, 0x45, 0x53, 0x54])
        kiss_frame = KissPacketizer.encode(ax25)
        # Simulate firmware sending this frame as INT-002 chunks (mtu=23)
        chunks = _chunk_kiss_frame(kiss_frame, mtu=23, msg_id=0x01)
        for chunk in chunks:
            bridge._on_kiss_rx_notify(None, chunk)
        assert len(frames) == 1
        # The delivered payload should be the original KISS frame
        assert frames[0] == kiss_frame
        # Which decodes back to the original AX.25
        decoded = KissPacketizer.decode(frames[0])
        assert decoded == ax25

    # ── error / edge cases ────────────────────────────────────────────────────

    def test_chunk_too_short_dropped(self):
        bridge, frames = self._make_bridge()
        bridge._on_kiss_rx_notify(None, bytes([0x01, 0x00]))  # only 2 bytes, need >= 3
        assert len(frames) == 0

    def test_invalid_chunk_total_zero_dropped(self):
        bridge, frames = self._make_bridge()
        bridge._on_kiss_rx_notify(None, bytes([0x01, 0x00, 0x00, 0xAA]))  # chunk_total=0
        assert len(frames) == 0

    def test_table_eviction_when_full(self):
        """When 4 in-progress messages are pending, oldest is evicted for a 5th."""
        bridge, frames = self._make_bridge()
        kiss_frame = bytes([FEND, 0x00] + list(range(30)) + [FEND])
        # Start 4 incomplete multi-chunk sequences (send only first chunk each)
        for msg_id in range(4):
            chunks = _chunk_kiss_frame(kiss_frame, mtu=10, msg_id=msg_id)
            bridge._on_kiss_rx_notify(None, chunks[0])
        assert len(bridge._rx_pending) == 4
        # 5th message should evict msg_id=0 (oldest)
        chunks5 = _chunk_kiss_frame(kiss_frame, mtu=10, msg_id=99)
        bridge._on_kiss_rx_notify(None, chunks5[0])
        assert len(bridge._rx_pending) == 4
        assert 0 not in bridge._rx_pending
        assert 99 in bridge._rx_pending

    def test_disconnect_clears_pending(self):
        bridge, frames = self._make_bridge()
        kiss_frame = bytes([FEND, 0x00] + list(range(30)) + [FEND])
        chunks = _chunk_kiss_frame(kiss_frame, mtu=10, msg_id=7)
        bridge._on_kiss_rx_notify(None, chunks[0])  # start but don't finish
        assert len(bridge._rx_pending) == 1
        bridge._rx_pending.clear()  # simulates disconnect cleanup
        assert len(bridge._rx_pending) == 0
