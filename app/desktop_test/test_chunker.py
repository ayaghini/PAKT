# test_chunker.py – pytest unit tests for chunker.py (APP-000 / INT-002)
#
# Run: pytest app/desktop_test/test_chunker.py -v
# (from repo root, or just: pytest -v inside app/desktop_test/)

import pytest

from chunker import split, Reassembler, HEADER_SIZE, MAX_CHUNKS

# ── split() ───────────────────────────────────────────────────────────────────


def test_split_single_chunk_header_fields():
    payload = b"\x01\x02\x03"
    chunks = split(payload, msg_id=0x42, chunk_payload_max=20)
    assert len(chunks) == 1
    assert chunks[0][0] == 0x42   # msg_id
    assert chunks[0][1] == 0      # chunk_idx
    assert chunks[0][2] == 1      # chunk_total
    assert chunks[0][HEADER_SIZE:] == payload


def test_split_multiple_chunks_exact_division():
    payload = bytes(range(40))
    chunks = split(payload, msg_id=1, chunk_payload_max=20)
    assert len(chunks) == 2
    assert chunks[0][2] == 2
    assert chunks[1][2] == 2
    assert chunks[0][1] == 0
    assert chunks[1][1] == 1
    assert chunks[0][HEADER_SIZE:] == payload[:20]
    assert chunks[1][HEADER_SIZE:] == payload[20:]


def test_split_last_chunk_smaller():
    payload = bytes(25)
    chunks = split(payload, msg_id=2, chunk_payload_max=20)
    assert len(chunks) == 2
    assert len(chunks[0]) == HEADER_SIZE + 20
    assert len(chunks[1]) == HEADER_SIZE + 5


def test_split_empty_payload_returns_empty():
    assert split(b"", msg_id=1, chunk_payload_max=20) == []


def test_split_exceeds_max_chunks_raises():
    # 1-byte chunks of 65 bytes → 65 chunks > MAX_CHUNKS (64)
    with pytest.raises(ValueError):
        split(bytes(65), msg_id=1, chunk_payload_max=1)


def test_split_msg_id_in_all_chunks():
    chunks = split(b"\xDE\xAD\xBE\xEF", msg_id=0xAB, chunk_payload_max=2)
    assert all(c[0] == 0xAB for c in chunks)


def test_split_payload_bytes_across_boundary():
    payload = bytes([0xAA, 0xBB, 0xCC, 0xDD, 0xEE])
    chunks = split(payload, msg_id=1, chunk_payload_max=3)
    assert len(chunks) == 2
    assert chunks[0][HEADER_SIZE:] == bytes([0xAA, 0xBB, 0xCC])
    assert chunks[1][HEADER_SIZE:] == bytes([0xDD, 0xEE])


def test_split_msg_id_wraps_at_256():
    # msg_id=256 should wrap to 0
    chunks = split(b"\x01", msg_id=256, chunk_payload_max=20)
    assert chunks[0][0] == 0


def test_split_max_chunks_exactly():
    # MAX_CHUNKS chunks of 1 byte each → exactly at limit, no error
    payload = bytes(MAX_CHUNKS)
    chunks = split(payload, msg_id=1, chunk_payload_max=1)
    assert len(chunks) == MAX_CHUNKS


# ── Reassembler ───────────────────────────────────────────────────────────────


def test_reassemble_single_chunk():
    received: list[bytes] = []
    r = Reassembler(received.append)
    chunk = bytes([0x01, 0x00, 0x01, 0xAB, 0xCD])
    assert r.feed(chunk)
    assert received == [b"\xAB\xCD"]


def test_reassemble_two_chunks_in_order():
    received: list[bytes] = []
    r = Reassembler(received.append)
    assert r.feed(bytes([0x05, 0x00, 0x02, 0x11, 0x22]))
    assert received == []
    assert r.feed(bytes([0x05, 0x01, 0x02, 0x33, 0x44]))
    assert received == [b"\x11\x22\x33\x44"]


def test_reassemble_out_of_order():
    received: list[bytes] = []
    r = Reassembler(received.append)
    r.feed(bytes([0x07, 0x01, 0x03, 0xBB]))
    r.feed(bytes([0x07, 0x02, 0x03, 0xCC]))
    r.feed(bytes([0x07, 0x00, 0x03, 0xAA]))
    assert received == [b"\xAA\xBB\xCC"]


