#pragma once
// AprsTaskContext.h – Bridge between BLE on_tx_request and TxScheduler (P0)
//
// Owns a TxScheduler and provides a thread-safe SPSC ring buffer so the BLE
// handler (any RTOS context) can push TX requests that are consumed by
// aprs_task in its main loop.
//
// BLE handler context calls:
//   push_tx_request()  — enqueue a validated TxRequestFields; returns false if full
//
// aprs_task loop calls:
//   tick(now_ms)       — dequeue requests, drive TxScheduler retries
//   notify_ack(id)     — forward an APRS ack from the radio to TxScheduler
//
// TX result notifications (intermediate TX + terminal states) are delivered via
// the NotifyFn callback supplied at construction.
//
// Thread safety: single producer (push_tx_request), single consumer (tick /
// notify_ack).  Uses std::atomic for the ring-buffer head/tail; no mutex needed.
//
// Pure C++ – no ESP-IDF or FreeRTOS dependencies; host-testable.

#include "pakt/TxScheduler.h"
#include "pakt/TxResultEncoder.h"
#include "pakt/PayloadValidator.h"

#include <atomic>
#include <cstdint>
#include <functional>

namespace pakt {

class AprsTaskContext {
public:
    static constexpr size_t kRingDepth = 8;

    // RadioTxFn: called by TxScheduler for each transmission attempt.
    // Must return true on success, false if radio is unavailable.
    using RadioTxFn = std::function<bool(const TxMessage &)>;

    // NotifyFn: called on every TX attempt (TX event) and on every terminal
    // state transition (ACKED / TIMEOUT / CANCELLED / ERROR).
    using NotifyFn = std::function<void(const char *msg_id, TxResultEvent event)>;

    // radio_tx  – actual transmit function (stub in pre-hardware sprint)
    // notify    – BLE result notify callback; may be null
    explicit AprsTaskContext(RadioTxFn radio_tx, NotifyFn notify = nullptr);

    // Thread-safe (producer side).
    // Returns true if the request was accepted into the ring buffer.
    // Returns false if the buffer is full; the BLE handler should reject the write.
    bool push_tx_request(const TxRequestFields &req);

    // Thread-safe (producer side): push a raw AX.25 frame from the KISS TX path.
    // KISS TX bypasses TxScheduler — KISS is a raw pipe with no APRS retry.
    // Returns false if the ring buffer is full or the frame is invalid (null/0/oversize).
    bool push_kiss_ax25(const uint8_t *ax25, size_t len);

    // Set the raw transmit function called for each KISS TX frame in tick().
    // Not thread-safe; call before any push_kiss_ax25 calls.
    // If not set, frames are dequeued and silently discarded.
    using RawTxFn = std::function<bool(const uint8_t *ax25, size_t len)>;
    void set_raw_tx_fn(RawTxFn fn);

    // Consumer side — call from aprs_task loop at regular intervals.
    // Drains all pending ring-buffer entries into TxScheduler, then ticks.
    // Also drains the KISS TX raw ring and calls raw_tx_fn_ for each frame.
    void tick(uint32_t now_ms);

    // Consumer side — call from aprs_task when the APRS layer receives an ack.
    // Returns true if a matching PENDING message was found.
    bool notify_ack(const char *ack_msg_id);

    // Inspection helper for status/telemetry publishing.
    size_t pending_tx_count() const;

private:
    // notify_fn_ MUST be declared before scheduler_ so it is initialized first;
    // the scheduler's callbacks capture 'this' and call notify_fn_ at runtime.
    NotifyFn    notify_fn_;
    TxScheduler scheduler_;

    struct RingEntry {
        TxRequestFields req{};
    };
    RingEntry ring_[kRingDepth]{};

    // head_ written only by producer; tail_ written only by consumer.
    std::atomic<uint32_t> head_{0};
    std::atomic<uint32_t> tail_{0};

    uint8_t next_client_id_{0};

    // ── KISS raw TX ring (SPSC, KISS TX path) ─────────────────────────────────
    // Separate from the APRS message ring: KISS is a raw AX.25 pipe, no retry.

    static constexpr size_t kKissRingDepth = 4;
    static constexpr size_t kKissMaxAx25   = 330;  // = kKissMaxFrame in KissFramer.h

    struct KissRawEntry {
        uint8_t data[kKissMaxAx25]{};
        size_t  len{0};
    };
    KissRawEntry           kiss_ring_[kKissRingDepth]{};
    std::atomic<uint32_t>  kiss_head_{0};
    std::atomic<uint32_t>  kiss_tail_{0};
    RawTxFn                raw_tx_fn_;
};

} // namespace pakt
