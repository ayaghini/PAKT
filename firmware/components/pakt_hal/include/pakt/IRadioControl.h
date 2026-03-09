#pragma once

// IRadioControl – SA818 VHF radio control abstraction
//
// Contract (from architecture_contracts.md §A, §G):
//   - set_freq() is idempotent: calling it with the same value is always safe.
//   - ptt() always transitions to an explicit on/off state; never left ambiguous.
//   - Default and fallback state is PTT=off.
//   - Any unrecoverable error must call ptt(false) before returning.

#include <cstdint>

namespace pakt {

enum class RadioPower : uint8_t {
    Low  = 0,  // SA818 low-power TX (≈0.5 W)
    High = 1,  // SA818 high-power TX (≈1 W)
};

class IRadioControl
{
public:
    virtual ~IRadioControl() = default;

    // Open the UART link to the SA818 and send an initial configuration.
    // PTT is guaranteed off on return, even on failure.
    // Returns false on communication timeout or unexpected response.
    virtual bool init() = 0;

    // Set RX and TX frequency in Hz.
    // Valid range for VHF SA818: 134 000 000 – 174 000 000 Hz.
    // Idempotent: safe to call with the same values repeatedly.
    // Returns false if the module rejects the command.
    virtual bool set_freq(uint32_t rx_hz, uint32_t tx_hz) = 0;

    // Set squelch level: 0 = carrier squelch open, 1–8 per SA818 spec.
    virtual bool set_squelch(uint8_t level) = 0;

    // Set TX output power.
    virtual bool set_power(RadioPower power) = 0;

    // Assert PTT on or off.
    // ptt(true)  starts transmission; must be followed by ptt(false).
    // ptt(false) is always safe to call, including when already off.
    // On any internal error the implementation must force PTT off and
    // return false.
    virtual bool ptt(bool on) = 0;

    // Returns the last committed PTT state (cached, not polled from radio).
    // A return value of true means the SA818 PTT line is currently asserted.
    virtual bool is_transmitting() const = 0;
};

} // namespace pakt
