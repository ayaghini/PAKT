// Bell 202 AFSK modulator implementation

#include "pakt/AfskModulator.h"

#include <cmath>
#include <cstring>

namespace pakt {

static constexpr double kTwoPi    = 6.283185307179586;
static constexpr int16_t kAmplitude = 28000; // ~85% of INT16_MAX

AfskModulator::AfskModulator(uint32_t sample_rate_hz)
    : sample_rate_(sample_rate_hz)
    , phase_acc_(0.0)
    , nrzi_tone_(true)   // start at mark
    , stuff_count_(0)
    , sample_acc_(0.0)
    , samples_per_bit_(static_cast<double>(sample_rate_hz) / kBaudRate)
{}

void AfskModulator::reset()
{
    phase_acc_   = 0.0;
    nrzi_tone_   = true;
    stuff_count_ = 0;
    sample_acc_  = 0.0;
}

// Emit samples for one bit period at the current tone frequency.
// Uses a fractional accumulator so the average baud rate is exactly
// sample_rate / kBaudRate (e.g. 6.667 samples/bit at 8 kHz).
void AfskModulator::emit_bit_samples(int16_t *out, size_t &pos, size_t out_max)
{
    double freq  = nrzi_tone_ ? static_cast<double>(kMarkHz)
                              : static_cast<double>(kSpaceHz);
    double delta = kTwoPi * freq / sample_rate_;

    sample_acc_ += samples_per_bit_;
    int n = static_cast<int>(sample_acc_);
    sample_acc_ -= static_cast<double>(n);

    for (int i = 0; i < n && pos < out_max; ++i) {
        out[pos++] = static_cast<int16_t>(kAmplitude * std::sin(phase_acc_));
        phase_acc_ += delta;
        if (phase_acc_ >= kTwoPi) phase_acc_ -= kTwoPi;
    }
}

// Emit one data bit:
//   NRZI encoding   : 0 = transition, 1 = no transition
//   Bit stuffing    : after 5 consecutive 1s, insert a stuffed 0
void AfskModulator::emit_data_bit(bool bit, int16_t *out, size_t &pos, size_t out_max)
{
    if (!bit) {
        nrzi_tone_ = !nrzi_tone_; // 0 → transition
    }
    emit_bit_samples(out, pos, out_max);

    if (bit) {
        if (++stuff_count_ == 5) {
            // Insert a stuffed 0: forces a transition, never counted
            nrzi_tone_   = !nrzi_tone_;
            emit_bit_samples(out, pos, out_max);
            stuff_count_ = 0;
        }
    } else {
        stuff_count_ = 0;
    }
}

// Emit one HDLC flag (0x7E = 0b01111110), LSB first, without bit stuffing.
void AfskModulator::emit_flag(int16_t *out, size_t &pos, size_t out_max)
{
    static constexpr uint8_t kFlag = 0x7E;
    for (int i = 0; i < 8; ++i) {
        bool bit = (kFlag >> i) & 1u;
        if (!bit) nrzi_tone_ = !nrzi_tone_;
        emit_bit_samples(out, pos, out_max);
    }
    stuff_count_ = 0; // flags reset the stuffing counter
}

size_t AfskModulator::modulate_frame(const uint8_t *data, size_t data_len,
                                      int16_t *out, size_t out_max)
{
    if (!data || !out || data_len == 0 || out_max == 0) return 0;

    size_t pos = 0;

    // Preamble: N flags to let receiver PLL lock
    for (int i = 0; i < kPreambleFlags; ++i) {
        emit_flag(out, pos, out_max);
    }

    // Start flag
    emit_flag(out, pos, out_max);

    // Frame data (with bit stuffing), LSB of each byte first
    for (size_t b = 0; b < data_len; ++b) {
        for (int bit_idx = 0; bit_idx < 8; ++bit_idx) {
            emit_data_bit((data[b] >> bit_idx) & 1u, out, pos, out_max);
        }
    }

    // Tail flags
    for (int i = 0; i < kTailFlags; ++i) {
        emit_flag(out, pos, out_max);
    }

    return pos;
}

} // namespace pakt
