# test_messaging.py – pytest tests for message_tracker.py (APP-006)
#
# No BLE adapter required.
#
# Run: pytest test_messaging.py -v --tb=short

import json
import pytest
from message_tracker import MessageTracker, MsgState, TrackedMessage


# ── Helpers ───────────────────────────────────────────────────────────────────

def make_tracker():
    updates = []
    t = MessageTracker(on_update=updates.append)
    return t, updates


def _result(msg_id: str, status: str) -> str:
    return json.dumps({"msg_id": msg_id, "status": status})


# ── on_sent ───────────────────────────────────────────────────────────────────

class TestOnSent:
    def test_returns_client_id(self):
        t, _ = make_tracker()
        cid = t.on_sent("1", "W1AW-9", "Hello")
        assert isinstance(cid, int)
        assert cid >= 1

    def test_message_initially_pending(self):
        t, _ = make_tracker()
        t.on_sent("1", "W1AW-9", "Hello")
        msg = t.get_by_msg_id("1")
        assert msg is not None
        assert msg.state == MsgState.PENDING

    def test_on_update_fires_on_sent(self):
        t, updates = make_tracker()
        t.on_sent("1", "W1AW-9", "Hello")
        assert len(updates) == 1
        assert updates[0].msg_id == "1"

    def test_message_stored_by_msg_id(self):
        t, _ = make_tracker()
        t.on_sent("42", "K1ABC", "Test")
        assert t.get_by_msg_id("42") is not None

    def test_get_by_client_id(self):
        t, _ = make_tracker()
        cid = t.on_sent("5", "N0CALL", "Hi")
        assert t.get_by_client_id(cid) is not None
        assert t.get_by_client_id(cid).msg_id == "5"

    def test_multiple_sends_get_unique_client_ids(self):
        t, _ = make_tracker()
        ids = {t.on_sent(str(i), "W1AW", "msg") for i in range(5)}
        assert len(ids) == 5

    def test_dest_and_text_stored(self):
        t, _ = make_tracker()
        t.on_sent("7", "VE3XYZ-4", "APRS test")
        msg = t.get_by_msg_id("7")
        assert msg.dest == "VE3XYZ-4"
        assert msg.text == "APRS test"


# ── on_tx_result ──────────────────────────────────────────────────────────────

class TestOnTxResult:
    def test_ack_transitions_to_acked(self):
        t, _ = make_tracker()
        t.on_sent("1", "W1AW", "Hi")
        t.on_tx_result(_result("1", "acked"))
        assert t.get_by_msg_id("1").state == MsgState.ACKED

    def test_timeout_transitions_to_timeout(self):
        t, _ = make_tracker()
        t.on_sent("2", "W1AW", "Hi")
        t.on_tx_result(_result("2", "timeout"))
        assert t.get_by_msg_id("2").state == MsgState.TIMEOUT

    def test_error_transitions_to_error(self):
        t, _ = make_tracker()
        t.on_sent("3", "W1AW", "Hi")
        t.on_tx_result(_result("3", "error"))
        assert t.get_by_msg_id("3").state == MsgState.ERROR

    def test_tx_increments_attempts(self):
        t, _ = make_tracker()
        t.on_sent("4", "W1AW", "Hi")
        t.on_tx_result(_result("4", "tx"))
        t.on_tx_result(_result("4", "tx"))
        assert t.get_by_msg_id("4").tx_attempts == 2

    def test_tx_does_not_transition_to_terminal(self):
        t, _ = make_tracker()
        t.on_sent("5", "W1AW", "Hi")
        t.on_tx_result(_result("5", "tx"))
        assert not t.get_by_msg_id("5").state.is_terminal()

    def test_unknown_msg_id_returns_none(self):
        t, _ = make_tracker()
        result = t.on_tx_result(_result("999", "acked"))
        assert result is None

    def test_invalid_json_returns_none(self):
        t, _ = make_tracker()
        assert t.on_tx_result("NOT JSON") is None

    def test_on_update_fires_on_ack(self):
        t, updates = make_tracker()
        t.on_sent("1", "W1AW", "Hi")
        updates.clear()
        t.on_tx_result(_result("1", "acked"))
        assert len(updates) == 1
        assert updates[0].state == MsgState.ACKED

    def test_second_ack_ignored_once_terminal(self):
        t, _ = make_tracker()
        t.on_sent("1", "W1AW", "Hi")
        t.on_tx_result(_result("1", "acked"))
        t.on_tx_result(_result("1", "timeout"))   # too late
        assert t.get_by_msg_id("1").state == MsgState.ACKED  # unchanged

    def test_resolved_at_set_on_terminal(self):
        t, _ = make_tracker()
        t.on_sent("1", "W1AW", "Hi")
        t.on_tx_result(_result("1", "acked"))
        assert t.get_by_msg_id("1").resolved_at is not None

    def test_resolved_at_not_set_on_tx(self):
        t, _ = make_tracker()
        t.on_sent("1", "W1AW", "Hi")
        t.on_tx_result(_result("1", "tx"))
        assert t.get_by_msg_id("1").resolved_at is None


