#pragma once
// golden_payloads.h – Canonical JSON test fixtures (P1)
//
// Single source-of-truth payload examples that match docs/aprs_mvp_docs/payload_contracts.md.
// Include in any test file that needs representative BLE payload strings.
//
// All string literals are valid UTF-8 and match the schemas in payload_contracts.md.

namespace pakt::golden {

// ── Device Config (write) ─────────────────────────────────────────────────────

// Minimal valid config: callsign only.
inline constexpr const char *kConfigMinimal =
    R"({"callsign":"W1AW"})";

// Config with SSID.
inline constexpr const char *kConfigWithSsid =
    R"({"callsign":"VE3XYZ","ssid":7})";

// Config with max-length callsign and max SSID.
inline constexpr const char *kConfigMaxCallsign =
    R"({"callsign":"VE3XYZ","ssid":15})";

// Invalid: callsign too long.
inline constexpr const char *kConfigCallsignTooLong =
    R"({"callsign":"ABCDEFG"})";

// Invalid: ssid out of range.
inline constexpr const char *kConfigSsidOutOfRange =
    R"({"callsign":"W1AW","ssid":16})";

// ── TX Request (write) ────────────────────────────────────────────────────────

// Minimal valid TX request.
inline constexpr const char *kTxRequestMinimal =
    R"({"dest":"APRS","text":"Hello APRS"})";

// TX request with SSID.
inline constexpr const char *kTxRequestWithSsid =
    R"({"dest":"W1AW","text":"Status update","ssid":9})";

// TX request with maximum-length text (67 chars).
inline constexpr const char *kTxRequestMaxText =
    R"({"dest":"APRS","text":"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"})";

// Invalid: text too long (68 chars).
inline constexpr const char *kTxRequestTextTooLong =
    R"({"dest":"APRS","text":"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"})";

// Invalid: missing text field.
inline constexpr const char *kTxRequestMissingText =
    R"({"dest":"APRS"})";

// Invalid: missing dest field.
inline constexpr const char *kTxRequestMissingDest =
    R"({"text":"Hello APRS"})";

// ── TX Result (notify, firmware → app) ───────────────────────────────────────

// Intermediate TX attempt.
inline constexpr const char *kTxResultTx =
    R"({"msg_id":"1","status":"tx"})";

// Terminal: acked.
inline constexpr const char *kTxResultAcked =
    R"({"msg_id":"1","status":"acked"})";

// Terminal: timeout.
inline constexpr const char *kTxResultTimeout =
    R"({"msg_id":"42","status":"timeout"})";

// Terminal: cancelled.
inline constexpr const char *kTxResultCancelled =
    R"({"msg_id":"7","status":"cancelled"})";

// Terminal: error.
inline constexpr const char *kTxResultError =
    R"({"msg_id":"3","status":"error"})";

// ── GPS Telemetry (notify, firmware → app) ────────────────────────────────────

// Toronto area, 8 sats, GPS fix, 6 knots NE, 75 m MSL, 1994-03-23 12:35:19 UTC.
inline constexpr const char *kGpsTelemetry =
    R"({"lat":43.8130,"lon":-79.3943,"alt_m":75.0,"speed_kmh":11.1,"course":54.7,"sats":8,"fix":1,"ts":764426119})";

// No fix (zeros, ts=0).
inline constexpr const char *kGpsTelemetryNoFix =
    R"({"lat":0.0,"lon":0.0,"alt_m":0.0,"speed_kmh":0.0,"course":0.0,"sats":0,"fix":0,"ts":0})";

} // namespace pakt::golden
