#pragma once
// DeviceConfigStore.h – In-memory device config with optional IStorage backend
//
// Holds the active DeviceConfig (callsign, ssid, radio, beacon fields).
// If an IStorage backend is supplied, validated BLE config writes are also
// persisted atomically.  When no backend is available (hardware not yet wired),
// updates are in-memory only and the caller receives an explicit success result.
//
// Usage:
//   DeviceConfigStore store;            // in-memory only (NVS not wired yet)
//   DeviceConfigStore store(&nvs_impl); // persisted
//
//   // On BLE config write:
//   ConfigFields f; // validated upstream by PayloadValidator
//   bool ok = store.apply(f);           // true = written (and persisted if storage set)
//
//   // Runtime read:
//   const char *cs = store.config().callsign;
//
// Pure C++ – no ESP-IDF dependencies; host-testable.

#include "pakt/IStorage.h"
#include "pakt/PayloadValidator.h"
#include <cstdio>
#include <cstring>

namespace pakt {

class DeviceConfigStore {
public:
    // storage may be nullptr (in-memory only; NVS not yet wired).
    explicit DeviceConfigStore(IStorage *storage = nullptr)
        : storage_(storage), cfg_(default_config())
    {}

    // Attach or replace the storage backend after construction.
    // Call from app_main() once the NVS driver is initialised, before tasks start.
    // Not thread-safe; call only from the main task before any worker task runs.
    void set_storage(IStorage *storage) { storage_ = storage; }

    // Load config from the storage backend on start-up.
    // Returns true if a valid config was loaded; false falls back to defaults.
    // No-op and returns false if no storage backend is set.
    bool load()
    {
        if (!storage_) return false;
        return storage_->load(cfg_);
    }

    // Apply validated BLE ConfigFields to the in-memory config.
    // If a storage backend is set, also persists atomically.
    // Returns true if the update succeeded (and persisted if storage is set).
    // Returns false if persistence was attempted but storage_.save() failed.
    // Note: in-memory state is always updated first; a persist failure does
    // not roll back the in-memory copy (runtime continues with the new value).
    bool apply(const ConfigFields &fields)
    {
        strncpy(cfg_.callsign, fields.callsign, sizeof(cfg_.callsign) - 1);
        cfg_.callsign[sizeof(cfg_.callsign) - 1] = '\0';
        cfg_.ssid = static_cast<uint8_t>(fields.ssid);
        cfg_.schema_version = kConfigSchemaVersion;

        if (storage_) {
            return storage_->save(cfg_);
        }
        return true;   // in-memory only: always succeeds
    }

    // Returns the current in-memory config.
    const DeviceConfig &config() const { return cfg_; }

    // Serialize cfg to JSON: {"callsign":"...","ssid":N}
    // Returns bytes written (excluding null terminator), or 0 on overflow.
    // Used by the BLE on_config_read handler to expose live config to clients.
    static size_t config_to_json(const DeviceConfig &cfg, char *buf, size_t len)
    {
        int n = snprintf(buf, len,
                         "{\"callsign\":\"%s\",\"ssid\":%u}",
                         cfg.callsign,
                         static_cast<unsigned>(cfg.ssid));
        if (n <= 0 || static_cast<size_t>(n) >= len) return 0;
        return static_cast<size_t>(n);
    }

private:
    IStorage    *storage_;
    DeviceConfig cfg_;
};

} // namespace pakt
