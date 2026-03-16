#pragma once
// NvsStorage.h – ESP-IDF NVS-backed IStorage implementation
//
// Persists DeviceConfig as a single binary blob under the "pakt_cfg" NVS
// namespace, key "device_config".  Atomic: nvs_commit() is called on every
// save so a power loss mid-write does not corrupt the stored config.
//
// Prerequisites (call before constructing NvsStorage):
//   nvs_flash_init() — returns ESP_OK or is re-tried after erase.
//   nvs_flash_init() is called in app_main() before tasks are created.
//
// ESP-IDF only: do NOT include in the host test build.

#include "pakt/IStorage.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

namespace pakt {

class NvsStorage final : public IStorage
{
public:
    static constexpr const char *kNamespace = "pakt_cfg";
    static constexpr const char *kKey       = "device_config";

    bool load(DeviceConfig &cfg) override
    {
        nvs_handle_t h;
        if (nvs_open(kNamespace, NVS_READONLY, &h) != ESP_OK) return false;

        size_t sz = sizeof(DeviceConfig);
        DeviceConfig tmp{};
        esp_err_t err = nvs_get_blob(h, kKey, &tmp, &sz);
        nvs_close(h);

        if (err != ESP_OK || sz != sizeof(DeviceConfig)) return false;
        if (tmp.schema_version != kConfigSchemaVersion)  return false;

        cfg = tmp;
        return true;
    }

    bool save(const DeviceConfig &cfg) override
    {
        nvs_handle_t h;
        if (nvs_open(kNamespace, NVS_READWRITE, &h) != ESP_OK) return false;

        esp_err_t err = nvs_set_blob(h, kKey, &cfg, sizeof(cfg));
        if (err == ESP_OK) err = nvs_commit(h);
        nvs_close(h);
        return err == ESP_OK;
    }

    bool erase() override
    {
        nvs_handle_t h;
        if (nvs_open(kNamespace, NVS_READWRITE, &h) != ESP_OK) return false;
        esp_err_t err = nvs_erase_all(h);
        if (err == ESP_OK) err = nvs_commit(h);
        nvs_close(h);
        return err == ESP_OK;
    }
};

} // namespace pakt
