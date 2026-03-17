// test_tx_integration.cpp – Host unit tests for TxResultEncoder + AprsTaskContext (P0)
//
// Tests the TX result JSON encoder and the SPSC-ring-buffer integration layer
// that bridges BLE on_tx_request writes to TxScheduler.
//
// Run: ./build/test_host/pakt_tests --reporters=console --no-intro

#include "doctest/doctest.h"
#include "pakt/TxResultEncoder.h"
#include "pakt/AprsTaskContext.h"

#include <cstring>
#include <string>
#include <vector>

using namespace pakt;

// ── TxResultEncoder ───────────────────────────────────────────────────────────

TEST_CASE("TxResultEncoder: TX event encodes correctly") {
    char buf[64];
    size_t n = TxResultEncoder::encode("42", TxResultEvent::TX, buf, sizeof(buf));
    CHECK(n > 0);
    CHECK(std::string(buf) == "{\"msg_id\":\"42\",\"status\":\"tx\"}");
}

TEST_CASE("TxResultEncoder: ACKED event encodes correctly") {
    char buf[64];
    TxResultEncoder::encode("1", TxResultEvent::ACKED, buf, sizeof(buf));
    CHECK(std::string(buf) == "{\"msg_id\":\"1\",\"status\":\"acked\"}");
}

TEST_CASE("TxResultEncoder: TIMEOUT event encodes correctly") {
    char buf[64];
    TxResultEncoder::encode("99999", TxResultEvent::TIMEOUT, buf, sizeof(buf));
    CHECK(std::string(buf) == "{\"msg_id\":\"99999\",\"status\":\"timeout\"}");
}

TEST_CASE("TxResultEncoder: CANCELLED event encodes correctly") {
    char buf[64];
    TxResultEncoder::encode("7", TxResultEvent::CANCELLED, buf, sizeof(buf));
    CHECK(std::string(buf) == "{\"msg_id\":\"7\",\"status\":\"cancelled\"}");
}

TEST_CASE("TxResultEncoder: ERROR event encodes correctly") {
    char buf[64];
    TxResultEncoder::encode("3", TxResultEvent::ERROR, buf, sizeof(buf));
    CHECK(std::string(buf) == "{\"msg_id\":\"3\",\"status\":\"error\"}");
}

TEST_CASE("TxResultEncoder: null buf returns 0") {
    CHECK(TxResultEncoder::encode("1", TxResultEvent::TX, nullptr, 64) == 0);
}

TEST_CASE("TxResultEncoder: zero buf_len returns 0") {
    char buf[64];
    CHECK(TxResultEncoder::encode("1", TxResultEvent::TX, buf, 0) == 0);
}

TEST_CASE("TxResultEncoder: null msg_id returns 0") {
    char buf[64];
    CHECK(TxResultEncoder::encode(nullptr, TxResultEvent::TX, buf, sizeof(buf)) == 0);
}

TEST_CASE("TxResultEncoder: undersized buf truncates and is NUL-terminated") {
    char buf[10];
    buf[9] = '\xAB';  // sentinel
    size_t n = TxResultEncoder::encode("1", TxResultEvent::ACKED, buf, sizeof(buf));
    // snprintf guarantees NUL in buf[9] even if output is truncated.
    CHECK(buf[9] == '\0');
    CHECK(n <= 9);
}

// ── TxResultEncoder: state_to_event mapping ───────────────────────────────────

TEST_CASE("TxResultEncoder: state_to_event ACKED") {
    CHECK(TxResultEncoder::state_to_event(TxMsgState::ACKED) == TxResultEvent::ACKED);
}

TEST_CASE("TxResultEncoder: state_to_event TIMED_OUT") {
    CHECK(TxResultEncoder::state_to_event(TxMsgState::TIMED_OUT) == TxResultEvent::TIMEOUT);
}

TEST_CASE("TxResultEncoder: state_to_event CANCELLED") {
    CHECK(TxResultEncoder::state_to_event(TxMsgState::CANCELLED) == TxResultEvent::CANCELLED);
}

TEST_CASE("TxResultEncoder: state_to_event QUEUED maps to ERROR") {
    CHECK(TxResultEncoder::state_to_event(TxMsgState::QUEUED) == TxResultEvent::ERROR);
}

TEST_CASE("TxResultEncoder: state_to_event PENDING maps to ERROR") {
    CHECK(TxResultEncoder::state_to_event(TxMsgState::PENDING) == TxResultEvent::ERROR);
}