def test_reassemble_duplicate_ignored():
    calls = 0

    def cb(_: bytes) -> None:
        nonlocal calls
        calls += 1

    r = Reassembler(cb)
    c0 = bytes([0x10, 0x00, 0x02, 0xAA])
    c1 = bytes([0x10, 0x01, 0x02, 0xBB])
    r.feed(c0)
    r.feed(c0)  # duplicate
    r.feed(c1)
    assert calls == 1


def test_reassemble_timeout_expires(monkeypatch):
    import time as _time

    start = 0.0

    def fake_monotonic() -> float:
        return start

    monkeypatch.setattr(_time, "monotonic", fake_monotonic)

    received: list[bytes] = []
    r = Reassembler(received.append, timeout_s=1.0)

    # Feed chunk 0/2 at t=0
    r.feed(bytes([0x20, 0x00, 0x02, 0xAA]))

    # Advance time past timeout
    start = 2.0

    # Next feed triggers expire(); the old slot is discarded.
    # This chunk (1/2) starts a fresh message – still incomplete.
    r.feed(bytes([0x20, 0x01, 0x02, 0xBB]))
    assert received == []


def test_reassemble_malformed_too_short():
    r = Reassembler(lambda _: None)
    assert not r.feed(bytes([0x01, 0x00]))  # 2 bytes, need ≥ 3


def test_reassemble_malformed_chunk_total_zero():
    r = Reassembler(lambda _: None)
    assert not r.feed(bytes([0x01, 0x00, 0x00, 0xAA]))


def test_reassemble_malformed_idx_out_of_range():
    r = Reassembler(lambda _: None)
    assert not r.feed(bytes([0x01, 0x02, 0x02, 0xAA]))  # idx=2, total=2


def test_reassemble_inconsistent_chunk_total():
    r = Reassembler(lambda _: None)
    assert r.feed(bytes([0x30, 0x00, 0x02, 0xAA]))   # total=2
    assert not r.feed(bytes([0x30, 0x01, 0x03, 0xBB]))  # total=3 – mismatch


def test_reassemble_reset_discards_in_progress():
    completed: list[bytes] = []
    r = Reassembler(completed.append)
    r.feed(bytes([0x40, 0x00, 0x02, 0xAA]))
    r.reset()
    r.feed(bytes([0x40, 0x01, 0x02, 0xBB]))  # second chunk after reset
    assert completed == []


def test_reassemble_two_independent_messages():
    results: dict[int, bytes] = {}

    def cb(data: bytes) -> None:
        results[data[0]] = data

    r = Reassembler(cb)
    # Two simultaneous single-chunk messages
    r.feed(bytes([0x01, 0x00, 0x01, 0x01]))
    r.feed(bytes([0x02, 0x00, 0x01, 0x02]))
    assert results == {0x01: b"\x01", 0x02: b"\x02"}


# ── Round-trips ───────────────────────────────────────────────────────────────


def test_roundtrip_60_bytes():
    original = bytes(range(60))
    received: list[bytes] = []
    r = Reassembler(received.append)
    for chunk in split(original, msg_id=0xAB, chunk_payload_max=20):
        r.feed(chunk)
    assert received == [original]


def test_roundtrip_out_of_order():
    original = b"\xDE\xAD\xBE\xEF\xCA\xFE"
    received: list[bytes] = []
    r = Reassembler(received.append)
    chunks = split(original, msg_id=0x99, chunk_payload_max=2)
    for chunk in reversed(chunks):
        r.feed(chunk)
    assert received == [original]


def test_roundtrip_max_chunks():
    original = bytes(range(MAX_CHUNKS))
    received: list[bytes] = []
    r = Reassembler(received.append)
    for chunk in split(original, msg_id=0xCC, chunk_payload_max=1):
        r.feed(chunk)
    assert received == [original]


def test_roundtrip_default_mtu_chunk_size():
    # Simulate default MTU=23: chunk_payload_max = 23 - 3 (ATT) - 3 (header) = 17
    payload = bytes(range(256))
    received: list[bytes] = []
    r = Reassembler(received.append)
    for chunk in split(payload, msg_id=0x01, chunk_payload_max=17):
        r.feed(chunk)
    assert received == [payload]


def test_roundtrip_single_byte():
    received: list[bytes] = []
    r = Reassembler(received.append)
    for chunk in split(b"\x7F", msg_id=0x01, chunk_payload_max=20):
        r.feed(chunk)
    assert received == [b"\x7F"]
