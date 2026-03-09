// Bell 202 AFSK demodulator implementation
//
// DSP pipeline per sample:
//   1. Biquad bandpass @ 1200 Hz  (mark filter)
//   2. Biquad bandpass @ 2200 Hz  (space filter)
//   3. Envelope detection on each (IIR lowpass on |output|)
//   4. Decision: mark_env > space_env → mark (1)
//   5. Symbol timing: transition-tracking synchroniser
//   6. Per-symbol: NRZI decode → HDLC bit unstuffing → frame assembly

#include "pakt/AfskDemodulator.h"
#include "pakt/Ax25.h"

#include <cmath>
#include <cstring>

namespace pakt {

static constexpr double kTwoPi = 6.283185307179586;

// ── Biquad bandpass filter ────────────────────────────────────────────────────

AfskDemodulator::Biquad
AfskDemodulator::Biquad::bandpass(float freq_hz, float q, float sample_rate_hz)
{
    float w0    = static_cast<float>(kTwoPi) * freq_hz / sample_rate_hz;
    float alpha = std::sin(w0) / (2.0f * q);
    float a0    = 1.0f + alpha;

    Biquad bq;
    bq.b0 =  alpha / a0;             // b0 / a0
    bq.b2 = -alpha / a0;             // b2 / a0  (b1 = 0)
    bq.a1 = -2.0f * std::cos(w0) / a0;
    bq.a2 = (1.0f - alpha) / a0;
    return bq;
}

// Direct Form II Transposed: y[n] = b0·x + z1; z1 = -a1·y + z2 + b1·x; z2 = b2·x - a2·y
// Since b1 = 0 for our symmetric bandpass, the middle term simplifies.
float AfskDemodulator::Biquad::process(float x)
{
    float y = b0 * x + z1;
    z1      = -a1 * y + z2;      // b1·x = 0
    z2      = b2 * x - a2 * y;
    return y;
}

// ── Envelope detector ─────────────────────────────────────────────────────────

AfskDemodulator::EnvDetect
AfskDemodulator::EnvDetect::make(float fc_hz, float sample_rate_hz)
{
    EnvDetect e;
    e.alpha = 1.0f - std::exp(-static_cast<float>(kTwoPi) * fc_hz / sample_rate_hz);
    return e;
}

float AfskDemodulator::EnvDetect::process(float x)
{
    float abs_x = (x >= 0.0f) ? x : -x;
    state += alpha * (abs_x - state);
    return state;
}

// ── Constructor / reset ───────────────────────────────────────────────────────

AfskDemodulator::AfskDemodulator(uint32_t sample_rate_hz, FrameCallback frame_cb)
    : sample_rate_(sample_rate_hz)
    , frame_cb_(std::move(frame_cb))
    , samples_per_symbol_(static_cast<double>(sample_rate_hz) / 1200.0)
    , symbol_phase_(0.0)
    , prev_decision_(false)
    , nrzi_prev_(false)
    , sr_(0)
    , ones_count_(0)
    , in_frame_(false)
    , bit_buf_(0)
    , bit_pos_(0)
    , frame_len_(0)
{
    // Q = 3.5 gives ~340 Hz bandwidth at 1200 Hz and ~630 Hz at 2200 Hz —
    // enough to separate the two Bell 202 tones with good rejection.
    static constexpr float kQ = 3.5f;
    mark_bp_  = Biquad::bandpass(1200.0f, kQ, static_cast<float>(sample_rate_hz));
    space_bp_ = Biquad::bandpass(2200.0f, kQ, static_cast<float>(sample_rate_hz));

    // Envelope lowpass at ~600 Hz (half the baud rate) to smooth the envelope
    // without smearing the symbol edges.
    mark_env_  = EnvDetect::make(600.0f, static_cast<float>(sample_rate_hz));
    space_env_ = EnvDetect::make(600.0f, static_cast<float>(sample_rate_hz));
}

void AfskDemodulator::reset()
{
    mark_bp_   = {};
    space_bp_  = {};
    mark_env_  = {};
    space_env_ = {};

    // Recalculate filter coefficients after zeroing structs
    static constexpr float kQ = 3.5f;
    mark_bp_  = Biquad::bandpass(1200.0f, kQ, static_cast<float>(sample_rate_));
    space_bp_ = Biquad::bandpass(2200.0f, kQ, static_cast<float>(sample_rate_));
    mark_env_ = EnvDetect::make(600.0f,  static_cast<float>(sample_rate_));
    space_env_= EnvDetect::make(600.0f,  static_cast<float>(sample_rate_));

    symbol_phase_  = 0.0;
    prev_decision_ = false;
    nrzi_prev_     = false;
    sr_            = 0;
    ones_count_    = 0;
    in_frame_      = false;
    bit_buf_       = 0;
    bit_pos_       = 0;
    frame_len_     = 0;
}

// ── Per-sample processing ─────────────────────────────────────────────────────

void AfskDemodulator::process(const int16_t *samples, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        process_sample(static_cast<float>(samples[i]) / 32768.0f);
    }
}

