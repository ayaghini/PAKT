// test_sa818.cpp – SA818 driver host unit tests
//
// Tests Sa818CommandFormatter, Sa818ResponseParser, and Sa818Radio
// using a MockTransport that captures writes and injects canned reads.
// No ESP-IDF headers are used; PTT GPIO is simulated via a lambda counter.

#include <doctest/doctest.h>

#include "pakt/Sa818CommandFormatter.h"
#include "pakt/Sa818ResponseParser.h"
#include "pakt/Sa818Radio.h"

#include <cstring>
#include <string>
#include <vector>

using namespace pakt;

// ── MockTransport ─────────────────────────────────────────────────────────────

struct MockTransport : ISa818Transport
{
    std::vector<std::string> writes;     // commands sent by driver
    std::string              inject;     // response to return on next read()
    bool                     fail_write  = false;
    bool                     fail_read   = false;

    bool write(const char *data, size_t len) override {
        if (fail_write) return false;
        writes.emplace_back(data, len);
        return true;
    }

    size_t read(char *buf, size_t len, uint32_t /*timeout_ms*/) override {
        if (fail_read || inject.empty()) return 0;
        size_t n = inject.size() < len - 1 ? inject.size() : len - 1;
        memcpy(buf, inject.data(), n);
        buf[n] = '\0';
        return n;
    }

    void queue_response(const char *resp) { inject = resp; }
    void clear() { writes.clear(); inject.clear(); fail_write = false; fail_read = false; }
};

// ── Sa818CommandFormatter tests ───────────────────────────────────────────────

TEST_CASE("Sa818CommandFormatter: connect command is correct") {
    char buf[32];
    size_t n = Sa818CommandFormatter::connect(buf, sizeof(buf));
    CHECK(n > 0);
    CHECK(std::string(buf, n) == "AT+DMOCONNECT\r\n");
}

TEST_CASE("Sa818CommandFormatter: connect returns 0 on buffer too small") {
    char buf[5];
    CHECK(Sa818CommandFormatter::connect(buf, sizeof(buf)) == 0);
}

TEST_CASE("Sa818CommandFormatter: set_group formats APRS frequency correctly") {
    char buf[64];
    // 144390000 Hz → "144.3900", wide_band=1, squelch=1
    size_t n = Sa818CommandFormatter::set_group(buf, sizeof(buf),
                                                 144390000u, 144390000u, 1, true);
    CHECK(n > 0);
    CHECK(std::string(buf, n) == "AT+DMOSETGROUP=1,144.3900,144.3900,0000,1,0000\r\n");
}

TEST_CASE("Sa818CommandFormatter: set_group narrow band uses BW=0") {
    char buf[64];
    size_t n = Sa818CommandFormatter::set_group(buf, sizeof(buf),
                                                 144390000u, 144390000u, 0, false);
    CHECK(n > 0);
    // First field after '=' should be '0'
    const std::string cmd(buf, n);
    CHECK(cmd.find("AT+DMOSETGROUP=0,") == 0);
}

TEST_CASE("Sa818CommandFormatter: set_group squelch encoded correctly") {
    char buf[64];
    size_t n = Sa818CommandFormatter::set_group(buf, sizeof(buf),
                                                 144390000u, 144390000u, 5, true);
    CHECK(n > 0);
    CHECK(std::string(buf, n) == "AT+DMOSETGROUP=1,144.3900,144.3900,0000,5,0000\r\n");
}

// ── Sa818ResponseParser tests ─────────────────────────────────────────────────

TEST_CASE("Sa818ResponseParser: parse_connect recognizes OK") {
    CHECK(Sa818ResponseParser::parse_connect("+DMOCONNECT:0\r\n")
          == Sa818ResponseParser::Result::Ok);
    CHECK(Sa818ResponseParser::parse_connect("+DMOCONNECT:0")
          == Sa818ResponseParser::Result::Ok);
}

TEST_CASE("Sa818ResponseParser: parse_connect recognizes error") {
    CHECK(Sa818ResponseParser::parse_connect("+DMOCONNECT:1\r\n")
          == Sa818ResponseParser::Result::Error);
}

TEST_CASE("Sa818ResponseParser: parse_connect rejects unknown") {
    CHECK(Sa818ResponseParser::parse_connect("")
          == Sa818ResponseParser::Result::Unknown);
    CHECK(Sa818ResponseParser::parse_connect("+DMOSETGROUP:0")
          == Sa818ResponseParser::Result::Unknown);
    CHECK(Sa818ResponseParser::parse_connect(nullptr)
          == Sa818ResponseParser::Result::Unknown);
}

TEST_CASE("Sa818ResponseParser: parse_set_group recognizes OK") {
    CHECK(Sa818ResponseParser::parse_set_group("+DMOSETGROUP:0\r\n")
          == Sa818ResponseParser::Result::Ok);
    CHECK(Sa818ResponseParser::parse_set_group("+DMOSETGROUP:0")
          == Sa818ResponseParser::Result::Ok);
}

