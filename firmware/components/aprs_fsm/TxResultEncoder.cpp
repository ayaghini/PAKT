// TxResultEncoder.cpp – TX result JSON encoder for BLE notify (P0)

#include "pakt/TxResultEncoder.h"

#include <cstdio>

namespace pakt {

size_t TxResultEncoder::encode(const char *msg_id, TxResultEvent event,
                                char *buf, size_t buf_len)
{
    if (!buf || buf_len == 0 || !msg_id) return 0;
    int n = snprintf(buf, buf_len,
                     "{\"msg_id\":\"%s\",\"status\":\"%s\"}",
                     msg_id, event_to_str(event));
    if (n < 0) return 0;
    return (static_cast<size_t>(n) < buf_len) ? static_cast<size_t>(n) : buf_len - 1;
}

TxResultEvent TxResultEncoder::state_to_event(TxMsgState state)
{
    switch (state) {
        case TxMsgState::ACKED:     return TxResultEvent::ACKED;
        case TxMsgState::TIMED_OUT: return TxResultEvent::TIMEOUT;
        case TxMsgState::CANCELLED: return TxResultEvent::CANCELLED;
        default:                    return TxResultEvent::ERROR;
    }
}

const char *TxResultEncoder::event_to_str(TxResultEvent event)
{
    switch (event) {
        case TxResultEvent::TX:        return "tx";
        case TxResultEvent::ACKED:     return "acked";
        case TxResultEvent::TIMEOUT:   return "timeout";
        case TxResultEvent::CANCELLED: return "cancelled";
        case TxResultEvent::ERROR:     return "error";
    }
    return "error";
}

} // namespace pakt
