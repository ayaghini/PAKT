#pragma once
// Sa818Radio – IRadioControl implementation for the SA818-V VHF module
//
// Communicates via an injectable ISa818Transport (UART abstraction) so this
// class is fully host-testable with a mock transport.
//
// PTT is driven through a PttGpioFn callback where:
//   ptt_fn_(true)  → assert PTT  (start transmitting)
//   ptt_fn_(false) → deassert PTT (stop transmitting, radio idle)
// The concrete lambda in radio_task inverts for active-low GPIO:
//   [](bool on){ gpio_set_level(GPIO11, on ? 0 : 1); }
//
// Contract (IRadioControl §A):
//   init()      : calls ptt_fn_(false) immediately; PTT always off on return.
//   set_freq()  : idempotent — same rx/tx values skip the UART round-trip.
//   error path  : any UART timeout or write failure calls force_ptt_off()
//                 and sets error_ = true; all subsequent calls return false
//                 (ptt(false) remains safe).

#include "pakt/IRadioControl.h"
#include "pakt/ISa818Transport.h"
#include <functional>
#include <cstdint>

namespace pakt {

// Callback type for PTT GPIO control.
// true  = assert PTT (TX on);  false = deassert PTT (TX off, safe state).
using PttGpioFn = std::function<void(bool)>;

class Sa818Radio final : public IRadioControl
{
public:
    static constexpr uint32_t kDefaultTimeoutMs = 500;
    static constexpr uint8_t  kDefaultSquelch   = 1;

    Sa818Radio(ISa818Transport &transport, PttGpioFn ptt_fn);

    // ── IRadioControl ─────────────────────────────────────────────────────────
    bool init()                                   override;
    bool set_freq(uint32_t rx_hz, uint32_t tx_hz) override;
    bool set_squelch(uint8_t level)               override;
    bool set_power(RadioPower power)              override;
    bool ptt(bool on)                             override;
    bool is_transmitting() const                  override { return transmitting_; }

private:
    // Write cmd, read response into resp_buf (null-terminated on success).
    // Calls force_ptt_off() on any UART error and returns false.
    bool exchange(const char *cmd, size_t cmd_len,
                  char *resp_buf, size_t resp_len,
                  uint32_t timeout_ms = kDefaultTimeoutMs);

    void force_ptt_off();

    ISa818Transport &transport_;
    PttGpioFn        ptt_fn_;

    uint32_t   rx_hz_       = 0;
    uint32_t   tx_hz_       = 0;
    uint8_t    squelch_     = kDefaultSquelch;
    RadioPower power_       = RadioPower::Low;
    bool       transmitting_ = false;
    bool       initialized_  = false;
    bool       error_        = false;
};

} // namespace pakt
