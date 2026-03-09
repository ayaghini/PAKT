#pragma once

// PacketLinkMock – test double for IPacketLink
//
// Fixed-depth queues with no heap allocation per frame (vectors are used
// for the queue storage, which is fine for test-only code).

#include "pakt/IPacketLink.h"

#include <cstring>
#include <queue>
#include <vector>

namespace pakt::mock {

class PacketLinkMock final : public IPacketLink
{
public:
    static constexpr size_t kMaxQueueDepth = 16;

    bool send(const Ax25Frame &frame) override
    {
        if (tx_queue_.size() >= kMaxQueueDepth) return false;
        tx_queue_.emplace(frame.data, frame.data + frame.length);
        return true;
    }

    bool recv(Ax25Frame &frame, size_t max_len) override
    {
        if (rx_queue_.empty()) return false;
        const auto &pkt = rx_queue_.front();
        size_t n = (pkt.size() < max_len) ? pkt.size() : max_len;
        std::memcpy(frame.data, pkt.data(), n);
        frame.length = n;
        rx_queue_.pop();
        return true;
    }

    size_t rx_available() const override { return rx_queue_.size(); }
    size_t tx_free()      const override { return kMaxQueueDepth - tx_queue_.size(); }

    // ── Test helpers ──────────────────────────────────────────────────────────

    // Push a frame into the RX queue as if it arrived from the modem.
    void inject_rx(const uint8_t *data, size_t length)
    {
        rx_queue_.emplace(data, data + length);
    }

    bool has_tx() const { return !tx_queue_.empty(); }

    // Pop and return the next queued TX frame for inspection.
    std::vector<uint8_t> pop_tx()
    {
        auto frame = tx_queue_.front();
        tx_queue_.pop();
        return frame;
    }

private:
    std::queue<std::vector<uint8_t>> rx_queue_;
    std::queue<std::vector<uint8_t>> tx_queue_;
};

} // namespace pakt::mock
