#pragma once
// TxMessage.h – Outbound APRS message record (FW-010)
//
// Represents a single outbound APRS message with its lifecycle state.
// All instances live in TxScheduler's static queue (no heap allocation).

#include <cstdint>
#include <cstring>

namespace pakt {

// ── State machine ─────────────────────────────────────────────────────────────

enum class TxMsgState : uint8_t {
    QUEUED,      // Waiting for first transmission slot
    PENDING,     // First TX sent; waiting for ack or retry
    ACKED,       // Remote station sent matching ack — terminal
    TIMED_OUT,   // kMaxRetries transmissions sent with no ack — terminal
    CANCELLED,   // Cancelled by caller — terminal
};

// ── Message record ────────────────────────────────────────────────────────────

static constexpr size_t kMaxCallsign  = 6;    // AX.25 callsign (excl. SSID)
static constexpr size_t kMaxMsgText   = 67;   // APRS message body limit
static constexpr size_t kMaxMsgIdStr  = 6;    // "00001\0"

struct TxMessage {
    // Identity
    uint8_t  client_id;                        // Opaque caller cookie
    char     dest_callsign[kMaxCallsign + 1];  // NUL-terminated
    uint8_t  dest_ssid;
    char     text[kMaxMsgText + 1];            // NUL-terminated
    char     aprs_msg_id[kMaxMsgIdStr];        // Numeric string "1"–"99999"

    // State
    TxMsgState state;
    uint8_t    retry_count;    // Number of transmissions so far
    uint32_t   last_tx_ms;     // Monotonic ms of last transmission (0 = never)
    uint32_t   queued_at_ms;   // Monotonic ms when enqueued

    // ── Helpers ───────────────────────────────────────────────────────────────

    bool is_terminal() const {
        return state == TxMsgState::ACKED     ||
               state == TxMsgState::TIMED_OUT ||
               state == TxMsgState::CANCELLED;
    }

    bool matches_ack(const char *ack_msg_id) const {
        return ack_msg_id != nullptr &&
               std::strncmp(aprs_msg_id, ack_msg_id, kMaxMsgIdStr) == 0;
    }
};

} // namespace pakt