// ── AprsTaskContext helpers ───────────────────────────────────────────────────

static TxRequestFields make_req(const char *dest, const char *text, uint8_t ssid = 0)
{
    TxRequestFields r;
    strncpy(r.dest, dest, 6);  r.dest[6] = '\0';
    strncpy(r.text, text, 67); r.text[67] = '\0';
    r.ssid = ssid;
    return r;
}

// ── AprsTaskContext: basic enqueue + transmit ─────────────────────────────────

TEST_CASE("AprsTaskContext: single request transmitted on first tick") {
    int tx_count = 0;
    AprsTaskContext ctx(
        [&tx_count](const TxMessage &) -> bool { ++tx_count; return true; }
    );

    REQUIRE(ctx.push_tx_request(make_req("APRS", "Hello")));
    ctx.tick(0);
    CHECK(tx_count == 1);
}

TEST_CASE("AprsTaskContext: two requests both transmitted") {
    int tx_count = 0;
    AprsTaskContext ctx(
        [&tx_count](const TxMessage &) -> bool { ++tx_count; return true; }
    );

    ctx.push_tx_request(make_req("APRS", "Msg 1"));
    ctx.push_tx_request(make_req("W1AW", "Msg 2"));
    ctx.tick(0);
    CHECK(tx_count == 2);
}

TEST_CASE("AprsTaskContext: ring buffer full returns false") {
    AprsTaskContext ctx(
        [](const TxMessage &) -> bool { return true; }
    );

    // Fill the ring without consuming.
    for (size_t i = 0; i < AprsTaskContext::kRingDepth; ++i) {
        CHECK(ctx.push_tx_request(make_req("APRS", "X")));
    }
    // One more push must fail.
    CHECK(!ctx.push_tx_request(make_req("APRS", "overflow")));
}

TEST_CASE("AprsTaskContext: ring buffer drains after tick") {
    AprsTaskContext ctx(
        [](const TxMessage &) -> bool { return true; }
    );

    for (size_t i = 0; i < AprsTaskContext::kRingDepth; ++i) {
        ctx.push_tx_request(make_req("APRS", "X"));
    }
    ctx.tick(0);  // drain

    // Ring is now empty — all kRingDepth slots should be available again.
    for (size_t i = 0; i < AprsTaskContext::kRingDepth; ++i) {
        CHECK(ctx.push_tx_request(make_req("APRS", "Y")));
    }
}

// ── AprsTaskContext: notify callbacks ─────────────────────────────────────────

TEST_CASE("AprsTaskContext: TX notify fired on transmit") {
    std::vector<std::pair<std::string, TxResultEvent>> events;

    AprsTaskContext ctx(
        [](const TxMessage &) -> bool { return true; },
        [&events](const char *id, TxResultEvent ev) {
            events.push_back({id, ev});
        }
    );

    ctx.push_tx_request(make_req("APRS", "Hi"));
    ctx.tick(0);

    REQUIRE(!events.empty());
    CHECK(events[0].second == TxResultEvent::TX);
}

TEST_CASE("AprsTaskContext: ACKED notify fired after notify_ack") {
    std::vector<std::pair<std::string, TxResultEvent>> events;
    std::string assigned_id;

    AprsTaskContext ctx(
        [&assigned_id](const TxMessage &msg) -> bool {
            assigned_id = msg.aprs_msg_id;
            return true;
        },
        [&events](const char *id, TxResultEvent ev) {
            events.push_back({id, ev});
        }
    );

    ctx.push_tx_request(make_req("APRS", "Beacon"));
    ctx.tick(0);  // TX fires, assigned_id is set

    REQUIRE(!assigned_id.empty());
    ctx.notify_ack(assigned_id.c_str());

    // Events: [TX, ACKED]
    REQUIRE(events.size() == 2);
    CHECK(events[0].second == TxResultEvent::TX);
    CHECK(events[1].second == TxResultEvent::ACKED);
    CHECK(events[1].first == assigned_id);
}

