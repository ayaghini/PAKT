// test_ptt_watchdog.cpp – Host unit tests for PttWatchdog (FW-016)
//
// Covers: unit tests for all state transitions, safe_fn fire-once guarantee,
// recovery path, and integration with a RadioControlMock.
//
// Run: ./build/test_host/pakt_tests --reporters=console --no-intro

#include "doctest/doctest.h"
#include "pakt/PttWatchdog.h"

#include <atomic>
#include <cstdint>

using namespace pakt;

// ── Unit tests ────────────────────────────────────────────────────────────────

TEST_CASE("PttWatchdog: IDLE – tick returns false before first heartbeat") {
    int calls = 0;
    PttWatchdog wd([&calls]{ ++calls; }, 1000);

    CHECK_FALSE(wd.is_armed());
    CHECK_FALSE(wd.is_triggered());
    CHECK_FALSE(wd.tick(5000));   // well past any timeout
    CHECK(calls == 0);
}

TEST_CASE("PttWatchdog: ARMED – no timeout before threshold") {
    int calls = 0;
    PttWatchdog wd([&calls]{ ++calls; }, 1000);

    wd.heartbeat(0);
    CHECK(wd.is_armed());

    CHECK_FALSE(wd.tick(999));    // 999 ms elapsed – not yet
    CHECK(calls == 0);
}

TEST_CASE("PttWatchdog: timeout fires exactly at threshold") {
    int calls = 0;
    PttWatchdog wd([&calls]{ ++calls; }, 1000);

    wd.heartbeat(0);
    CHECK_FALSE(wd.tick(999));    // 999 ms – no fire
    CHECK(wd.tick(1000));         // 1000 ms – fires
    CHECK(calls == 1);
    CHECK(wd.is_triggered());
}

TEST_CASE("PttWatchdog: timeout fires at threshold + 1") {
    int calls = 0;
    PttWatchdog wd([&calls]{ ++calls; }, 1000);

    wd.heartbeat(0);
    CHECK(wd.tick(1001));
    CHECK(calls == 1);
}

TEST_CASE("PttWatchdog: tick is idempotent after trigger") {
    int calls = 0;
    PttWatchdog wd([&calls]{ ++calls; }, 1000);

    wd.heartbeat(0);
    wd.tick(1000);   // first trigger
    CHECK(calls == 1);

    // Additional ticks must not re-fire safe_fn
    wd.tick(2000);
    wd.tick(9000);
    CHECK(calls == 1);
}

TEST_CASE("PttWatchdog: heartbeat resets the stale timer") {
    int calls = 0;
    PttWatchdog wd([&calls]{ ++calls; }, 1000);

    wd.heartbeat(0);
    wd.tick(900);           // not yet

    wd.heartbeat(1000);     // refresh at t=1000
    CHECK_FALSE(wd.tick(1500));   // only 500 ms since last heartbeat – safe
    CHECK(calls == 0);

    CHECK(wd.tick(2000));  // 1000 ms since last heartbeat – fires
    CHECK(calls == 1);
}

TEST_CASE("PttWatchdog: heartbeat clears triggered state (recovery)") {
    int calls = 0;
    PttWatchdog wd([&calls]{ ++calls; }, 1000);

    wd.heartbeat(0);
    wd.tick(1000);   // trigger episode 1
    CHECK(wd.is_triggered());
    CHECK(calls == 1);

    // Recovery: fresh heartbeat
    wd.heartbeat(2000);
    CHECK_FALSE(wd.is_triggered());
    CHECK(wd.is_armed());

    // Episode 2: another timeout
    CHECK(wd.tick(3001));
    CHECK(calls == 2);
}

TEST_CASE("PttWatchdog: is_triggered transitions correctly") {
    int calls = 0;
    PttWatchdog wd([&calls]{ ++calls; }, 500);

    CHECK_FALSE(wd.is_triggered());
    wd.heartbeat(0);
    CHECK_FALSE(wd.is_triggered());
    wd.tick(500);
    CHECK(wd.is_triggered());
    wd.heartbeat(1000);   // recovery
    CHECK_FALSE(wd.is_triggered());
}

TEST_CASE("PttWatchdog: is_armed transitions correctly") {
    int calls = 0;
    PttWatchdog wd([&calls]{ ++calls; }, 500);

    CHECK_FALSE(wd.is_armed());
    wd.heartbeat(0);
    CHECK(wd.is_armed());
    wd.tick(500);        // trigger – armed stays true
    CHECK(wd.is_armed());
    wd.heartbeat(1000);  // recovery – still armed
    CHECK(wd.is_armed());
}

TEST_CASE("PttWatchdog: uint32_t wrap-around arithmetic is correct") {
    int calls = 0;
    PttWatchdog wd([&calls]{ ++calls; }, 1000);

    // Simulate heartbeat near rollover (0xFFFFFFFF - 500)
    constexpr uint32_t near_max = 0xFFFFFFFF - 500u;
    wd.heartbeat(near_max);

    // now_ms = near_max + 999 (wraps past 0)
    CHECK_FALSE(wd.tick(near_max + 999u));
    CHECK(calls == 0);

    // now_ms = near_max + 1000 (wraps past 0) – should fire
    CHECK(wd.tick(near_max + 1000u));
    CHECK(calls == 1);
}

// ── force_safe tests ──────────────────────────────────────────────────────────

TEST_CASE("PttWatchdog: force_safe fires safe_fn immediately") {
    int calls = 0;
    PttWatchdog wd([&calls]{ ++calls; }, 10000);

    wd.heartbeat(0);
    CHECK_FALSE(wd.is_triggered());

    wd.force_safe(500);
    CHECK(calls == 1);
    CHECK(wd.is_triggered());
}

