#pragma once
// Telemetry.h – Telemetry data structures and JSON serialisers (FW-015)
//
// Defines four telemetry payloads that map to the corresponding BLE notify
// characteristics:
//
//   DeviceStatus  → UUID_DEV_STATUS   (device_status notify)
//   GpsTelem      → UUID_GPS_TELEM    (gps_telem notify)
//   PowerTelem    → UUID_POWER_TELEM  (power_telem notify)
//   SysTelem      → UUID_SYS_TELEM    (system_telem notify)
//
// All to_json() methods produce compact UTF-8 JSON that fits within a single
// BLE notify payload (≤ 244 bytes at MTU 247, before chunking).
// Callers should still chunk through BleServer if the device is operating at
// a low negotiated MTU.

#include <cstddef>
#include <cstdint>

namespace pakt {

// ── Limits ────────────────────────────────────────────────────────────────────

static constexpr size_t kTelemetryJsonMaxLen = 240;   // conservative max

// ── DeviceStatus ─────────────────────────────────────────────────────────────
// Emitted whenever the device state changes (idle, TX, RX, error, …).

enum class RadioState : uint8_t {
    IDLE   = 0,
    RX     = 1,
    TX     = 2,
    ERROR  = 3,
};

struct DeviceStatus {
    RadioState radio_state;
    bool       ble_bonded;         // LE secure connection bonded
    bool       ble_encrypted;      // true when current BLE link is encrypted
    bool       gps_fix;            // true = valid 2D/3D fix
    uint8_t    pending_tx_count;   // messages in TxScheduler queue
    uint8_t    rx_queue_depth;     // decoded frames waiting for host
    uint32_t   rx_freq_hz;         // current radio RX frequency
    uint32_t   tx_freq_hz;         // current radio TX frequency
    uint8_t    squelch;            // SA818 squelch level 0-8
    uint8_t    volume;             // SA818 AF volume 1-8
    bool       wide_band;          // true = 25 kHz, false = 12.5 kHz
    bool       debug_enabled;      // true when BLE debug stream mirroring is enabled
    uint32_t   uptime_s;           // seconds since boot

    // Serialise to JSON.  Returns number of bytes written (excl. NUL), or 0 on overflow.
    size_t to_json(char *buf, size_t buf_len) const;
};

// ── GpsTelem ─────────────────────────────────────────────────────────────────
// Raw NMEA-derived fix data from the onboard GPS module.

struct GpsTelem {
    double   lat_deg;      // +N / −S
    double   lon_deg;      // +E / −W
    float    alt_m;        // metres above MSL
    float    speed_kmh;
    float    course_deg;   // true north, 0–360
    uint8_t  sats_used;
    uint8_t  fix_quality;  // 0=none,1=GPS,2=DGPS
    uint32_t timestamp_s;  // Unix timestamp (seconds since 1970-01-01 UTC), 0 if unknown

    size_t to_json(char *buf, size_t buf_len) const;
};

// ── PowerTelem ────────────────────────────────────────────────────────────────
// Battery and RF power measurements.

struct PowerTelem {
    float   batt_voltage_v;   // cell voltage
    uint8_t batt_pct;         // 0–100 %
    float   tx_power_dbm;     // configured TX power
    float   vswr;             // 1.0 = perfect (0.0 = not measured)
    float   temp_c;           // board temperature (°C)

    size_t to_json(char *buf, size_t buf_len) const;
};

// ── SysTelem ─────────────────────────────────────────────────────────────────
// ESP32-S3 runtime diagnostics.

struct SysTelem {
    uint32_t free_heap_bytes;
    uint32_t min_free_heap_bytes;   // low-water mark since boot
    uint8_t  cpu_load_pct;          // 0–100 %
    uint32_t tx_packet_count;       // total frames transmitted
    uint32_t rx_packet_count;       // total frames decoded
    uint32_t tx_error_count;
    uint32_t rx_error_count;
    uint32_t uptime_s;

    size_t to_json(char *buf, size_t buf_len) const;
};

} // namespace pakt
