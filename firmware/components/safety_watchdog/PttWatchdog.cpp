// PttWatchdog.cpp – FW-016: PTT safe-off watchdog

#include "pakt/PttWatchdog.h"

namespace pakt {

PttWatchdog::PttWatchdog(std::function<void()> safe_fn, uint32_t timeout_ms)
    : safe_fn_(std::move(safe_fn))
    , timeout_ms_(timeout_ms)
{}

void PttWatchdog::heartbeat(uint32_t now_ms)
{
    last_heartbeat_ms_.store(now_ms, std::memory_order_release);
    armed_.store(true, std::memory_order_release);
    // Clear triggered so tick() can fire again if the system goes stale again.
    triggered_.store(false, std::memory_order_release);
}

bool PttWatchdog::tick(uint32_t now_ms)
{
    // IDLE: no heartbeat received yet — nothing to time out.
    if (!armed_.load(std::memory_order_acquire)) return false;

    // Already triggered this episode; safe_fn already fired.
    // Caller must receive a fresh heartbeat() to re-arm.
    if (triggered_.load(std::memory_order_acquire)) return false;

    uint32_t last = last_heartbeat_ms_.load(std::memory_order_acquire);

    // uint32_t subtraction wraps correctly for intervals < 2^31 ms (~24 days).
    if ((now_ms - last) < timeout_ms_) return false;

    // Attempt to be the first to set triggered_ (guards against a concurrent
    // force_safe() racing with this tick()).
    bool expected = false;
    if (!triggered_.compare_exchange_strong(expected, true,
                                             std::memory_order_acq_rel)) {
        return false;  // force_safe() won the race; safe_fn already fired
    }

    if (safe_fn_) safe_fn_();
    return true;
}

void PttWatchdog::force_safe(uint32_t /*now_ms*/)
{
    // Attempt to be the first to set triggered_.
    bool expected = false;
    if (!triggered_.compare_exchange_strong(expected, true,
                                             std::memory_order_acq_rel)) {
        return;  // already triggered; safe_fn already fired
    }

    if (safe_fn_) safe_fn_();
}

bool PttWatchdog::is_triggered() const
{
    return triggered_.load(std::memory_order_acquire);
}

bool PttWatchdog::is_armed() const
{
    return armed_.load(std::memory_order_acquire);
}

} // namespace pakt
