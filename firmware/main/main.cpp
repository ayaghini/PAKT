// PAKT Firmware – application entry point
//
// Each subsystem runs as an independent FreeRTOS task.  Tasks are stubs here
// (Step 0); full drivers are added in Steps 1-6.
//
// Priority policy (higher number = higher priority in FreeRTOS):
//   power  2  – background, non-time-critical
//   ble    4  – rate-limited by contract; must not starve modem
//   gps    5  – UART stream, moderate latency tolerance
//   aprs   5  – packet state machine
//   radio  7  – UART to SA818, timing-sensitive
//   audio  8  – I2S read/write, real-time critical

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"

#include "pakt/BleServer.h"
#include <cstring>

static const char *TAG = "pakt";

#define PAKT_FIRMWARE_VERSION "0.1.0"

// ── Forward declarations ──────────────────────────────────────────────────────

static void radio_task(void *arg);
static void audio_task(void *arg);
static void aprs_task(void *arg);
static void gps_task(void *arg);
static void ble_task(void *arg);
static void power_task(void *arg);

// ── app_main ─────────────────────────────────────────────────────────────────

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "PAKT firmware v%s starting (IDF %s)", PAKT_FIRMWARE_VERSION, esp_get_idf_version());

    // PTT must be safe-off before any task starts.
    // The radio driver (Step 1) will assert this at init; document the intent here.
    ESP_LOGI(TAG, "PTT default state: OFF");

    xTaskCreate(audio_task, "audio", 8192, nullptr, 8, nullptr);
    xTaskCreate(radio_task, "radio", 4096, nullptr, 7, nullptr);
    xTaskCreate(gps_task,   "gps",   4096, nullptr, 5, nullptr);
    xTaskCreate(aprs_task,  "aprs",  4096, nullptr, 5, nullptr);
    xTaskCreate(ble_task,   "ble",   8192, nullptr, 4, nullptr);
    xTaskCreate(power_task, "power", 2048, nullptr, 2, nullptr);

    ESP_LOGI(TAG, "All tasks created");
}

// ── Task stubs ────────────────────────────────────────────────────────────────
// Each stub will be replaced in subsequent implementation steps.
// The tag prefix matches the task name so logs are easy to filter.

static void radio_task(void *arg)
{
    // Step 1: SA818 UART driver + PTT control
    ESP_LOGI("radio", "task started (stub)");
    for (;;) { vTaskDelay(pdMS_TO_TICKS(1000)); }
}

static void audio_task(void *arg)
{
    // Step 2: SGTL5000 I2S driver + AFSK modem pipeline
    ESP_LOGI("audio", "task started (stub)");
    for (;;) { vTaskDelay(pdMS_TO_TICKS(10)); }
}

static void aprs_task(void *arg)
{
    // Step 3: AX.25 framing + APRS encode/decode + TX retry FSM
    ESP_LOGI("aprs", "task started (stub)");
    for (;;) { vTaskDelay(pdMS_TO_TICKS(100)); }
}

static void gps_task(void *arg)
{
    // Step 5: NMEA parser + stale-fix management
    ESP_LOGI("gps", "task started (stub)");
    for (;;) { vTaskDelay(pdMS_TO_TICKS(100)); }
}

static void ble_task(void *arg)
{
    // Step 4: BLE GATT services + encrypted/bonded write policy.
    // Stub handlers – replaced when APRS logic (Step 6/7) is wired in.
    pakt::BleServer::Handlers handlers;
    handlers.on_config_read = [](uint8_t *buf, size_t max) -> size_t {
        const char *placeholder = "{\"callsign\":\"\"}";
        size_t n = strlen(placeholder);
        if (n > max) return 0;
        memcpy(buf, placeholder, n);
        return n;
    };
    handlers.on_config_write = [](const uint8_t *, size_t) { return true; };
    handlers.on_command      = [](const uint8_t *, size_t) { return true; };
    handlers.on_tx_request   = [](const uint8_t *, size_t) { return true; };

    if (!pakt::BleServer::instance().init(handlers, "PAKT-TNC")) {
        ESP_LOGE("ble", "BleServer init failed");
        vTaskDelete(nullptr);
        return;
    }
    pakt::BleServer::instance().start();
    // NimBLE now runs in its own internal task; this task is no longer needed.
    vTaskDelete(nullptr);
}

static void power_task(void *arg)
{
    // Step 7: MAX17048 fuel gauge + charging telemetry
    ESP_LOGI("power", "task started (stub)");
    for (;;) { vTaskDelay(pdMS_TO_TICKS(5000)); }
}
