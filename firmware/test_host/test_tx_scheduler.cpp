// test_tx_scheduler.cpp – Host unit tests for TxScheduler (FW-010)
//
// No hardware or RTOS required.  Uses doctest.
// Run: ./build/test_host/pakt_tests --reporters=console --no-intro

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "pakt/TxScheduler.h"

#include <cstring>
#include <vector>
#include <string>

using namespace pakt;

// ── Test helpers ──────────────────────────────────────────────────────────────

// Simple always-succeeds transmitter that records what was sent.
struct FakeTx {
    std::vector<std::string> sent_ids;
    bool fail{false};

    bool operator()(const TxMessage &msg) {
        if (fail) return false;
        sent_ids.push_back(msg.aprs_msg_id);
        return true;
    }
};

struct FakeResult {
    std::vector<TxMsgState> outcomes;
    std::vector<uint8_t>    client_ids;

    void operator()(const TxMessage &msg) {
        outcomes.push_back(msg.state);
        client_ids.push_back(msg.client_id);
    }
};

static TxScheduler make_sched(FakeTx &tx, FakeResult &res) {
    return TxScheduler(
        [&tx](const TxMessage &m) { return tx(m); },
        [&res](const TxMessage &m) { res(m); }
    );
}

// ── EnqueueResult ─────────────────────────────────────────────────────────────

TEST_CASE("enqueue: basic success") {
    FakeTx tx; FakeResult res;
    auto sched = make_sched(tx, res);
    char id[TxScheduler::kMaxMsgIdStr]{};
    auto r = sched.enqueue(1, "W1AW", 0, "Hello", 0, id);
    CHECK(r == EnqueueResult::OK);
    CHECK(id[0] != '\0');
    CHECK(sched.active_count() == 1);
}

TEST_CASE("enqueue: rejects empty dest") {
    FakeTx tx; FakeResult res;
    auto sched = make_sched(tx, res);
    CHECK(sched.enqueue(1, "", 0, "Hello", 0) == EnqueueResult::BAD_PARAM);
}

TEST_CASE("enqueue: rejects empty text") {
    FakeTx tx; FakeResult res;
    auto sched = make_sched(tx, res);
    CHECK(sched.enqueue(1, "W1AW", 0, "", 0) == EnqueueResult::BAD_PARAM);
}

TEST_CASE("enqueue: rejects null dest") {
    FakeTx tx; FakeResult res;
    auto sched = make_sched(tx, res);
    CHECK(sched.enqueue(1, nullptr, 0, "Hello", 0) == EnqueueResult::BAD_PARAM);
}

TEST_CASE("enqueue: rejects null text") {
    FakeTx tx; FakeResult res;
    auto sched = make_sched(tx, res);
    CHECK(sched.enqueue(1, "W1AW", 0, nullptr, 0) == EnqueueResult::BAD_PARAM);
}

TEST_CASE("enqueue: rejects dest > 6 chars") {
    FakeTx tx; FakeResult res;
    auto sched = make_sched(tx, res);
    CHECK(sched.enqueue(1, "TOOLONG", 0, "Hi", 0) == EnqueueResult::BAD_PARAM);
}

TEST_CASE("enqueue: rejects text > 67 chars") {
    FakeTx tx; FakeResult res;
    auto sched = make_sched(tx, res);
    std::string long_text(68, 'x');
    CHECK(sched.enqueue(1, "W1AW", 0, long_text.c_str(), 0) == EnqueueResult::BAD_PARAM);
}

TEST_CASE("enqueue: queue full returns QUEUE_FULL") {
    FakeTx tx; FakeResult res;
    auto sched = make_sched(tx, res);
    for (size_t i = 0; i < TxScheduler::kMaxQueue; ++i) {
        CHECK(sched.enqueue(static_cast<uint8_t>(i), "W1AW", 0, "Hi", 0) == EnqueueResult::OK);
    }
    CHECK(sched.enqueue(99, "W1AW", 0, "overflow", 0) == EnqueueResult::QUEUE_FULL);
}

TEST_CASE("enqueue: assigns unique monotonic msg IDs") {
    FakeTx tx; FakeResult res;
    auto sched = make_sched(tx, res);
    char id1[TxScheduler::kMaxMsgIdStr]{};
    char id2[TxScheduler::kMaxMsgIdStr]{};
    sched.enqueue(1, "W1AW", 0, "first",  0, id1);
    sched.enqueue(2, "W1AW", 0, "second", 0, id2);
    CHECK(std::string(id1) != std::string(id2));
}

