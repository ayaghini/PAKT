#pragma once
// PayloadValidator.h – BLE write payload validation (P0: pre-hardware sprint)
//
// Pure C++ – no ESP-IDF or FreeRTOS dependencies; host-testable.
//
// Validates inbound JSON payloads from BLE write characteristics:
//   config     : {"callsign":"W1AW","ssid":0}
//   tx_request : {"dest":"APRS","text":"Hello world","ssid":3}
//
// Field rules:
//   callsign / dest  1-6 chars from [A-Za-z0-9-]; required
//   text             1-67 chars; required in tx_request
//   ssid             integer 0-15; optional (defaults to 0)

#include <cstddef>
#include <cstdint>

namespace pakt {

// Parsed config write payload.
struct ConfigFields {
    char    callsign[7]{};  // NUL-terminated; max 6 chars
    uint8_t ssid{0};
};

// Parsed tx_request write payload.
struct TxRequestFields {
    char    dest[7]{};   // Destination callsign, NUL-terminated; max 6 chars
    uint8_t ssid{0};
    char    text[68]{};  // Message text, NUL-terminated; max 67 chars
};

class PayloadValidator {
public:
    // Maximum accepted JSON payload length (bytes, excluding NUL terminator).
    // Payloads at or above this size are rejected without parsing.
    static constexpr size_t kMaxJsonLen = 512;

    // Validate a config write payload.
    // Returns true if the payload is valid JSON with a legal callsign field.
    // If out is non-null and validation passes, fills *out with parsed fields.
    static bool validate_config_payload(const uint8_t *data, size_t len,
                                        ConfigFields *out = nullptr);

    // Validate a tx_request write payload.
    // Returns true if the payload contains valid dest and text fields.
    // If out is non-null and validation passes, fills *out with parsed fields.
    static bool validate_tx_request_payload(const uint8_t *data, size_t len,
                                            TxRequestFields *out = nullptr);
};

} // namespace pakt
