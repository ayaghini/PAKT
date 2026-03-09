// Telemetry.cpp – Telemetry JSON serialisers (FW-015)

#include "pakt/Telemetry.h"

#include <cstdio>

namespace pakt {

// ── Helpers ───────────────────────────────────────────────────────────────────

static const char *radio_state_str(RadioState s) {
    switch (s) {
        case RadioState::IDLE:  return "idle";
        case RadioState::RX:    return "rx";
        case RadioState::TX:    return "tx";
        case RadioState::ERROR: return "error";
        default:                return "unknown";
    }
}

// ── DeviceStatus::to_json ────────────────────────────────────────────────────

size_t DeviceStatus::to_json(char *buf, size_t buf_len) const {
    int n = std::snprintf(buf, buf_len,
        "{\"radio\":\"%s\","
        "\"bonded\":%s,"
        "\"gps_fix\":%s,"
        "\"pending_tx\":%u,"
        "\"rx_queue\":%u,"
        "\"uptime_s\":%lu}",
        radio_state_str(radio_state),
        ble_bonded    ? "true" : "false",
        gps_fix       ? "true" : "false",
        static_cast<unsigned>(pending_tx_count),
        static_cast<unsigned>(rx_queue_depth),
        static_cast<unsigned long>(uptime_s)
    );
    return (n > 0 && static_cast<size_t>(n) < buf_len) ? static_cast<size_t>(n) : 0;
}

// ── GpsTelem::to_json ─────────────────────────────────────────────────────────

size_t GpsTelem::to_json(char *buf, size_t buf_len) const {
    int n = std::snprintf(buf, buf_len,
        "{\"lat\":%.6f,"
        "\"lon\":%.6f,"
        "\"alt_m\":%.1f,"
        "\"speed_kmh\":%.1f,"
        "\"course\":%.1f,"
        "\"sats\":%u,"
        "\"fix\":%u,"
        "\"ts\":%lu}",
        lat_deg, lon_deg,
        static_cast<double>(alt_m),
        static_cast<double>(speed_kmh),
        static_cast<double>(course_deg),
        static_cast<unsigned>(sats_used),
        static_cast<unsigned>(fix_quality),
        static_cast<unsigned long>(timestamp_s)
    );
    return (n > 0 && static_cast<size_t>(n) < buf_len) ? static_cast<size_t>(n) : 0;
}

// ── PowerTelem::to_json ───────────────────────────────────────────────────────

size_t PowerTelem::to_json(char *buf, size_t buf_len) const {
    int n = std::snprintf(buf, buf_len,
        "{\"batt_v\":%.3f,"
        "\"batt_pct\":%u,"
        "\"tx_dbm\":%.1f,"
        "\"vswr\":%.2f,"
        "\"temp_c\":%.1f}",
        static_cast<double>(batt_voltage_v),
        static_cast<unsigned>(batt_pct),
        static_cast<double>(tx_power_dbm),
        static_cast<double>(vswr),
        static_cast<double>(temp_c)
    );
    return (n > 0 && static_cast<size_t>(n) < buf_len) ? static_cast<size_t>(n) : 0;
}

// ── SysTelem::to_json ─────────────────────────────────────────────────────────

size_t SysTelem::to_json(char *buf, size_t buf_len) const {
    int n = std::snprintf(buf, buf_len,
        "{\"free_heap\":%lu,"
        "\"min_heap\":%lu,"
        "\"cpu_pct\":%u,"
        "\"tx_pkts\":%lu,"
        "\"rx_pkts\":%lu,"
        "\"tx_errs\":%lu,"
        "\"rx_errs\":%lu,"
        "\"uptime_s\":%lu}",
        static_cast<unsigned long>(free_heap_bytes),
        static_cast<unsigned long>(min_free_heap_bytes),
        static_cast<unsigned>(cpu_load_pct),
        static_cast<unsigned long>(tx_packet_count),
        static_cast<unsigned long>(rx_packet_count),
        static_cast<unsigned long>(tx_error_count),
        static_cast<unsigned long>(rx_error_count),
        static_cast<unsigned long>(uptime_s)
    );
    return (n > 0 && static_cast<size_t>(n) < buf_len) ? static_cast<size_t>(n) : 0;
}

} // namespace pakt