// ── tick: first transmission ──────────────────────────────────────────────────

TEST_CASE("tick: QUEUED message is transmitted immediately") {
    FakeTx tx; FakeResult res;
    auto sched = make_sched(tx, res);
    sched.enqueue(1, "W1AW", 0, "Hello", 0);
    int n = sched.tick(0);
    CHECK(n == 1);
    CHECK(tx.sent_ids.size() == 1);
}

TEST_CASE("tick: message transitions to PENDING after first TX") {
    FakeTx tx; FakeResult res;
    auto sched = make_sched(tx, res);
    sched.enqueue(1, "W1AW", 0, "Hello", 0);
    sched.tick(0);
    // Should NOT retransmit immediately after first TX
    int n = sched.tick(1);   // only 1 ms later
    CHECK(n == 0);
    CHECK(tx.sent_ids.size() == 1);
}

TEST_CASE("tick: retransmits after retry interval") {
    FakeTx tx; FakeResult res;
    auto sched = make_sched(tx, res);
    sched.enqueue(1, "W1AW", 0, "Hello", 0);
    sched.tick(0);
    sched.tick(TxScheduler::kRetryIntervalMs);
    CHECK(tx.sent_ids.size() == 2);
}

TEST_CASE("tick: does not retransmit before retry interval") {
    FakeTx tx; FakeResult res;
    auto sched = make_sched(tx, res);
    sched.enqueue(1, "W1AW", 0, "Hello", 0);
    sched.tick(0);
    sched.tick(TxScheduler::kRetryIntervalMs - 1);
    CHECK(tx.sent_ids.size() == 1);
}

TEST_CASE("tick: transmitter failure does not advance state") {
    FakeTx tx; FakeResult res;
    tx.fail = true;
    auto sched = make_sched(tx, res);
    sched.enqueue(1, "W1AW", 0, "Hello", 0);
    sched.tick(0);
    CHECK(tx.sent_ids.empty());
    CHECK(sched.active_count() == 1);
    // QUEUED — still in active queue
    CHECK(sched.slots()[0].state == TxMsgState::QUEUED);
}

// ── tick: retry exhaustion ────────────────────────────────────────────────────

TEST_CASE("tick: TIMED_OUT after kMaxRetries transmissions") {
    FakeTx tx; FakeResult res;
    auto sched = make_sched(tx, res);
    sched.enqueue(1, "W1AW", 0, "Hello", 0);

    for (uint8_t i = 0; i < TxScheduler::kMaxRetries; ++i) {
        sched.tick(static_cast<uint32_t>(i) * TxScheduler::kRetryIntervalMs);
    }

    CHECK(tx.sent_ids.size() == TxScheduler::kMaxRetries);
    CHECK(res.outcomes.size() == 1);
    CHECK(res.outcomes[0] == TxMsgState::TIMED_OUT);
    CHECK(sched.active_count() == 0);
}

TEST_CASE("tick: TIMED_OUT result fires result callback with correct client_id") {
    FakeTx tx; FakeResult res;
    auto sched = make_sched(tx, res);
    sched.enqueue(42, "W1AW", 0, "Hello", 0);

    for (uint8_t i = 0; i < TxScheduler::kMaxRetries; ++i) {
        sched.tick(static_cast<uint32_t>(i) * TxScheduler::kRetryIntervalMs);
    }

    CHECK(res.client_ids.size() == 1);
    CHECK(res.client_ids[0] == 42);
}

// ── on_ack_received ───────────────────────────────────────────────────────────

TEST_CASE("on_ack_received: matches correct message ID") {
    FakeTx tx; FakeResult res;
    auto sched = make_sched(tx, res);
    char id[TxScheduler::kMaxMsgIdStr]{};
    sched.enqueue(1, "W1AW", 0, "Hello", 0, id);
    sched.tick(0);   // → PENDING
    bool acked = sched.on_ack_received(id);
    CHECK(acked);
    CHECK(res.outcomes[0] == TxMsgState::ACKED);
    CHECK(sched.active_count() == 0);
}

TEST_CASE("on_ack_received: no match for wrong ID") {
    FakeTx tx; FakeResult res;
    auto sched = make_sched(tx, res);
    sched.enqueue(1, "W1AW", 0, "Hello", 0);
    sched.tick(0);
    bool acked = sched.on_ack_received("99999");
    CHECK(!acked);
    CHECK(res.outcomes.empty());
}

