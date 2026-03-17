#pragma once
// DeviceCapabilities.h – Device capability advertisement (INT-001)
//
// Defines the capability record that the firmware exposes on the
// Device Capabilities BLE characteristic (UUID_DEV_CAPS, 0xA0040000).
//
// Wire format: compact UTF-8 JSON, read-only, no chunking required.
//
// Example payload:
//   {"fw_ver":"0.1.0","hw_rev":"EVT-A","protocol":1,
//    "features":["aprs_2m","ble_chunking","telemetry","msg_ack","config_rw"]}
//
// Protocol versioning:
//   protocol == 1   this GATT layout (all Steps 0-10)
//   A future breaking change to the GATT service structure increments protocol.
//   Clients SHOULD check protocol before using advanced features.

#include <cstddef>
#include <cstdint>

namespace pakt {

// ── Feature flag bits ─────────────────────────────────────────────────────────
// Used internally; serialised as a JSON array of string names.

enum class Feature : uint32_t {
    APRS_2M      = 1u << 0,   // 144 MHz APRS (SA818 path)
    BLE_CHUNKING = 1u << 1,   // INT-002 msg_id/chunk_idx/chunk_total framing
    TELEMETRY    = 1u << 2,   // GPS, power, system, status notify characteristics
    MSG_ACK      = 1u << 3,   // APRS message ACK/retry (TxScheduler)
    CONFIG_RW    = 1u << 4,   // Device config read/write characteristic
    GPS_ONBOARD  = 1u << 5,   // Onboard GPS module present and active
    HF_AUDIO     = 1u << 6,   // HF audio bridge (discovery track, not MVP)
    KISS_BLE     = 1u << 7,   // KISS TNC over BLE (INT-003, MVP-required)
};

static constexpr uint32_t kMvpFeatures =
    static_cast<uint32_t>(Feature::APRS_2M)      |
    static_cast<uint32_t>(Feature::BLE_CHUNKING)  |
    static_cast<uint32_t>(Feature::TELEMETRY)     |
    static_cast<uint32_t>(Feature::MSG_ACK)       |
    static_cast<uint32_t>(Feature::CONFIG_RW)     |
    static_cast<uint32_t>(Feature::GPS_ONBOARD)   |
    static_cast<uint32_t>(Feature::KISS_BLE);

// ── Capability record ─────────────────────────────────────────────────────────

static constexpr size_t kCapJsonMaxLen = 240;

struct DeviceCapabilities {
    const char *fw_ver;     // Semantic version string e.g. "0.1.0"
    const char *hw_rev;     // Hardware revision e.g. "EVT-A"
    uint8_t     protocol;   // GATT protocol version (currently 1)
    uint32_t    features;   // Bitmask of Feature flags

    // Serialise to compact UTF-8 JSON.
    // Returns number of bytes written (excl. NUL), or 0 on overflow.
    size_t to_json(char *buf, size_t buf_len) const;

    // Test whether a specific feature is advertised.
    bool has(Feature f) const {
        return (features & static_cast<uint32_t>(f)) != 0;
    }

    // Default MVP capabilities (used by BleServer stub until hardware wired).
    static DeviceCapabilities mvp_defaults();
};

} // namespace pakt