TEST_CASE("PttWatchdog: force_safe is idempotent") {
    int calls = 0;
    PttWatchdog wd([&calls]{ ++calls; }, 10000);

    wd.heartbeat(0);
    wd.force_safe(100);
    wd.force_safe(200);
    wd.force_safe(300);
    CHECK(calls == 1);
}

TEST_CASE("PttWatchdog: force_safe in IDLE (no heartbeat) fires safe_fn once") {
    int calls = 0;
    PttWatchdog wd([&calls]{ ++calls; }, 10000);

    // No heartbeat – watchdog is IDLE
    wd.force_safe(0);
    CHECK(calls == 1);
    CHECK(wd.is_triggered());

    wd.force_safe(0);   // idempotent
    CHECK(calls == 1);
}

TEST_CASE("PttWatchdog: force_safe after tick timeout does not double-fire") {
    int calls = 0;
    PttWatchdog wd([&calls]{ ++calls; }, 1000);

    wd.heartbeat(0);
    wd.tick(1000);       // first trigger via tick
    CHECK(calls == 1);

    wd.force_safe(1500); // already triggered; CAS should fail
    CHECK(calls == 1);
}

// ── PttController tests ───────────────────────────────────────────────────────
//
// Tests the settable safe-off hook that decouples the watchdog from the
// concrete SA818 radio driver.

#include "pakt/PttController.h"

TEST_CASE("PttController: ptt_safe_off with no registration is a safe no-op") {
    ptt_register_safe_off(std::function<void()>{});  // clear any prior state
    CHECK_FALSE(ptt_is_registered());
    ptt_safe_off();   // must not crash
    CHECK_FALSE(ptt_is_registered());
}

TEST_CASE("PttController: ptt_safe_off fires registered callback") {
    int calls = 0;
    ptt_register_safe_off([&calls]{ ++calls; });
    CHECK(ptt_is_registered());
    ptt_safe_off();
    CHECK(calls == 1);
    ptt_register_safe_off(std::function<void()>{});  // cleanup
}

TEST_CASE("PttController: watchdog trigger invokes ptt_safe_off exactly once") {
    int calls = 0;
    ptt_register_safe_off([&calls]{ ++calls; });
    PttWatchdog wd([]{ ptt_safe_off(); }, 1000);
    wd.heartbeat(0);
    wd.tick(1000);   // fires safe_fn → ptt_safe_off() → calls++
    CHECK(calls == 1);
    wd.tick(2000);   // already triggered – must not re-fire
    CHECK(calls == 1);
    ptt_register_safe_off(std::function<void()>{});  // cleanup
}

TEST_CASE("PttController: registration state transitions – unregistered to registered to cleared") {
    ptt_register_safe_off(std::function<void()>{});  // start clean
    CHECK_FALSE(ptt_is_registered());                // unregistered

    int calls = 0;
    ptt_register_safe_off([&calls]{ ++calls; });
    CHECK(ptt_is_registered());                      // registered

    ptt_safe_off();
    CHECK(calls == 1);

    ptt_register_safe_off(std::function<void()>{});  // clear
    CHECK_FALSE(ptt_is_registered());                // back to unregistered
    ptt_safe_off();                                  // must not re-fire
    CHECK(calls == 1);                               // still 1
}

// ── Integration tests: RadioControlMock ──────────────────────────────────────
//
// Simulates the production safe_fn wiring:
//   PttWatchdog safe_fn = [&radio]{ radio.ptt(false); }
//
// No IRadioControl interface needed; the mock captures ptt() calls directly.

struct RadioControlMock {
    int ptt_false_calls = 0;
    int ptt_true_calls  = 0;

    void ptt(bool value) {
        if (value) ++ptt_true_calls;
        else       ++ptt_false_calls;
    }
};

TEST_CASE("PttWatchdog integration: stale heartbeat triggers RadioControlMock.ptt(false)") {
    RadioControlMock radio;
    PttWatchdog wd([&radio]{ radio.ptt(false); }, 1000);

    // Start transmitting
    wd.heartbeat(0);
    radio.ptt(true);

    // Heartbeat goes stale
    wd.tick(500);    // not yet
    CHECK(radio.ptt_false_calls == 0);

    wd.tick(1000);   // watchdog fires
    CHECK(radio.ptt_false_calls == 1);
    CHECK(radio.ptt_true_calls  == 1);  // the one manual ptt(true) above
}

TEST_CASE("PttWatchdog integration: active heartbeat prevents RadioControlMock.ptt(false)") {
    RadioControlMock radio;
    PttWatchdog wd([&radio]{ radio.ptt(false); }, 1000);

    wd.heartbeat(0);
    radio.ptt(true);

    // Keep sending heartbeats – watchdog must not fire
    for (uint32_t t = 500; t <= 5000; t += 500) {
        wd.heartbeat(t);
        wd.tick(t);
    }

    CHECK(radio.ptt_false_calls == 0);
    CHECK_FALSE(wd.is_triggered());
}

TEST_CASE("PttWatchdog integration: recovery cycle – radio safe, recover, safe again") {
    RadioControlMock radio;
    PttWatchdog wd([&radio]{ radio.ptt(false); }, 1000);

    // Episode 1
    wd.heartbeat(0);
    radio.ptt(true);
    wd.tick(1000);
    CHECK(radio.ptt_false_calls == 1);

    // Recovery
    wd.heartbeat(2000);
    radio.ptt(true);
    CHECK_FALSE(wd.is_triggered());

    // Episode 2 – verify watchdog re-arms
    wd.tick(2500);    // 500 ms since heartbeat – safe
    CHECK(radio.ptt_false_calls == 1);

    wd.tick(3000);    // 1000 ms since heartbeat – fires again
    CHECK(radio.ptt_false_calls == 2);
}