TEST_CASE("on_ack_received: null ID returns false") {
    FakeTx tx; FakeResult res;
    auto sched = make_sched(tx, res);
    sched.enqueue(1, "W1AW", 0, "Hello", 0);
    sched.tick(0);
    CHECK(!sched.on_ack_received(nullptr));
}

TEST_CASE("on_ack_received: ignores already-terminal messages") {
    FakeTx tx; FakeResult res;
    auto sched = make_sched(tx, res);
    char id[TxScheduler::kMaxMsgIdStr]{};
    sched.enqueue(1, "W1AW", 0, "Hello", 0, id);
    sched.tick(0);
    sched.on_ack_received(id);   // → ACKED
    res.outcomes.clear();
    bool second = sched.on_ack_received(id);  // already ACKED
    CHECK(!second);
    CHECK(res.outcomes.empty());
}

// ── cancel ────────────────────────────────────────────────────────────────────

TEST_CASE("cancel: cancels a QUEUED message") {
    FakeTx tx; FakeResult res;
    auto sched = make_sched(tx, res);
    sched.enqueue(7, "W1AW", 0, "Hello", 0);
    bool ok = sched.cancel(7);
    CHECK(ok);
    CHECK(res.outcomes[0] == TxMsgState::CANCELLED);
    CHECK(sched.active_count() == 0);
}

TEST_CASE("cancel: cancels a PENDING message") {
    FakeTx tx; FakeResult res;
    auto sched = make_sched(tx, res);
    sched.enqueue(7, "W1AW", 0, "Hello", 0);
    sched.tick(0);
    CHECK(sched.cancel(7));
    CHECK(res.outcomes[0] == TxMsgState::CANCELLED);
}

TEST_CASE("cancel: returns false for unknown client_id") {
    FakeTx tx; FakeResult res;
    auto sched = make_sched(tx, res);
    CHECK(!sched.cancel(99));
}

TEST_CASE("cancel: returns false for already-terminal message") {
    FakeTx tx; FakeResult res;
    auto sched = make_sched(tx, res);
    char id[TxScheduler::kMaxMsgIdStr]{};
    sched.enqueue(3, "W1AW", 0, "Hi", 0, id);
    sched.tick(0);
    sched.on_ack_received(id);  // → ACKED
    CHECK(!sched.cancel(3));
}

// ── slot recycling ────────────────────────────────────────────────────────────

TEST_CASE("slot recycling: terminal slot reused when queue would otherwise be full") {
    FakeTx tx; FakeResult res;
    auto sched = make_sched(tx, res);

    // Fill queue with 8 messages that are sent and acked immediately.
    for (size_t i = 0; i < TxScheduler::kMaxQueue; ++i) {
        char id[TxScheduler::kMaxMsgIdStr]{};
        sched.enqueue(static_cast<uint8_t>(i), "W1AW", 0, "Hi",
                      static_cast<uint32_t>(i), id);
        sched.tick(static_cast<uint32_t>(i));
        sched.on_ack_received(id);  // terminal → slot recyclable
    }

    // All 8 slots should be occupied (terminal) now; active_count == 0.
    CHECK(sched.active_count() == 0);

    // New enqueue should succeed (reclaims a terminal slot).
    auto r = sched.enqueue(99, "W1AW", 0, "New", 1000);
    CHECK(r == EnqueueResult::OK);
    CHECK(sched.active_count() == 1);
}

// ── multiple concurrent messages ──────────────────────────────────────────────

TEST_CASE("multiple concurrent messages: each follows independent state machine") {
    FakeTx tx; FakeResult res;
    auto sched = make_sched(tx, res);

    char id1[TxScheduler::kMaxMsgIdStr]{};
    char id2[TxScheduler::kMaxMsgIdStr]{};
    sched.enqueue(1, "K1AAA", 0, "msg one", 0, id1);
    sched.enqueue(2, "K2BBB", 0, "msg two", 0, id2);

    sched.tick(0);   // Both transmitted
    CHECK(tx.sent_ids.size() == 2);

    sched.on_ack_received(id1);  // msg1 acked
    CHECK(sched.active_count() == 1);  // msg2 still pending

    sched.on_ack_received(id2);  // msg2 acked
    CHECK(sched.active_count() == 0);
    CHECK(res.outcomes.size() == 2);
}
