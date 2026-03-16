#pragma once
// PttWatchdog.h – FW-016: PTT safe-off watchdog
//
// Forces PTT off when the APRS/radio heartbeat goes stale.
//
// Safety contract (mirrors IRadioControl §A,§G):
//   - Default/fallback state is PTT OFF.
//   - If no heartbeat() is ever called (IDLE), the watchdog never fires
//     automatically — PTT was never asserted, so there is nothing to time out.
//   - Once armed (after the first heartbeat), a stale heartbeat fires safe_fn
//     exactly once per timeout event.
//   - A fresh heartbeat() call re-arms the watchdog and clears any prior
//     triggered state, enabling recovery after a timeout.
//   - force_safe() fires safe_fn immediately regardless of the timer state.
//     It is idempotent: calling it multiple times only fires safe_fn once.
//
// Thread safety:
//   heartbeat()   — may be called from any task (e.g. aprs_task)
//   tick()        — must be called from a single supervisor task
//   force_safe()  — may be called from any task
//   is_triggered() / is_armed() — read-only; safe from any task
//   All shared state is protected by std::atomic with acquire/release ordering.
//
// Pure C++ — no ESP-IDF or FreeRTOS dependencies; host-testable.

#include <atomic>
#include <cstdint>
#include <functional>

namespace pakt {

class PttWatchdog {
public:
    // Default timeout: 10 s.  Fires if no heartbeat is received within this window.
    // A typical APRS task at 1 Hz tick gives 10 missed beats before the watchdog fires.
    static constexpr uint32_t kDefaultTimeoutMs = 10'000;

    // safe_fn   : called once when the watchdog triggers (must de-assert PTT).
    //             In production this calls IRadioControl::ptt(false) and logs.
    // timeout_ms: stale window in milliseconds; must be > 0.
    explicit PttWatchdog(std::function<void()> safe_fn,
                         uint32_t timeout_ms = kDefaultTimeoutMs);

    // ── Producer API (called from APRS/radio task) ─────────────────────────────

    // Signal that the system is healthy at time now_ms.
    // - Arms the watchdog on first call.
    // - Resets the stale timer.
    // - Clears any prior triggered state (enables recovery after a timeout).
    void heartbeat(uint32_t now_ms);

    // ── Consumer API (called from supervisor/watchdog task) ───────────────────

    // Advance the watchdog clock.  Call periodically (e.g. every 500 ms).
    // Returns true exactly once when the timeout fires (first trigger only).
    // Returns false if: not armed, already triggered, or timer not yet expired.
    bool tick(uint32_t now_ms);

    // ── Emergency API (may be called from any task) ────────────────────────────

    // Immediately fire safe_fn regardless of the timer state.
    // Idempotent: safe_fn is called at most once per triggered episode.
    void force_safe(uint32_t now_ms);

    // ── Inspection ────────────────────────────────────────────────────────────

    // Returns true if the watchdog has triggered and not yet recovered via heartbeat.
    bool is_triggered() const;

    // Returns true if at least one heartbeat() has been received (watchdog is armed).
    bool is_armed() const;

private:
    std::function<void()>  safe_fn_;
    uint32_t               timeout_ms_;
    std::atomic<uint32_t>  last_heartbeat_ms_{0};
    std::atomic<bool>      armed_{false};
    std::atomic<bool>      triggered_{false};
};

} // namespace pakt
