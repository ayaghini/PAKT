// TxScheduler.cpp – Outbound APRS message scheduler (FW-010)

#include "pakt/TxScheduler.h"

#include <cstdio>
#include <cstring>

namespace pakt {

// ── Constructor ───────────────────────────────────────────────────────────────

TxScheduler::TxScheduler(TransmitFn tx_fn, ResultFn result_fn)
    : tx_fn_(std::move(tx_fn))
    , result_fn_(std::move(result_fn))
{
    std::memset(queue_,    0, sizeof(queue_));
    std::memset(occupied_, 0, sizeof(occupied_));
}

// ── Public API ────────────────────────────────────────────────────────────────

EnqueueResult TxScheduler::enqueue(
    uint8_t     client_id,
    const char *dest_callsign,
    uint8_t     dest_ssid,
    const char *text,
    uint32_t    now_ms,
    char       *out_msg_id)
{
    // Validate inputs
    if (!dest_callsign || dest_callsign[0] == '\0') return EnqueueResult::BAD_PARAM;
    if (!text || text[0] == '\0')                    return EnqueueResult::BAD_PARAM;
    if (std::strlen(dest_callsign) > kMaxCallsign)   return EnqueueResult::BAD_PARAM;
    if (std::strlen(text) > kMaxMsgText)             return EnqueueResult::BAD_PARAM;

    int idx = find_free_slot();
    if (idx < 0) return EnqueueResult::QUEUE_FULL;

    // Assign a numeric message ID (1–99999, wrapping)
    msg_id_counter_ = (msg_id_counter_ % 99999) + 1;

    TxMessage &msg      = queue_[idx];
    msg.client_id       = client_id;
    msg.dest_ssid       = dest_ssid;
    msg.state           = TxMsgState::QUEUED;
    msg.retry_count     = 0;
    msg.last_tx_ms      = 0;
    msg.queued_at_ms    = now_ms;

    std::strncpy(msg.dest_callsign, dest_callsign, kMaxCallsign);
    msg.dest_callsign[kMaxCallsign] = '\0';

    std::strncpy(msg.text, text, kMaxMsgText);
    msg.text[kMaxMsgText] = '\0';

    std::snprintf(msg.aprs_msg_id, sizeof(msg.aprs_msg_id), "%u",
                  static_cast<unsigned>(msg_id_counter_));

    occupied_[idx] = true;

    if (out_msg_id) {
        std::strncpy(out_msg_id, msg.aprs_msg_id, kMaxMsgIdStr);
    }

    return EnqueueResult::OK;
}

int TxScheduler::tick(uint32_t now_ms)
{
    int tx_count = 0;

    for (size_t i = 0; i < kMaxQueue; ++i) {
        if (!occupied_[i]) continue;
        TxMessage &msg = queue_[i];
        if (msg.is_terminal()) continue;

        bool ready = false;
        if (msg.state == TxMsgState::QUEUED) {
            ready = true;
        } else if (msg.state == TxMsgState::PENDING) {
            // Retry after kRetryIntervalMs
            ready = (now_ms - msg.last_tx_ms) >= kRetryIntervalMs;
        }

        if (!ready) continue;

        if (transmit_now_(static_cast<int>(i), now_ms)) {
            ++tx_count;
        }
    }

    return tx_count;
}

bool TxScheduler::on_ack_received(const char *ack_msg_id)
{
    if (!ack_msg_id) return false;

    for (size_t i = 0; i < kMaxQueue; ++i) {
        if (!occupied_[i]) continue;
        TxMessage &msg = queue_[i];
        if (msg.is_terminal()) continue;
        if (msg.matches_ack(ack_msg_id)) {
            complete_(static_cast<int>(i), TxMsgState::ACKED);
            return true;
        }
    }
    return false;
}

bool TxScheduler::cancel(uint8_t client_id)
{
    int idx = find_slot_by_client(client_id);
    if (idx < 0) return false;
    if (queue_[idx].is_terminal()) return false;
    complete_(idx, TxMsgState::CANCELLED);
    return true;
}

size_t TxScheduler::active_count() const
{
    size_t n = 0;
    for (size_t i = 0; i < kMaxQueue; ++i) {
        if (occupied_[i] && !queue_[i].is_terminal()) ++n;
    }
    return n;
}

// ── Private ───────────────────────────────────────────────────────────────────

int TxScheduler::find_free_slot() const
{
    // Prefer a genuinely empty slot first.
    for (size_t i = 0; i < kMaxQueue; ++i) {
        if (!occupied_[i]) return static_cast<int>(i);
    }
    // Reclaim a terminal slot.
    for (size_t i = 0; i < kMaxQueue; ++i) {
        if (occupied_[i] && queue_[i].is_terminal()) return static_cast<int>(i);
    }
    return -1;
}

int TxScheduler::find_slot_by_client(uint8_t client_id) const
{
    for (size_t i = 0; i < kMaxQueue; ++i) {
        if (occupied_[i] && queue_[i].client_id == client_id) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void TxScheduler::complete_(int idx, TxMsgState terminal_state)
{
    queue_[idx].state = terminal_state;
    if (result_fn_) {
        result_fn_(queue_[idx]);
    }
    // Keep occupied_ true so callers can still inspect the terminal record.
    // find_free_slot() reclaims terminal slots when space is needed.
}

bool TxScheduler::transmit_now_(int idx, uint32_t now_ms)
{
    TxMessage &msg = queue_[idx];

    if (tx_fn_ && tx_fn_(msg)) {
        msg.retry_count++;
        msg.last_tx_ms = now_ms;
        msg.state      = TxMsgState::PENDING;

        if (msg.retry_count >= kMaxRetries) {
            complete_(idx, TxMsgState::TIMED_OUT);
        }
        return true;
    }
    return false;
}

} // namespace pakt
