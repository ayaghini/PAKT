// AprsTaskContext.cpp – Bridge between BLE on_tx_request and TxScheduler (P0)

#include "pakt/AprsTaskContext.h"

#include <cstring>

namespace pakt {

AprsTaskContext::AprsTaskContext(RadioTxFn radio_tx, NotifyFn notify)
    : notify_fn_(std::move(notify))
    , scheduler_(
        // TransmitFn wrapper: fire intermediate TX notify, then call the real
        // radio transmit.  Invoked on the aprs_task context.
        [this, tx = std::move(radio_tx)](const TxMessage &msg) -> bool {
            if (notify_fn_) {
                notify_fn_(msg.aprs_msg_id, TxResultEvent::TX);
            }
            return tx(msg);
        },
        // ResultFn: fired by TxScheduler on every terminal state transition.
        [this](const TxMessage &msg) {
            if (notify_fn_) {
                notify_fn_(msg.aprs_msg_id,
                           TxResultEncoder::state_to_event(msg.state));
            }
        }
    )
{}

bool AprsTaskContext::push_kiss_ax25(const uint8_t *ax25, size_t len)
{
    if (!ax25 || len == 0 || len > kKissMaxAx25) return false;

    // SPSC ring: only producer updates kiss_head_.
    uint32_t h = kiss_head_.load(std::memory_order_relaxed);
    uint32_t t = kiss_tail_.load(std::memory_order_acquire);
    if ((h - t) >= static_cast<uint32_t>(kKissRingDepth)) {
        return false;  // ring full
    }
    auto &e = kiss_ring_[h % kKissRingDepth];
    std::memcpy(e.data, ax25, len);
    e.len = len;
    kiss_head_.store(h + 1, std::memory_order_release);
    return true;
}

void AprsTaskContext::set_raw_tx_fn(RawTxFn fn)
{
    raw_tx_fn_ = std::move(fn);
}

bool AprsTaskContext::push_tx_request(const TxRequestFields &req)
{
    // SPSC ring buffer: only producer updates head_.
    uint32_t h = head_.load(std::memory_order_relaxed);
    uint32_t t = tail_.load(std::memory_order_acquire);
    if ((h - t) >= static_cast<uint32_t>(kRingDepth)) {
        return false;  // buffer full
    }
    ring_[h % kRingDepth].req = req;
    head_.store(h + 1, std::memory_order_release);
    return true;
}

void AprsTaskContext::tick(uint32_t now_ms)
{
    // Drain all pending ring-buffer entries into TxScheduler.
    // Only consumer updates tail_.
    uint32_t t = tail_.load(std::memory_order_relaxed);
    const uint32_t h = head_.load(std::memory_order_acquire);

    while (t != h) {
        const TxRequestFields &req = ring_[t % kRingDepth].req;
        // Silently drop if scheduler is full or parameters are bad; the BLE
        // client will see no TX notify and can retry.
        scheduler_.enqueue(
            next_client_id_++,
            req.dest,
            req.ssid,
            req.text,
            now_ms
        );
        tail_.store(++t, std::memory_order_release);
    }

    scheduler_.tick(now_ms);

    // Drain KISS TX raw AX.25 ring.
    // KISS is a raw pipe: no retry, no result notify — just transmit and forget.
    {
        uint32_t t = kiss_tail_.load(std::memory_order_relaxed);
        const uint32_t h = kiss_head_.load(std::memory_order_acquire);
        while (t != h) {
            const KissRawEntry &e = kiss_ring_[t % kKissRingDepth];
            if (raw_tx_fn_) {
                raw_tx_fn_(e.data, e.len);
            }
            kiss_tail_.store(++t, std::memory_order_release);
        }
    }
}

bool AprsTaskContext::notify_ack(const char *ack_msg_id)
{
    return scheduler_.on_ack_received(ack_msg_id);
}

size_t AprsTaskContext::pending_tx_count() const
{
    return scheduler_.active_count();
}

} // namespace pakt
