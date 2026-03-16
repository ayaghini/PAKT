#pragma once
// PttController.h – Settable PTT safe-off hook (FW-016)
//
// Decouples the watchdog trigger from the concrete radio driver.
// The watchdog's safe_fn calls ptt_safe_off(); the SA818 radio driver
// registers the real de-assert implementation via ptt_register_safe_off()
// when it initialises.
//
// Until a callback is registered, ptt_safe_off() is a safe no-op:
// hardware PTT default state is OFF, so no action is needed; the
// watchdog's own log documents that the trigger fired.
//
// Thread-safety:
//   ptt_register_safe_off() – call once from radio_task init, before
//                             aprs_task publishes g_ptt_watchdog.
//   ptt_safe_off()          – called from watchdog_task (or any task
//                             that calls force_safe()).
//   ptt_is_registered()     – read-only; safe from any task.
//
// Pure C++ – no ESP-IDF or FreeRTOS dependencies; host-testable.

#include <functional>

namespace pakt {

// Register the callback to invoke when PTT must be de-asserted.
// Passing an empty std::function clears any existing registration.
// Not thread-safe at registration time; register before the watchdog
// can fire (i.e. before aprs_task enters its loop).
void ptt_register_safe_off(std::function<void()> fn);

// De-assert PTT.  Calls the registered callback if one is set.
// Safe no-op if no callback is registered.
void ptt_safe_off();

// Returns true if a safe-off callback has been registered.
bool ptt_is_registered();

} // namespace pakt