TEST_CASE("AprsTaskContext: TIMEOUT notify fired after max retries") {
    std::vector<TxResultEvent> event_types;

    AprsTaskContext ctx(
        [](const TxMessage &) -> bool { return true; },
        [&event_types](const char *, TxResultEvent ev) {
            event_types.push_back(ev);
        }
    );

    ctx.push_tx_request(make_req("APRS", "Beacon"));

    // First tick: TX #1 fired.
    ctx.tick(0);

    // Advance time past kRetryIntervalMs for each subsequent retry.
    uint32_t now = 0;
    for (uint8_t i = 1; i < TxScheduler::kMaxRetries; ++i) {
        now += TxScheduler::kRetryIntervalMs + 1;
        ctx.tick(now);
    }

    // After kMaxRetries transmissions: should have kMaxRetries TX events + 1 TIMEOUT.
    CHECK(event_types.size() == static_cast<size_t>(TxScheduler::kMaxRetries) + 1);
    CHECK(event_types.back() == TxResultEvent::TIMEOUT);
}

TEST_CASE("AprsTaskContext: notify_ack returns false for unknown id") {
    AprsTaskContext ctx(
        [](const TxMessage &) -> bool { return true; }
    );
    CHECK(!ctx.notify_ack("99999"));
}

TEST_CASE("AprsTaskContext: null notify fn is safe (no crash)") {
    AprsTaskContext ctx(
        [](const TxMessage &) -> bool { return true; }
        // no notify fn
    );
    ctx.push_tx_request(make_req("APRS", "Hi"));
    ctx.tick(0);  // must not crash
}

// ── AprsTaskContext: edge cases ───────────────────────────────────────────────

TEST_CASE("AprsTaskContext: radio tx failure fires TX notify, message retries next tick") {
    // When the radio is unavailable (returns false), the TX notify is still
    // fired (we attempt), but the message stays QUEUED and is retried.
    int radio_calls = 0;
    std::vector<TxResultEvent> events;

    AprsTaskContext ctx(
        [&radio_calls](const TxMessage &) -> bool {
            ++radio_calls;
            return false;  // radio unavailable
        },
        [&events](const char *, TxResultEvent ev) {
            events.push_back(ev);
        }
    );

    ctx.push_tx_request(make_req("APRS", "Hi"));
    ctx.tick(0);  // attempt 1 — radio fails, message stays QUEUED

    CHECK(radio_calls == 1);
    REQUIRE(events.size() == 1);
    CHECK(events[0] == TxResultEvent::TX);  // notify fires before radio result

    ctx.tick(0);  // attempt 2 — QUEUED messages are always ready
    CHECK(radio_calls == 2);
    CHECK(events.size() == 2);
}

TEST_CASE("AprsTaskContext: invalid request (empty dest) is silently dropped") {
    // TxScheduler rejects BAD_PARAM requests; AprsTaskContext must not crash.
    int tx_count = 0;
    AprsTaskContext ctx(
        [&tx_count](const TxMessage &) -> bool { ++tx_count; return true; }
    );

    TxRequestFields invalid{};  // dest[0] == '\0', text[0] == '\0'
    ctx.push_tx_request(invalid);
    ctx.tick(0);

    CHECK(tx_count == 0);  // Nothing transmitted — scheduler rejected the request
}

TEST_CASE("AprsTaskContext: ack for wrong msg_id does not disturb active message") {
    std::vector<std::pair<std::string, TxResultEvent>> events;
    std::string assigned_id;

    AprsTaskContext ctx(
        [&assigned_id](const TxMessage &msg) -> bool {
            assigned_id = msg.aprs_msg_id;
            return true;
        },
        [&events](const char *id, TxResultEvent ev) {
            events.push_back({id, ev});
        }
    );

    ctx.push_tx_request(make_req("APRS", "Beacon"));
    ctx.tick(0);

    REQUIRE(!assigned_id.empty());
    // Ack for a completely different id — must return false and leave message intact.
    CHECK(!ctx.notify_ack("wrong_id_99"));

    // Only TX event recorded; no terminal event for the real message.
    CHECK(events.size() == 1);
    CHECK(events[0].second == TxResultEvent::TX);
}

// ── AprsTaskContext: KISS raw TX ring ─────────────────────────────────────────

TEST_CASE("AprsTaskContext: push_kiss_ax25 null data rejected") {
    AprsTaskContext ctx([](const TxMessage &) -> bool { return true; });
    CHECK(!ctx.push_kiss_ax25(nullptr, 10));
}

TEST_CASE("AprsTaskContext: push_kiss_ax25 zero length rejected") {
    const uint8_t data[4] = {0xC0, 0x00, 0xAA, 0xC0};
    AprsTaskContext ctx([](const TxMessage &) -> bool { return true; });
    CHECK(!ctx.push_kiss_ax25(data, 0));
}

