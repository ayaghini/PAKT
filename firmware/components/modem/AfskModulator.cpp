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
//
// Returns true if the output buffer was full before all samples could be
// written (genuine truncation). Returns false if all samples were written
// successfully — including the case where pos reaches out_max exactly on
// the last sample of this bit (exact-fit is NOT truncation).
bool AfskModulator::emit_bit_samples(int16_t *out, size_t &pos, size_t out_max)
{
    double freq  = nrzi_tone_ ? static_cast<double>(kMarkHz)
                              : static_cast<double>(kSpaceHz);
    double delta = kTwoPi * freq / sample_rate_;

    sample_acc_ += samples_per_bit_;
    int n = static_cast<int>(sample_acc_);
    sample_acc_ -= static_cast<double>(n);

    for (int i = 0; i < n; ++i) {
        if (pos >= out_max) return true;  // buffer full before this sample — truncated
        out[pos++] = static_cast<int16_t>(kAmplitude * std::sin(phase_acc_));
        phase_acc_ += delta;
        if (phase_acc_ >= kTwoPi) phase_acc_ -= kTwoPi;
    }
    return false;  // all n samples written; pos may equal out_max but that is not truncation
}

// Emit one data bit with NRZI encoding and bit stuffing.
// Returns true on truncation (propagated from emit_bit_samples).
bool AfskModulator::emit_data_bit(bool bit, int16_t *out, size_t &pos, size_t out_max)
{
    if (!bit) {
        nrzi_tone_ = !nrzi_tone_; // 0 → transition
    }
    bool truncated = emit_bit_samples(out, pos, out_max);

    if (bit) {
        if (++stuff_count_ == 5) {
            // Insert a stuffed 0: forces a transition, never counted
            nrzi_tone_   = !nrzi_tone_;
            if (!truncated)
                truncated = emit_bit_samples(out, pos, out_max);
            stuff_count_ = 0;
        }
    } else {
        stuff_count_ = 0;
    }
    return truncated;
}

// Emit one HDLC flag (0x7E = 0b01111110), LSB first, without bit stuffing.
// Returns true on truncation.
bool AfskModulator::emit_flag(int16_t *out, size_t &pos, size_t out_max)
{
    static constexpr uint8_t kFlag = 0x7E;
    bool truncated = false;
    for (int i = 0; i < 8; ++i) {
        bool bit = (kFlag >> i) & 1u;
        if (!bit) nrzi_tone_ = !nrzi_tone_;
        if (!truncated)
            truncated = emit_bit_samples(out, pos, out_max);
    }
    stuff_count_ = 0; // flags reset the stuffing counter
    return truncated;
}

size_t AfskModulator::modulate_frame(const uint8_t *data, size_t data_len,
                                      int16_t *out, size_t out_max)
{
    if (!data || !out || data_len == 0 || out_max == 0) return 0;

    size_t pos = 0;
    bool truncated = false;

    // Preamble: N flags to let receiver PLL lock
    for (int i = 0; i < kPreambleFlags && !truncated; ++i)
        truncated = emit_flag(out, pos, out_max);

    // Start flag
    if (!truncated)
        truncated = emit_flag(out, pos, out_max);

    // Frame data (with bit stuffing), LSB of each byte first
    for (size_t b = 0; b < data_len && !truncated; ++b) {
        for (int bit_idx = 0; bit_idx < 8 && !truncated; ++bit_idx)
            truncated = emit_data_bit((data[b] >> bit_idx) & 1u, out, pos, out_max);
    }

    // Tail flags
    for (int i = 0; i < kTailFlags && !truncated; ++i)
        truncated = emit_flag(out, pos, out_max);

    // truncated is true only when a write was skipped due to buffer overflow.
    // pos == out_max with truncated == false means the frame exactly filled
    // the buffer — that is valid success.
    return truncated ? 0 : pos;
}

} // namespace pakt
