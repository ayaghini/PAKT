#pragma once
// TxScheduler.h – Outbound APRS message scheduler (FW-010)
//
// Manages a static queue of outbound APRS messages.  The caller supplies a
// transmit callback; TxScheduler drives retries and ack matching.
//
// Constraints (APRS spec / protocol contract):
//   - kMaxQueue    = 8   simultaneous outbound messages
//   - kMaxRetries  = 5   transmissions per message (1 initial + 4 retries)
//   - kRetryIntervalMs = 20 000 ms (20 s) between attempts
//
// Thread safety: intended for single-task (APRS task) use only.

#include "TxMessage.h"
#include <cstdint>
#include <functional>

namespace pakt {

// ── Enqueue result ────────────────────────────────────────────────────────────

enum class EnqueueResult : uint8_t {
    OK,          // Message accepted
    QUEUE_FULL,  // No room (all 8 slots occupied with non-terminal messages)
    BAD_PARAM,   // dest or text empty / too long
};

// ── Scheduler ─────────────────────────────────────────────────────────────────

class TxScheduler {
public:
    static constexpr size_t  kMaxQueue         = 8;
    static constexpr uint8_t kMaxRetries       = 5;        // total transmissions
    static constexpr uint32_t kRetryIntervalMs = 20'000;   // ms between retries

    // TransmitFn: caller must transmit the APRS message and return true on
    // success, false if radio is unavailable (scheduler will retry next tick).
    using TransmitFn = std::function<bool(const TxMessage &msg)>;

    // ResultFn: called when a message reaches a terminal state.
    // Invoked inline from enqueue(), tick(), on_ack_received(), or cancel().
    using ResultFn = std::function<void(const TxMessage &msg)>;

    explicit TxScheduler(TransmitFn tx_fn, ResultFn result_fn = nullptr);

    // ── Public API ────────────────────────────────────────────────────────────

    // Enqueue a new outbound message.
    // Fills aprs_msg_id automatically (1-99999 counter).
    // Returns OK and populates *out_msg_id (if non-null) on success.
    EnqueueResult enqueue(
        uint8_t     client_id,
        const char *dest_callsign,
        uint8_t     dest_ssid,
        const char *text,
        uint32_t    now_ms,
        char       *out_msg_id = nullptr   // optional, kMaxMsgIdStr bytes
    );

    // Process all pending/queued messages.  Must be called regularly (e.g. every
    // 1 s from the APRS task).  Returns the number of messages transmitted.
    int tick(uint32_t now_ms);

    // Call when an APRS ack is received.  Matches by numeric message ID.
    // Returns true if a matching PENDING message was found and acked.
    bool on_ack_received(const char *ack_msg_id);

    // Cancel a message by client_id.  Returns true if found and not already
    // terminal.
    bool cancel(uint8_t client_id);

    // ── Inspection ────────────────────────────────────────────────────────────

    // Number of non-terminal messages currently in the queue.
    size_t active_count() const;

    // Read-only view of all slots (includes terminal messages until evicted).
    const TxMessage *slots() const { return queue_; }
    size_t           slot_count() const { return kMaxQueue; }

private:
    TxMessage   queue_[kMaxQueue]{};
    bool        occupied_[kMaxQueue]{};   // true = slot in use (any state)
    TransmitFn  tx_fn_;
    ResultFn    result_fn_;
    uint32_t    msg_id_counter_{0};

    int  find_slot_by_client(uint8_t client_id) const;
    int  find_free_slot() const;
    void complete_(int idx, TxMsgState terminal_state);
    bool transmit_now_(int idx, uint32_t now_ms);
};

} // namespace pakt
