#pragma once

// IStorage – versioned, atomically-written device configuration storage
//
// Contract (from architecture_contracts.md §A):
//   - Writes are atomic: a partial write must never corrupt the stored config.
//   - load() must migrate older schema versions; never fail on unknown new fields.
//   - Reads degrade gracefully if the backing store is temporarily unavailable.

#include <cstdint>

namespace pakt {

// Increment when DeviceConfig layout changes in a breaking way.
// Non-breaking additions should still bump the version to trigger migration checks.
static constexpr uint16_t kConfigSchemaVersion = 1;

// Persisted device configuration.
// Field sizes are chosen to fit within a single BLE write payload after JSON
// encoding (see ble_gatt_spec.md – Device Config characteristic, 256 B limit).
struct DeviceConfig {
    uint16_t schema_version;          // must equal kConfigSchemaVersion after load

    // Station identity
    char    callsign[10];             // e.g. "N0CALL", null-terminated
    uint8_t ssid;                     // 0–15

    // Radio
    uint32_t aprs_rx_freq_hz;         // e.g. 144390000 (NA) or 144800000 (EU)
    uint32_t aprs_tx_freq_hz;
    uint8_t  squelch;                 // 0–8 per SA818 spec

    // Beaconing
    uint16_t beacon_interval_s;       // 0 = disabled
    char     beacon_comment[64];      // e.g. "Pocket TNC", null-terminated

    // APRS symbol (see APRS symbol table reference)
    char aprs_symbol_table;           // '/' (primary) or '\\' (alternate)
    char aprs_symbol_code;            // e.g. '>' for car, '[' for jogger
};

// Default config values applied when no valid stored config is found.
inline DeviceConfig default_config()
{
    DeviceConfig cfg{};
    cfg.schema_version   = kConfigSchemaVersion;
    cfg.callsign[0]      = '\0';          // must be set by user before operating
    cfg.ssid             = 0;
    cfg.aprs_rx_freq_hz  = 0;             // 0 = not configured; requires explicit region selection
    cfg.aprs_tx_freq_hz  = 0;
    cfg.squelch          = 1;
    cfg.beacon_interval_s = 0;            // beaconing off by default
    cfg.beacon_comment[0] = '\0';
    cfg.aprs_symbol_table = '/';
    cfg.aprs_symbol_code  = '>';
    return cfg;
}

class IStorage
{
public:
    virtual ~IStorage() = default;

    // Load config from persistent store into cfg.
    // Returns false if no valid config exists (caller should write defaults).
    // Older schema versions are migrated in-place before returning.
    virtual bool load(DeviceConfig &cfg) = 0;

    // Atomically write cfg to persistent store.
    // Returns false on write failure; the previous value is preserved.
    virtual bool save(const DeviceConfig &cfg) = 0;

    // Erase all stored config (factory reset).
    // Returns false if the erase could not be completed.
    virtual bool erase() = 0;
};

} // namespace pakt
