#pragma once

// Bell 202 AFSK demodulator (FW-006)
//
// Converts a PCM audio sample stream into raw AX.25 frame bytes.
// Pipeline: bandpass filter → envelope detector → symbol slicer →
//           symbol timing → NRZI decode → HDLC bit unstuffing →
//           flag detection → frame assembly → FCS check → callback.
//
// Decoded frames are delivered via a callback; the callback receives the
// raw frame bytes (same format as ax25::encode output) which the caller
// passes to ax25::decode() for address/FCS parsing.

#include <cstddef>
#include <cstdint>
#include <functional>

namespace pakt {

class AfskDemodulator
{
public:
    static constexpr size_t kMaxFrameBytes = 330;

    // frame_cb is called for each successfully demodulated frame.
    // data: raw frame bytes (address through FCS, no flags, not bit-stuffed).
    // len:  byte count.
    using FrameCallback = std::function<void(const uint8_t *data, size_t len)>;

    explicit AfskDemodulator(uint32_t sample_rate_hz, FrameCallback frame_cb);

    // Push PCM samples through the demodulation pipeline.
    // Call repeatedly as audio blocks arrive from IAudioIO::read_samples().
    void process(const int16_t *samples, size_t count);

    // Reset all demodulator state (filters, timing, HDLC state machine).
    void reset();

private:
    // ── Biquad bandpass filter (Direct Form II Transposed) ──────────────────
    struct Biquad {
        float b0{0}, b2{0};     // b1 = 0 for symmetric bandpass
        float a1{0}, a2{0};
        float z1{0}, z2{0};

        float process(float x);

        // Factory: bandpass centred at freq_hz with quality factor q.
        static Biquad bandpass(float freq_hz, float q, float sample_rate_hz);
    };

    // ── First-order IIR envelope detector (lowpass on |x|) ─────────────────
    struct EnvDetect {
        float alpha{0};  // attack/decay coefficient
        float state{0};

        float process(float x);

        // Factory: lowpass with cutoff at fc_hz.
        static EnvDetect make(float fc_hz, float sample_rate_hz);
    };

    void process_sample(float s);
    void process_symbol(bool mark_detected);
    void process_bit(bool bit);     // NRZI decoder feeds into HDLC assembler
    void assemble_bit(bool bit);    // HDLC bit unstuffing and frame assembly
    void dispatch_frame();          // validate FCS and call frame_cb_

    uint32_t     sample_rate_;
    FrameCallback frame_cb_;

    // Signal chain
    Biquad    mark_bp_;
    Biquad    space_bp_;
    EnvDetect mark_env_;
    EnvDetect space_env_;

    // Symbol timing (transition-tracking synchroniser)
    double  samples_per_symbol_;
    double  symbol_phase_;   // counts up; decision fires when >= samples_per_symbol_
    bool    prev_decision_;  // last decision level (for transition detection)

    // NRZI decoder
    bool nrzi_prev_;  // last decoded level

    // HDLC state machine
    uint8_t sr_;           // 8-bit shift register (newest bit at bit 0)
    int     ones_count_;   // consecutive 1s since last 0 (for bit stuffing)
    bool    in_frame_;     // true while collecting frame bits
    uint8_t bit_buf_;      // current byte being assembled (LSB first)
    int     bit_pos_;      // bit position within bit_buf_ (0–7)
    uint8_t frame_buf_[kMaxFrameBytes];
    size_t  frame_len_;    // bytes in frame_buf_ so far
};

} // namespace pakt