TEST_CASE("AprsTaskContext: push_kiss_ax25 oversize rejected") {
    // kKissMaxAx25 = 330; 331 bytes must be rejected
    std::vector<uint8_t> big(331, 0xAA);
    AprsTaskContext ctx([](const TxMessage &) -> bool { return true; });
    CHECK(!ctx.push_kiss_ax25(big.data(), big.size()));
}

TEST_CASE("AprsTaskContext: push_kiss_ax25 max-size accepted") {
    std::vector<uint8_t> frame(330, 0xBB);
    int tx_count = 0;
    AprsTaskContext ctx([](const TxMessage &) -> bool { return true; });
    ctx.set_raw_tx_fn([&](const uint8_t *, size_t) -> bool { ++tx_count; return true; });
    CHECK(ctx.push_kiss_ax25(frame.data(), 330));
    ctx.tick(0);
    CHECK(tx_count == 1);
}

TEST_CASE("AprsTaskContext: push_kiss_ax25 enqueued and drained by tick") {
    std::vector<std::vector<uint8_t>> received;

    AprsTaskContext ctx([](const TxMessage &) -> bool { return true; });
    ctx.set_raw_tx_fn([&](const uint8_t *data, size_t len) -> bool {
        received.emplace_back(data, data + len);
        return true;
    });

    const uint8_t frame[] = {0xC0, 0x00, 0x01, 0x02, 0x03, 0xC0};
    CHECK(ctx.push_kiss_ax25(frame, sizeof(frame)));
    CHECK(received.empty());  // not consumed until tick

    ctx.tick(0);
    REQUIRE(received.size() == 1);
    CHECK(received[0] == std::vector<uint8_t>(frame, frame + sizeof(frame)));
}

TEST_CASE("AprsTaskContext: multiple KISS frames drained in FIFO order") {
    std::vector<size_t> rx_lens;
    AprsTaskContext ctx([](const TxMessage &) -> bool { return true; });
    ctx.set_raw_tx_fn([&](const uint8_t *, size_t len) -> bool {
        rx_lens.push_back(len);
        return true;
    });

    uint8_t f1[3] = {0x01, 0x02, 0x03};
    uint8_t f2[5] = {0x04, 0x05, 0x06, 0x07, 0x08};
    CHECK(ctx.push_kiss_ax25(f1, 3));
    CHECK(ctx.push_kiss_ax25(f2, 5));
    ctx.tick(0);

    REQUIRE(rx_lens.size() == 2);
    CHECK(rx_lens[0] == 3);
    CHECK(rx_lens[1] == 5);
}

TEST_CASE("AprsTaskContext: push_kiss_ax25 ring full returns false") {
    // kKissRingDepth = 4; 5th push must fail without a tick in between.
    AprsTaskContext ctx([](const TxMessage &) -> bool { return true; });
    // No raw_tx_fn set so frames accumulate in the ring.
    uint8_t frame[4] = {0xC0, 0x00, 0xAA, 0xC0};
    for (int i = 0; i < 4; ++i) {
        CHECK(ctx.push_kiss_ax25(frame, sizeof(frame)));
    }
    CHECK(!ctx.push_kiss_ax25(frame, sizeof(frame)));  // ring is full
}

TEST_CASE("AprsTaskContext: KISS TX and APRS TX coexist — tick drains both rings") {
    int kiss_tx_count = 0;

    AprsTaskContext ctx(
        [](const TxMessage &) -> bool { return true; }  // APRS TX always succeeds
    );
    ctx.set_raw_tx_fn([&](const uint8_t *, size_t) -> bool {
        ++kiss_tx_count;
        return true;
    });

    // Enqueue one APRS TX request and one raw KISS frame.
    ctx.push_tx_request(make_req("APRS", "Hello"));
    uint8_t kf[] = {0xC0, 0x00, 0x42, 0xC0};
    CHECK(ctx.push_kiss_ax25(kf, sizeof(kf)));

    ctx.tick(0);

    // KISS frame must have been consumed regardless of APRS scheduler state.
    CHECK(kiss_tx_count == 1);
}

TEST_CASE("AprsTaskContext: no raw_tx_fn set — KISS frames silently discarded") {
    // Verify there is no crash when raw_tx_fn_ is null and frames are pushed.
    AprsTaskContext ctx([](const TxMessage &) -> bool { return true; });
    // raw_tx_fn_ not set (nullptr default)
    uint8_t frame[4] = {0xC0, 0x00, 0xBB, 0xC0};
    CHECK(ctx.push_kiss_ax25(frame, sizeof(frame)));
    ctx.tick(0);  // must not crash
}