void AfskDemodulator::process_sample(float s)
{
    // Bandpass + envelope
    float mark_out  = mark_env_.process(mark_bp_.process(s));
    float space_out = space_env_.process(space_bp_.process(s));

    bool decision = (mark_out > space_out); // true = mark tone dominant

    // Transition-tracking symbol synchroniser:
    // On any transition in the decision signal, snap the symbol clock to the
    // midpoint of the next expected symbol. This keeps us phase-locked to
    // the incoming bit stream without a full PLL.
    if (decision != prev_decision_) {
        symbol_phase_ = samples_per_symbol_ * 0.5;
    }
    prev_decision_ = decision;

    // Advance symbol clock; fire a symbol decision when threshold is crossed.
    symbol_phase_ += 1.0;
    if (symbol_phase_ >= samples_per_symbol_) {
        symbol_phase_ -= samples_per_symbol_;
        process_symbol(decision);
    }
}

void AfskDemodulator::process_symbol(bool mark_detected)
{
    // NRZI decode: transition → 0, no transition → 1
    bool bit = (mark_detected == nrzi_prev_); // same as previous → 1
    nrzi_prev_ = mark_detected;
    process_bit(bit);
}

// ── HDLC state machine ────────────────────────────────────────────────────────

void AfskDemodulator::process_bit(bool bit)
{
    // Update 8-bit shift register (newest bit at MSB via left-shift;
    // we detect 0x7E after all 8 bits of a flag have been received).
    // Since AX.25 transmits LSB first, the first bit received goes into bit 0:
    sr_ = (sr_ >> 1) | (bit ? 0x80u : 0x00u);

    if (sr_ == 0x7Eu) {
        // Flag detected: start or end a frame
        if (in_frame_ && frame_len_ >= 2) {
            dispatch_frame();
        }
        // Begin collecting a new frame on the next bit
        in_frame_   = true;
        bit_buf_    = 0;
        bit_pos_    = 0;
        frame_len_  = 0;
        ones_count_ = 0;
        return;
    }

    if (!in_frame_) return;

    // Bit stuffing: after 5 consecutive 1s in the data stream, the transmitter
    // inserts a 0. We discard that 0 and reset the ones counter.
    if (bit) {
        if (++ones_count_ >= 6) {
            // Abort: 6+ consecutive 1s is not a valid data sequence
            in_frame_   = false;
            frame_len_  = 0;
            return;
        }
    } else {
        if (ones_count_ == 5) {
            // Stuffed 0: discard it
            ones_count_ = 0;
            return;
        }
        ones_count_ = 0;
    }

    assemble_bit(bit);
}

void AfskDemodulator::assemble_bit(bool bit)
{
    // Accumulate bits LSB-first into bytes
    if (bit) bit_buf_ |= (1u << bit_pos_);
    ++bit_pos_;

    if (bit_pos_ == 8) {
        if (frame_len_ < kMaxFrameBytes) {
            frame_buf_[frame_len_++] = bit_buf_;
        } else {
            // Frame too long: abort
            in_frame_  = false;
            frame_len_ = 0;
        }
        bit_buf_ = 0;
        bit_pos_ = 0;
    }
}

void AfskDemodulator::dispatch_frame()
{
    // Must have at least FCS (2 bytes) plus some address data
    if (frame_len_ < 4) return;

    // Validate FCS before delivering
    size_t payload_len = frame_len_ - 2;
    uint16_t stored = static_cast<uint16_t>(frame_buf_[payload_len])
                    | (static_cast<uint16_t>(frame_buf_[payload_len + 1]) << 8);
    if (ax25::fcs(frame_buf_, payload_len) == stored) {
        frame_cb_(frame_buf_, frame_len_);
    }
    // On FCS mismatch, silently drop the frame (logged at a higher layer)
}

} // namespace pakt
