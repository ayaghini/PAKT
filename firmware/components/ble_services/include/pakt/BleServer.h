// BleServer – NimBLE GATT server for PAKT APRS TNC (FW-011, FW-012)
//
// Exposes four BLE services:
//   0x180A  Device Information Service (standard)
//   0xA000  APRS Service (config, command, status, RX stream, TX request/result)
//   0xA020  Device Telemetry Service (GPS, power, system, debug stream)
//   0xA050  KISS Service (KISS RX notify, KISS TX write — INT-003)
//
// Write security policy (architecture contract B):
//   Device Config, Device Command, and TX Request require an encrypted + bonded link.
//   Notify characteristics carry no security restriction for reads/subscriptions.
//
// Chunking (INT-002):
//   Writes to config and TX request pass through BleChunker before delivery to the
//   application callback. The caller sees only fully-reassembled payloads.
//
// Rate-limiting (architecture contract B/C):
//   notify_* calls are silently dropped if the same characteristic was notified
//   within the last kNotifyMinIntervalMs milliseconds, protecting modem/audio tasks.
//
// Usage (from ble_task):
//   BleServer::Handlers h;
//   h.on_config_read  = [](uint8_t *buf, size_t max) { return fill_config(buf, max); };
//   h.on_config_write = [](const uint8_t *d, size_t n) { return apply_config(d, n); };
//   h.on_command      = [](const uint8_t *d, size_t n) { return handle_cmd(d, n); };
//   h.on_tx_request   = [](const uint8_t *d, size_t n) { return enqueue_tx(d, n); };
//   BleServer::instance().init(h, "PAKT-TNC");
//   BleServer::instance().start();
//   // BleServer manages its own NimBLE FreeRTOS task; ble_task may then delete itself.

#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>

namespace pakt {

class BleServer {
public:
    // Minimum interval between successive notifications on the same characteristic.
    static constexpr uint32_t kNotifyMinIntervalMs = 100;

    struct Handlers {
        // Called on a Device Config read; fill `buf` with UTF-8 JSON, return byte count.
        std::function<size_t(uint8_t *buf, size_t max_len)> on_config_read;

        // Called after a fully-reassembled Device Config write (encrypted + bonded).
        // Return true to accept, false to respond with an application error.
        std::function<bool(const uint8_t *data, size_t len)> on_config_write;

        // Called after a fully-reassembled Device Command write (encrypted + bonded).
        std::function<bool(const uint8_t *data, size_t len)> on_command;

        // Called after a fully-reassembled TX Request write (encrypted + bonded).
        std::function<bool(const uint8_t *data, size_t len)> on_tx_request;

        // Called on a Device Capabilities read; fill `buf` with UTF-8 JSON, return byte count.
        // Read-only, no security restriction.  If null, returns an empty object.
        std::function<size_t(uint8_t *buf, size_t max_len)> on_caps_read;

        // Called after a fully-reassembled KISS TX write (encrypted + bonded, INT-003).
        // `data` is the raw reassembled KISS frame bytes (may include FEND delimiters).
        // Return true to accept the frame, false to respond with an application error.
        // If null, KISS TX writes are silently dropped (no error returned to client).
        std::function<bool(const uint8_t *data, size_t len)> on_kiss_tx;
    };

    static BleServer &instance();

    // Configure and register GATT services. Must be called before start().
    // `device_name` is the GAP device name broadcast in advertisements (max 29 B).
    bool init(const Handlers &handlers, const char *device_name);

    // Start NimBLE advertising. After this call BleServer manages the NimBLE
    // FreeRTOS task; the calling task may vTaskDelete(nullptr) if desired.
    void start();

    // ── Notify helpers ────────────────────────────────────────────────────────
    // Each returns true if the notification was sent, false if not connected,
    // not subscribed, or rate-limited. Thread-safe (uses NimBLE mbuf API).

    bool notify_status      (const uint8_t *data, size_t len);
    bool notify_rx_packet   (const uint8_t *data, size_t len);
    bool notify_tx_result   (const uint8_t *data, size_t len);
    bool notify_gps         (const uint8_t *data, size_t len);
    bool notify_power       (const uint8_t *data, size_t len);
    bool notify_system      (const uint8_t *data, size_t len);
    bool notify_debug       (const uint8_t *data, size_t len);

    // Send a KISS-framed AX.25 frame to subscribed KISS RX clients (INT-003).
    // `data` must be a complete KISS frame (FEND-delimited, escaped).
    // Returns true if the notification was sent.
    bool notify_kiss_rx     (const uint8_t *data, size_t len);

    bool is_connected() const;
    bool is_bonded()    const;
    bool is_encrypted() const;

    const Handlers &handlers() const;
    const char *device_name() const;
    void set_connected(uint16_t conn_handle, bool bonded);
    void clear_connection();
    void set_bonded(bool bonded);
    // Called from on_ble_sync after ble_gatts_start() has assigned handles.
    void sync_handles();

private:
    BleServer() = default;

    bool     initialized_  = false;
    Handlers handlers_;
    char     device_name_[32]{};

    uint16_t conn_handle_          = 0xFFFF; // BLE_HS_CONN_HANDLE_NONE
    bool     connected_            = false;
    bool     bonded_               = false;

    // Value handles – filled by NimBLE after service registration.
    uint16_t h_dev_config_    = 0;
    uint16_t h_dev_command_   = 0;
    uint16_t h_dev_status_    = 0;
    uint16_t h_dev_caps_      = 0;
    uint16_t h_rx_packet_     = 0;
    uint16_t h_tx_request_    = 0;
    uint16_t h_tx_result_     = 0;
    uint16_t h_gps_telem_     = 0;
    uint16_t h_power_telem_   = 0;
    uint16_t h_system_telem_  = 0;
    uint16_t h_debug_stream_  = 0;
    uint16_t h_kiss_rx_       = 0;   // KISS RX notify handle (INT-003)
    uint16_t h_kiss_tx_       = 0;   // KISS TX write handle (INT-003)

    // Rate-limit: last notify timestamp per characteristic (esp_timer_get_time µs).
    int64_t last_notify_status_   = 0;
    int64_t last_notify_rx_       = 0;
    int64_t last_notify_tx_res_   = 0;
    int64_t last_notify_gps_      = 0;
    int64_t last_notify_power_    = 0;
    int64_t last_notify_system_   = 0;
    int64_t last_notify_debug_    = 0;
    int64_t last_notify_kiss_rx_  = 0;

    // Rolling msg_id counter for INT-002 chunking of KISS RX notifies.
    uint8_t kiss_rx_msg_id_       = 0;

    // Internal helpers (implemented in BleServer.cpp, ESP-IDF only).
    void on_sync_();
    void on_reset_(int reason);
    bool send_notify_(uint16_t val_handle, int64_t &last_us,
                      const uint8_t *data, size_t len);
    bool check_security_(uint16_t conn_handle) const;
    void start_advertising_();
};

} // namespace pakt
