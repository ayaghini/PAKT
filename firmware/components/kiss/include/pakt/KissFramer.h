#pragma once
// KissFramer.h – KISS TNC framing encode/decode (FW-018, INT-003)
//
// Implements the binary framing layer for KISS-over-BLE:
//
//   encode()  Wraps a raw AX.25 frame into a KISS data frame ready to send via
//             the KISS RX notify characteristic.
//             Output format: FEND 0x00 <escaped-AX.25> FEND
//
//   decode()  Strips FEND delimiters and un-escapes a KISS frame received on
//             the KISS TX characteristic, then returns the raw AX.25 bytes.
//
// KISS escaping (TAPR KISS spec):
//   0xC0 in data -> 0xDB 0xDC
//   0xDB in data -> 0xDB 0xDD
//
// MVP constraints (frozen in docs/16_kiss_over_ble_spec.md §7):
//   - Only port 0 (data frame type 0x00) is forwarded.
//   - Extended commands 0x01-0x06 are no-op (decode returns 0).
//   - Return-from-KISS 0x0F is returned as cmd=0x0F, ax25_len=0.
//   - Maximum logical KISS frame after reassembly: kKissMaxFrame bytes.
//
// Pure C++ – no ESP-IDF or FreeRTOS dependencies; host-testable.

#include <cstddef>
#include <cstdint>

namespace pakt {

// Maximum logical KISS frame size after reassembly (MVP contract).
static constexpr size_t kKissMaxFrame = 330;

// KISS framing constants
static constexpr uint8_t kKissFend = 0xC0;  // Frame end / delimiter
static constexpr uint8_t kKissFesc = 0xDB;  // Frame escape
static constexpr uint8_t kKissTfend = 0xDC; // Transposed FEND
static constexpr uint8_t kKissTfesc = 0xDD; // Transposed FESC

// KISS command bytes
static constexpr uint8_t kKissCmdData           = 0x00; // Data frame, port 0
static constexpr uint8_t kKissCmdReturnFromKiss  = 0x0F; // Return-from-KISS

class KissFramer {
public:
    // ── encode ────────────────────────────────────────────────────────────────
    //
    // Wrap `ax25_data[0..ax25_len)` as a KISS port-0 data frame.
    // Output format: FEND 0x00 <escaped AX.25 bytes> FEND
    //
    // Returns the number of bytes written to `out_buf`, or 0 on overflow.
    // Caller must ensure `out_buf` is large enough: worst case is
    //   2 (header) + 2 * ax25_len (all bytes escaped) + 1 (tail FEND).
    static size_t encode(const uint8_t *ax25_data, size_t ax25_len,
                         uint8_t *out_buf, size_t out_len);

    // ── decode ────────────────────────────────────────────────────────────────
    //
    // Decode a reassembled KISS frame from the KISS TX characteristic.
    //
    // The input may include leading/trailing FEND (0xC0) bytes or omit them —
    // both are handled. The first non-FEND byte is the KISS command byte.
    //
    // Returns:
    //   > 0  Number of AX.25 bytes written to `ax25_buf`. `cmd_out` is set to
    //        the command byte (0x00 for a port-0 data frame).
    //   0    Valid KISS frame but not a data frame (command ignored in MVP).
    //        `cmd_out` is set to the actual command byte.
    //  -1    Malformed input (bad escaping, oversize, or empty after stripping).
    //
    // Non-port-0 data frames (e.g. port 1-7 in cmd byte bits 7-4) return -1
    // because only port 0 is supported in MVP.
    static int decode(const uint8_t *kiss_frame, size_t kiss_len,
                      uint8_t *ax25_buf, size_t ax25_buf_len,
                      uint8_t *cmd_out = nullptr);

    // ── escape helpers (exposed for testing) ─────────────────────────────────
    static size_t escape(const uint8_t *in, size_t in_len,
                         uint8_t *out, size_t out_len);
    static int    unescape(const uint8_t *in, size_t in_len,
                           uint8_t *out, size_t out_len);
};

} // namespace pakt
