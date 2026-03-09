#pragma once

// AX.25 UI-frame codec (data-link layer)
//
// Scope: address encoding/decoding, FCS computation, TNC2 formatting.
// NOT in scope: HDLC flags, bit stuffing, NRZI — those are handled by
//               the AFSK modem (physical layer).
//
// Reference: AX.25 Amateur Packet-Radio Link-Layer Protocol Version 2.2

#include <cstddef>
#include <cstdint>

namespace pakt::ax25 {

// ── Constants ──────────────────────────────────────────────────────────────────

static constexpr size_t  kAddrCallsignMaxLen = 6;   // chars, excluding SSID
static constexpr size_t  kMaxAddresses       = 10;  // dest + src + 8 digipeaters
static constexpr size_t  kMaxInfoLen         = 256; // APRS information field limit
// Worst-case encoded frame: (10×7) addr + 1 ctrl + 1 pid + 256 info + 2 fcs
static constexpr size_t  kMaxEncodedLen      = 330;

static constexpr uint8_t kControlUI          = 0x03;  // Unnumbered Information
static constexpr uint8_t kPidNoLayer3        = 0xF0;  // No Layer 3 protocol (APRS)

// APRS experimental tocall (APZxxx range; "PKT" = PAKT).
// Destination callsign used when building outbound APRS frames.
static constexpr char    kTocall[]           = "APZPKT";

// ── Data structures ───────────────────────────────────────────────────────────

struct Address {
    char    callsign[kAddrCallsignMaxLen + 1]; // null-terminated, uppercase, no SSID
    uint8_t ssid;                              // 0–15
    bool    has_been_repeated;                 // H bit; set by digipeaters that repeated this frame
};

struct Frame {
    Address addr[kMaxAddresses];
    uint8_t addr_count;   // always >= 2 (destination at [0], source at [1])
    uint8_t control;
    uint8_t pid;
    uint8_t info[kMaxInfoLen];
    size_t  info_len;
};

// ── Functions ─────────────────────────────────────────────────────────────────

// Compute AX.25 FCS (CRC-16/CCITT, reflected, poly 0x8408) over a byte buffer.
// Input bytes are processed LSB-first, matching AX.25 transmission order.
uint16_t fcs(const uint8_t *data, size_t len);

// Encode a Frame into a raw byte buffer.
// Output contains: address field + control + PID + info + FCS (2 bytes, LSB first).
// Excludes HDLC flags and bit stuffing — the modem adds those.
// Returns bytes written, or 0 on error (buffer too small, invalid frame).
size_t encode(const Frame &frame, uint8_t *out, size_t out_max);

// Decode a raw byte buffer (flags and bit stuffing already removed) into a Frame.
// Returns true if the FCS is valid and the address field is well-formed.
bool decode(const uint8_t *data, size_t len, Frame &out_frame);

// Format a Frame as a TNC2 monitor string:
//   "SRC[-SSID]>DST[-SSID][,VIA[-SSID][*]]*:INFO"
// Returns characters written (excluding null terminator), or 0 on error.
size_t to_tnc2(const Frame &frame, char *buf, size_t buf_max);

} // namespace pakt::ax25
