#pragma once

// APRS packet helpers (FW-009)
//
// Scope: building and parsing APRS information fields; building complete
//        AX.25 frames ready for the modem.
//
// Supported packet types (MVP):
//   '!'  – position without timestamp (no APRS messaging)
//   ':'  – message
//
// Reference: APRS Protocol Reference Version 1.0.1

#include "pakt/Ax25.h"

#include <cstddef>
#include <cstdint>

namespace pakt::aprs {

// ── Packet-type identifiers ────────────────────────────────────────────────────

static constexpr char kTypePosition = '!'; // position without timestamp
static constexpr char kTypeMessage  = ':'; // APRS message

// ── Frame builder ─────────────────────────────────────────────────────────────

// Build a minimal AX.25 UI frame header for an APRS transmission.
// callsign must be null-terminated, uppercase, max 6 chars, no SSID suffix.
// ssid: 0–15.
// The caller fills frame.info and frame.info_len before encoding.
ax25::Frame make_ui_frame(const char *callsign, uint8_t ssid);

// ── Information field encoders ────────────────────────────────────────────────

// Encode an uncompressed APRS position report (type '!') into info_out.
//
// Format: "!DDMM.mmN/DDDMM.mmW<sym_code><comment>"
//
// lat_deg: decimal degrees, positive = North, negative = South (-90 to +90)
// lon_deg: decimal degrees, positive = East,  negative = West (-180 to +180)
// symbol_table: '/' (primary) or '\\' (alternate)
// symbol_code:  APRS symbol code char (e.g. '>' = car, '[' = jogger)
// comment: optional null-terminated comment string, may be nullptr
//
// Returns bytes written into info_out, or 0 on error.
size_t encode_position(float lat_deg, float lon_deg,
                       char symbol_table, char symbol_code,
                       const char *comment,
                       uint8_t *info_out, size_t info_max);

// Encode an APRS message packet (type ':') into info_out.
//
// Format: ":CALLSSID :message text{msg_id}"
//   to_callsign: destination station callsign (max 6 chars, no SSID)
//   to_ssid:     destination SSID (0–15; 0 → not printed)
//   text:        message body (max 67 chars per APRS spec)
//   msg_id:      client-generated numeric ID string (1–5 chars)
//
// Returns bytes written into info_out, or 0 on error.
size_t encode_message(const char *to_callsign, uint8_t to_ssid,
                      const char *text, const char *msg_id,
                      uint8_t *info_out, size_t info_max);

// ── Packet parser ─────────────────────────────────────────────────────────────

// Return the APRS data type identifier from an info field, or '\0' on error.
char packet_type(const uint8_t *info, size_t info_len);

} // namespace pakt::aprs
