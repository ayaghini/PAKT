#pragma once

// Bell 202 AFSK modulator (FW-007)
//
// Converts raw AX.25 frame bytes into a PCM audio sample stream.
// Handles: HDLC preamble/tail flags, bit stuffing, NRZI encoding,
//          phase-continuous dual-tone generation.
//
// Parameters (Bell 202 / APRS standard):
//   Mark  frequency : 1200 Hz  (binary 1)
//   Space frequency : 2200 Hz  (binary 0)
//   Baud rate       : 1200 baud
//   Sample rate     : configured at construction (8000 or 16000 Hz)
//
// Input : raw frame bytes from ax25::encode() — no flags, no bit stuffing.
// Output: int16_t PCM samples at the configured sample rate.

#include <cstddef>
#include <cstdint>

namespace pakt {

class AfskModulator
{
public:
    static constexpr uint32_t kMarkHz       = 1200;
    static constexpr uint32_t kSpaceHz      = 2200;
    static constexpr uint32_t kBaudRate     = 1200;
    static constexpr int      kPreambleFlags = 20;   // flags before start flag
    static constexpr int      kTailFlags    = 3;    // flags after last data bit

    explicit AfskModulator(uint32_t sample_rate_hz);

    // Modulate frame bytes into PCM samples.
    // data     : raw frame bytes (address + ctrl + pid + info + fcs from ax25::encode)
    // data_len : byte count
    // out      : caller-supplied output buffer
    // out_max  : capacity of out in samples
    // Returns  : number of samples written, or 0 if out_max is too small.
    size_t modulate_frame(const uint8_t *data, size_t data_len,
                          int16_t *out, size_t out_max);

    // Reset all internal state (phase, NRZI, stuffing counter).
    // Call between frames to ensure a clean start.
    void reset();

private:
    // Emit the samples for exactly one bit period at the current tone.
    // Uses a fractional accumulator to maintain correct average baud rate.
    // Returns true if the buffer was full before all samples could be written
    // (genuine truncation); false if all samples were written (pos may equal
    // out_max on an exact fit — that is not truncation).
    bool emit_bit_samples(int16_t *out, size_t &pos, size_t out_max);

    // Emit one data bit with NRZI encoding and bit stuffing.
    // Returns true on truncation (propagated from emit_bit_samples).
    bool emit_data_bit(bool bit, int16_t *out, size_t &pos, size_t out_max);

    // Emit one HDLC flag byte (0x7E) without bit stuffing.
    // Returns true on truncation.
    bool emit_flag(int16_t *out, size_t &pos, size_t out_max);

    uint32_t sample_rate_;
    double   phase_acc_;        // current phase in radians [0, 2π)
    bool     nrzi_tone_;        // true = mark (1200 Hz), false = space (2200 Hz)
    int      stuff_count_;      // consecutive data 1s since last 0 or flag
    double   sample_acc_;       // fractional sample accumulator for correct baud timing
    double   samples_per_bit_;  // e.g. 6.667 at 8000/1200
};

} // namespace pakt