TEST_CASE("Sa818ResponseParser: parse_set_group recognizes error") {
    CHECK(Sa818ResponseParser::parse_set_group("+DMOSETGROUP:1")
          == Sa818ResponseParser::Result::Error);
}

// ── Sa818Radio tests ──────────────────────────────────────────────────────────

TEST_CASE("Sa818Radio: ptt before init forces PTT off via callback") {
    MockTransport t;
    int ptt_false_calls = 0;
    Sa818Radio radio(t, [&](bool on){ if (!on) ++ptt_false_calls; });

    // init() calls force_ptt_off() immediately on entry
    t.queue_response("+DMOCONNECT:0");
    radio.init();
    CHECK(ptt_false_calls >= 1);
}

TEST_CASE("Sa818Radio: ptt after successful init succeeds") {
    MockTransport t;
    int ptt_true_calls = 0, ptt_false_calls = 0;
    Sa818Radio radio(t, [&](bool on){
        if (on) ++ptt_true_calls; else ++ptt_false_calls;
    });

    t.queue_response("+DMOCONNECT:0");
    REQUIRE(radio.init());

    CHECK(radio.ptt(true));
    CHECK(ptt_true_calls == 1);
    CHECK(radio.is_transmitting());

    CHECK(radio.ptt(false));
    CHECK(ptt_false_calls >= 2);  // init() + explicit ptt(false)
    CHECK_FALSE(radio.is_transmitting());
}

TEST_CASE("Sa818Radio: init sends DMOCONNECT and parses OK") {
    MockTransport t;
    t.queue_response("+DMOCONNECT:0");
    Sa818Radio radio(t, [](bool){});

    bool ok = radio.init();

    CHECK(ok);
    REQUIRE(t.writes.size() == 1);
    CHECK(t.writes[0] == "AT+DMOCONNECT\r\n");
}

TEST_CASE("Sa818Radio: init failure forces PTT off and returns false") {
    MockTransport t;
    int ptt_false_calls = 0;
    Sa818Radio radio(t, [&](bool on){ if (!on) ++ptt_false_calls; });

    t.queue_response("+DMOCONNECT:1");   // error response
    bool ok = radio.init();

    CHECK_FALSE(ok);
    CHECK(ptt_false_calls >= 1);
    // radio is in error state; subsequent calls must fail
    CHECK_FALSE(radio.set_freq(144390000u, 144390000u));
}

TEST_CASE("Sa818Radio: UART timeout during init forces PTT off") {
    MockTransport t;
    int ptt_false_calls = 0;
    Sa818Radio radio(t, [&](bool on){ if (!on) ++ptt_false_calls; });

    t.fail_read = true;          // simulate read timeout
    bool ok = radio.init();

    CHECK_FALSE(ok);
    CHECK(ptt_false_calls >= 1);
}

TEST_CASE("Sa818Radio: set_freq is idempotent with same values") {
    MockTransport t;
    Sa818Radio radio(t, [](bool){});

    t.queue_response("+DMOCONNECT:0");
    REQUIRE(radio.init());
    t.writes.clear();

    // First set_freq: sends AT+DMOSETGROUP
    t.queue_response("+DMOSETGROUP:0");
    REQUIRE(radio.set_freq(144390000u, 144390000u));
    CHECK(t.writes.size() == 1);
    t.writes.clear();

    // Second call with same values: no UART command
    CHECK(radio.set_freq(144390000u, 144390000u));
    CHECK(t.writes.empty());
}

TEST_CASE("Sa818Radio: set_freq failure forces PTT off") {
    MockTransport t;
    int ptt_false_calls = 0;
    Sa818Radio radio(t, [&](bool on){ if (!on) ++ptt_false_calls; });

    t.queue_response("+DMOCONNECT:0");
    REQUIRE(radio.init());
    ptt_false_calls = 0;   // reset counter after init

    t.queue_response("+DMOSETGROUP:1");   // error response
    bool ok = radio.set_freq(144390000u, 144390000u);

    CHECK_FALSE(ok);
    CHECK(ptt_false_calls >= 1);
}

TEST_CASE("Sa818Radio: ptt(false) succeeds even in error state") {
    MockTransport t;
    int ptt_false_calls = 0;
    Sa818Radio radio(t, [&](bool on){ if (!on) ++ptt_false_calls; });

    // Drive radio into error state via failed init
    t.fail_read = true;
    radio.init();
    ptt_false_calls = 0;   // reset

    // ptt(false) must succeed regardless
    CHECK(radio.ptt(false));
    CHECK(ptt_false_calls == 1);

    // ptt(true) must fail in error state
    CHECK_FALSE(radio.ptt(true));
}