# ── cancel ────────────────────────────────────────────────────────────────────

class TestCancel:
    def test_cancel_pending_message(self):
        t, _ = make_tracker()
        cid = t.on_sent("1", "W1AW", "Hi")
        assert t.cancel(cid)
        assert t.get_by_msg_id("1").state == MsgState.CANCELLED

    def test_cancel_returns_false_for_unknown(self):
        t, _ = make_tracker()
        assert not t.cancel(99)

    def test_cancel_returns_false_for_terminal(self):
        t, _ = make_tracker()
        cid = t.on_sent("1", "W1AW", "Hi")
        t.on_tx_result(_result("1", "acked"))
        assert not t.cancel(cid)

    def test_cancel_fires_on_update(self):
        t, updates = make_tracker()
        cid = t.on_sent("1", "W1AW", "Hi")
        updates.clear()
        t.cancel(cid)
        assert len(updates) == 1
        assert updates[0].state == MsgState.CANCELLED


# ── pending() / recent() ──────────────────────────────────────────────────────

class TestInspection:
    def test_pending_returns_non_terminal(self):
        t, _ = make_tracker()
        t.on_sent("1", "W1AW", "Hi")
        t.on_sent("2", "W1AW", "Ho")
        t.on_tx_result(_result("1", "acked"))
        pending = t.pending()
        assert len(pending) == 1
        assert pending[0].msg_id == "2"

    def test_pending_empty_when_all_resolved(self):
        t, _ = make_tracker()
        cid = t.on_sent("1", "W1AW", "Hi")
        t.cancel(cid)
        assert t.pending() == []

    def test_recent_returns_all_messages(self):
        t, _ = make_tracker()
        for i in range(5):
            t.on_sent(str(i), "W1AW", f"msg{i}")
        assert len(t.recent()) == 5

    def test_recent_respects_n_limit(self):
        t, _ = make_tracker()
        for i in range(10):
            t.on_sent(str(i), "W1AW", f"msg{i}")
        assert len(t.recent(3)) == 3

    def test_recent_newest_first(self):
        t, _ = make_tracker()
        t.on_sent("1", "W1AW", "first")
        t.on_sent("2", "W1AW", "second")
        recent = t.recent()
        # Most recently queued should be first
        assert recent[0].msg_id == "2"


# ── clear_resolved ────────────────────────────────────────────────────────────

class TestClearResolved:
    def test_removes_terminal_messages(self):
        t, _ = make_tracker()
        t.on_sent("1", "W1AW", "Hi")
        t.on_tx_result(_result("1", "acked"))
        n = t.clear_resolved()
        assert n == 1
        assert t.get_by_msg_id("1") is None

    def test_keeps_non_terminal_messages(self):
        t, _ = make_tracker()
        t.on_sent("1", "W1AW", "Hi")          # pending
        t.on_sent("2", "W1AW", "Ho")
        t.on_tx_result(_result("2", "acked"))  # terminal
        t.clear_resolved()
        assert t.get_by_msg_id("1") is not None
        assert t.get_by_msg_id("2") is None

    def test_clear_resolved_removes_client_id_lookup(self):
        t, _ = make_tracker()
        cid = t.on_sent("1", "W1AW", "Hi")
        t.on_tx_result(_result("1", "acked"))
        t.clear_resolved()
        assert t.get_by_client_id(cid) is None

    def test_returns_zero_when_nothing_to_clear(self):
        t, _ = make_tracker()
        t.on_sent("1", "W1AW", "Hi")
        assert t.clear_resolved() == 0


# ── MsgState.is_terminal ──────────────────────────────────────────────────────

class TestMsgState:
    def test_pending_is_not_terminal(self):
        assert not MsgState.PENDING.is_terminal()

    def test_acked_is_terminal(self):
        assert MsgState.ACKED.is_terminal()

    def test_timeout_is_terminal(self):
        assert MsgState.TIMEOUT.is_terminal()

    def test_error_is_terminal(self):
        assert MsgState.ERROR.is_terminal()

    def test_cancelled_is_terminal(self):
        assert MsgState.CANCELLED.is_terminal()
