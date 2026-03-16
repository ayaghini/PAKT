#pragma once
// TxResultEncoder.h – TX result JSON encoder for BLE notify (P0: pre-hardware sprint)
//
// Encodes a TX result event as the wire-format JSON:
//   {"msg_id":"<id>","status":"tx|acked|timeout|cancelled|error"}
//
// Events:
//   TX        – transmission attempt fired (intermediate, not terminal)
//   ACKED     – remote station sent matching ack (terminal)
//   TIMEOUT   – max retries exhausted with no ack (terminal)
//   CANCELLED – cancelled by caller (terminal)
//   ERROR     – transmit function returned false (terminal, radio unavailable)
//
// Pure C++ – no ESP-IDF or FreeRTOS dependencies; host-testable.

#include "TxMessage.h"
#include <cstddef>

namespace pakt {

enum class TxResultEvent : uint8_t {
    TX,
    ACKED,
    TIMEOUT,
    CANCELLED,
    ERROR,
};

class TxResultEncoder {
public:
    // Encode into buf. Returns the number of bytes written (excluding NUL),
    // or 0 on error (null buf / zero buf_len / null msg_id).
    // Output is always NUL-terminated if buf_len >= 1.
    static size_t encode(const char *msg_id, TxResultEvent event,
                         char *buf, size_t buf_len);

    // Map a terminal TxMsgState to the corresponding TxResultEvent.
    // Non-terminal states (QUEUED, PENDING) map to ERROR.
    static TxResultEvent state_to_event(TxMsgState state);

    // Return the wire-format status string for an event.
    static const char *event_to_str(TxResultEvent event);
};

} // namespace pakt
