// KissFramer.cpp – KISS TNC framing encode/decode (FW-018, INT-003)
//
// Pure C++ – no ESP-IDF or FreeRTOS dependencies; host-testable.

#include "pakt/KissFramer.h"

#include <cstring>

namespace pakt {

// ── escape ────────────────────────────────────────────────────────────────────
//
// Escape raw bytes for inclusion in a KISS frame body:
//   0xC0 -> 0xDB 0xDC
//   0xDB -> 0xDB 0xDD
// Returns bytes written to `out`, or 0 on overflow.

size_t KissFramer::escape(const uint8_t *in, size_t in_len,
                           uint8_t *out, size_t out_len)
{
    if (!in || !out) return 0;
    size_t op = 0;
    for (size_t i = 0; i < in_len; ++i) {
        if (in[i] == kKissFend) {
            if (op + 2 > out_len) return 0;
            out[op++] = kKissFesc;
            out[op++] = kKissTfend;
        } else if (in[i] == kKissFesc) {
            if (op + 2 > out_len) return 0;
            out[op++] = kKissFesc;
            out[op++] = kKissTfesc;
        } else {
            if (op + 1 > out_len) return 0;
            out[op++] = in[i];
        }
    }
    return op;
}

// ── unescape ──────────────────────────────────────────────────────────────────
//
// Reverse KISS escaping from a frame body (FEND delimiters already stripped).
// Returns bytes written to `out`, or -1 on malformed input (e.g. trailing FESC
// with no following byte, or unknown escape sequence).

int KissFramer::unescape(const uint8_t *in, size_t in_len,
                          uint8_t *out, size_t out_len)
{
    if (!in || !out) return -1;
    size_t op = 0;
    for (size_t i = 0; i < in_len; ) {
        if (in[i] == kKissFesc) {
            ++i;
            if (i >= in_len) return -1; // trailing FESC — malformed
            if (in[i] == kKissTfend) {
                if (op >= out_len) return -1;
                out[op++] = kKissFend;
            } else if (in[i] == kKissTfesc) {
                if (op >= out_len) return -1;
                out[op++] = kKissFesc;
            } else {
                return -1; // unknown escape sequence — malformed
            }
            ++i;
        } else {
            // FEND inside body (after stripping delimiters) is malformed.
            if (in[i] == kKissFend) return -1;
            if (op >= out_len) return -1;
            out[op++] = in[i++];
        }
    }
    return static_cast<int>(op);
}

// ── encode ────────────────────────────────────────────────────────────────────

size_t KissFramer::encode(const uint8_t *ax25_data, size_t ax25_len,
                           uint8_t *out_buf, size_t out_len)
{
    if (!ax25_data || !out_buf) return 0;
    // Minimum output: FEND + 0x00 + (0 escaped bytes) + FEND = 3 bytes
    if (out_len < 3) return 0;

    size_t op = 0;

    // Leading FEND
    out_buf[op++] = kKissFend;

    // Command byte: port 0, data frame
    out_buf[op++] = kKissCmdData;

    // Escaped AX.25 payload
    size_t escaped = escape(ax25_data, ax25_len, out_buf + op, out_len - op - 1);
    if (escaped == 0 && ax25_len > 0) return 0; // overflow
    op += escaped;

    // Trailing FEND
    if (op >= out_len) return 0;
    out_buf[op++] = kKissFend;

    return op;
}

// ── decode ────────────────────────────────────────────────────────────────────

int KissFramer::decode(const uint8_t *kiss_frame, size_t kiss_len,
                        uint8_t *ax25_buf, size_t ax25_buf_len,
                        uint8_t *cmd_out)
{
    if (!kiss_frame || kiss_len == 0 || !ax25_buf) return -1;

    // Strip leading FEND bytes
    size_t start = 0;
    while (start < kiss_len && kiss_frame[start] == kKissFend) ++start;

    if (start >= kiss_len) return -1; // frame is only FENDs

    // Strip trailing FEND bytes
    size_t end = kiss_len;
    while (end > start && kiss_frame[end - 1] == kKissFend) --end;

    if (end <= start) return -1; // nothing left after stripping

    // First byte is the KISS command byte
    const uint8_t cmd = kiss_frame[start];
    if (cmd_out) *cmd_out = cmd;

    // Return-from-KISS: valid but no data
    if (cmd == kKissCmdReturnFromKiss) return 0;

    // Extended commands 0x01-0x06: valid but ignored (no-op) in MVP
    if (cmd != kKissCmdData) return 0;

    // Port check: upper 4 bits of cmd are port number; only port 0 supported
    // (cmd == kKissCmdData == 0x00 already implies port 0, but be explicit)
    const uint8_t port = (cmd >> 4) & 0x0F;
    if (port != 0) return -1;

    // Body is kiss_frame[start+1 .. end-1]
    const uint8_t *body = kiss_frame + start + 1;
    size_t body_len = end - start - 1;

    // Enforce MVP maximum frame size
    if (body_len > kKissMaxFrame) return -1;

    // Unescape body into ax25_buf
    int ax25_len = unescape(body, body_len, ax25_buf, ax25_buf_len);
    if (ax25_len < 0) return -1;
    if (static_cast<size_t>(ax25_len) > kKissMaxFrame) return -1;

    return ax25_len;
}

} // namespace pakt
