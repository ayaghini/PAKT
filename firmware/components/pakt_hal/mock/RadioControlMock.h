#pragma once

// RadioControlMock – test double for IRadioControl
//
// Records all calls for assertion in unit tests.
// ptt() before init() is always rejected and leaves PTT=off,
// matching the real driver's safety contract.

#include "pakt/IRadioControl.h"

namespace pakt::mock {

class RadioControlMock final : public IRadioControl
{
public:
    bool init() override
    {
        initialized_ = true;
        transmitting_ = false; // PTT always off on init
        return true;
    }

    bool set_freq(uint32_t rx_hz, uint32_t tx_hz) override
    {
        if (!initialized_) return false;
        rx_hz_ = rx_hz;
        tx_hz_ = tx_hz;
        return true;
    }

    bool set_squelch(uint8_t level) override
    {
        if (!initialized_) return false;
        squelch_ = level;
        return true;
    }

    bool set_power(RadioPower power) override
    {
        if (!initialized_) return false;
        power_ = power;
        return true;
    }

    bool ptt(bool on) override
    {
        if (!initialized_) {
            transmitting_ = false; // safe: reject and ensure off
            return false;
        }
        transmitting_ = on;
        return true;
    }

    bool is_transmitting() const override { return transmitting_; }

    // ── Test helpers ──────────────────────────────────────────────────────────

    bool        is_initialized() const { return initialized_; }
    uint32_t    rx_freq()        const { return rx_hz_; }
    uint32_t    tx_freq()        const { return tx_hz_; }
    uint8_t     squelch()        const { return squelch_; }
    RadioPower  power()          const { return power_; }

private:
    bool       initialized_{false};
    bool       transmitting_{false};
    uint32_t   rx_hz_{0};
    uint32_t   tx_hz_{0};
    uint8_t    squelch_{0};
    RadioPower power_{RadioPower::Low};
};

} // namespace pakt::mock
