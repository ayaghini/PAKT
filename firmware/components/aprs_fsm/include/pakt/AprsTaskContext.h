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

    // Consumer side — call from aprs_task loop at regular intervals.
    // Drains all pending ring-buffer entries into TxScheduler, then ticks.
    void tick(uint32_t now_ms);

    // Consumer side — call from aprs_task when the APRS layer receives an ack.
    // Returns true if a matching PENDING message was found.
    bool notify_ack(const char *ack_msg_id);

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
};

} // namespace pakt
