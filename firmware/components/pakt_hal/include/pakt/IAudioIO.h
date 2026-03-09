#pragma once

// IAudioIO – deterministic audio I/O abstraction (SGTL5000 or mock)
//
// Contract (from architecture_contracts.md §C):
//   - No dynamic allocation in the hot path (read_samples / write_samples).
//   - SYS_MCLK must be valid before init() returns true.
//   - Reinit paths must be idempotent.
//   - Audio path has priority over BLE notify bursts (enforced by task priorities).

#include <cstddef>
#include <cstdint>

namespace pakt {

class IAudioIO
{
public:
    virtual ~IAudioIO() = default;

    // Initialise the audio pipeline at the requested sample rate.
    // Validates SYS_MCLK clock-ratio before enabling the pipeline.
    // Supported rates: 8000 Hz, 16000 Hz.
    // Returns false if the sample rate is unsupported or the clock check fails.
    virtual bool init(uint32_t sample_rate_hz) = 0;

    // Read PCM samples from the ADC (codec RX path → modem demodulator input).
    // Non-blocking: returns 0 if no data is available.
    // buf must have capacity for at least max_count int16_t values.
    // No heap allocation; caller owns buf.
    virtual size_t read_samples(int16_t *buf, size_t max_count) = 0;

    // Write PCM samples to the DAC (modem modulator output → codec TX path).
    // Non-blocking: returns the number of samples accepted (may be < count
    // if the TX ring buffer is full).
    // No heap allocation; caller owns buf.
    virtual size_t write_samples(const int16_t *buf, size_t count) = 0;

    // Reset the pipeline without full re-initialisation.
    // Safe to call more than once (idempotent).
    // Returns false only if a hardware fault is detected.
    virtual bool reinit() = 0;

    // Returns true when the pipeline is running without sustained
    // underrun/overrun on either the RX or TX path.
    virtual bool is_healthy() const = 0;
};

} // namespace pakt
