#pragma once

// AudioIOMock – test double for IAudioIO
//
// Suitable for host unit tests only. Uses std::vector internally (not embedded-safe),
// which is acceptable because mocks are never compiled into production firmware.

#include "pakt/IAudioIO.h"

#include <algorithm>
#include <cstring>
#include <vector>

namespace pakt::mock {

class AudioIOMock final : public IAudioIO
{
public:
    bool init(uint32_t sample_rate_hz) override
    {
        sample_rate_ = sample_rate_hz;
        healthy_ = (sample_rate_hz == 8000u || sample_rate_hz == 16000u);
        return healthy_;
    }

    size_t read_samples(int16_t *buf, size_t max_count) override
    {
        if (!healthy_ || rx_buf_.empty()) return 0;
        size_t n = std::min(max_count, rx_buf_.size());
        std::memcpy(buf, rx_buf_.data(), n * sizeof(int16_t));
        rx_buf_.erase(rx_buf_.begin(), rx_buf_.begin() + static_cast<ptrdiff_t>(n));
        return n;
    }

    size_t write_samples(const int16_t *buf, size_t count) override
    {
        if (!healthy_) return 0;
        tx_buf_.insert(tx_buf_.end(), buf, buf + count);
        return count;
    }

    bool reinit() override
    {
        healthy_ = true;
        return true;
    }

    bool is_healthy() const override { return healthy_; }

    // ── Test helpers ──────────────────────────────────────────────────────────

    // Inject samples into the RX queue so read_samples() will return them.
    void inject_rx(const int16_t *samples, size_t count)
    {
        rx_buf_.insert(rx_buf_.end(), samples, samples + count);
    }

    const std::vector<int16_t> &captured_tx() const { return tx_buf_; }
    void clear_tx() { tx_buf_.clear(); }
    uint32_t sample_rate() const { return sample_rate_; }

private:
    uint32_t sample_rate_{0};
    bool     healthy_{false};
    std::vector<int16_t> rx_buf_;
    std::vector<int16_t> tx_buf_;
};

} // namespace pakt::mock
