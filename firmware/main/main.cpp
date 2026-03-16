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
#include "esp_timer.h"
#include "nvs_flash.h"

#include "pakt/BleServer.h"
#include "pakt/DeviceCapabilities.h"
#include "pakt/NmeaParser.h"
#include "pakt/AprsTaskContext.h"
#include "pakt/PayloadValidator.h"
#include "pakt/DeviceConfigStore.h"
#include "pakt/PttWatchdog.h"
#include "pakt/PttController.h"
#include "pakt/Sa818Radio.h"
#include "NvsStorage.h"
#include "Sa818UartTransport.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include <cinttypes>
#include <cstring>

static const char *TAG = "pakt";

// Shared between aprs_task (owner) and ble_task (producer).
// Set by aprs_task before it enters its run loop; ble_task checks for null.
static pakt::AprsTaskContext *g_aprs_ctx = nullptr;

// PTT watchdog (FW-016). Pointer set by aprs_task before its run loop;
// watchdog_task calls tick() every 500 ms. Null until aprs_task is ready.
static pakt::PttWatchdog *g_ptt_watchdog = nullptr;

// Device config (callsign, ssid, radio settings).
// NVS backend attached in app_main() after nvs_flash_init().
// Until then (and if NVS init fails) updates are in-memory only.
static pakt::DeviceConfigStore g_device_config;
static pakt::NvsStorage        g_nvs_storage;

#define PAKT_FIRMWARE_VERSION "0.1.0"

// ── Forward declarations ──────────────────────────────────────────────────────

static void radio_task(void *arg);
static void audio_task(void *arg);
static void aprs_task(void *arg);
static void watchdog_task(void *arg);
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

    // ── NVS init + config load ────────────────────────────────────────────────
    // Must complete before tasks start so callsign/ssid are available immediately.
    {
        esp_err_t ret = nvs_flash_init();
        if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            ESP_LOGW(TAG, "NVS partition truncated; erasing and reinitialising");
            ESP_ERROR_CHECK(nvs_flash_erase());
            ret = nvs_flash_init();
        }
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "NVS init failed (%s) – config not persisted this session",
                     esp_err_to_name(ret));
            // g_device_config keeps in-memory-only mode (no set_storage call).
        } else {
            g_device_config.set_storage(&g_nvs_storage);
            if (g_device_config.load()) {
                ESP_LOGI(TAG, "Config loaded from NVS: callsign=%s ssid=%u",
                         g_device_config.config().callsign,
                         static_cast<unsigned>(g_device_config.config().ssid));
            } else {
                ESP_LOGI(TAG, "No prior config in NVS – using defaults");
            }
        }
    }

    xTaskCreate(audio_task,    "audio",    8192, nullptr, 8, nullptr);
    xTaskCreate(radio_task,    "radio",    4096, nullptr, 7, nullptr);
    xTaskCreate(watchdog_task, "watchdog", 2048, nullptr, 6, nullptr);
    xTaskCreate(gps_task,      "gps",      4096, nullptr, 5, nullptr);
    xTaskCreate(aprs_task,     "aprs",     4096, nullptr, 5, nullptr);
    xTaskCreate(ble_task,      "ble",      8192, nullptr, 4, nullptr);
    xTaskCreate(power_task,    "power",    2048, nullptr, 2, nullptr);

    ESP_LOGI(TAG, "All tasks created");
}

// ── Task stubs ────────────────────────────────────────────────────────────────
// Each stub will be replaced in subsequent implementation steps.
// The tag prefix matches the task name so logs are easy to filter.

static void radio_task(void *arg)
{
    // FW-003: SA818-V radio driver (UART1, PTT GPIO11 active-low)
    //
    // GPIO11 – PTT, active low: LOW = TX asserted, HIGH = TX off.
    // Register a direct GPIO lambda as the watchdog safe-off callback BEFORE
    // init() so the watchdog can de-assert PTT even if init() fails or hangs.
    // This direct path bypasses Sa818Radio state and avoids any race between
    // watchdog_task and radio_task.

    // static constexpr gives these variables static storage duration, so they are
    // accessible from the non-capturing lambdas below without explicit capture
    // (C++17 §6.7.1 / §7.5.5.2).  GCC 12 (Xtensa ESP-IDF toolchain) handles
    // this correctly; no -Wcapture-this-in-constexpr-context warning expected.
    static constexpr gpio_num_t kPttGpio    = static_cast<gpio_num_t>(11);
    static constexpr uart_port_t kUartPort  = UART_NUM_1;
    static constexpr int         kUartTxPin = 15;   // SA818_RX_CTRL
    static constexpr int         kUartRxPin = 16;   // SA818_TX_STAT
    static constexpr int         kBaud      = 9600;

    // Configure PTT GPIO: output, default HIGH (PTT off).
    gpio_config_t ptt_cfg = {};
    ptt_cfg.pin_bit_mask = (1ULL << kPttGpio);
    ptt_cfg.mode         = GPIO_MODE_OUTPUT;
    ptt_cfg.pull_up_en   = GPIO_PULLUP_DISABLE;
    ptt_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    ptt_cfg.intr_type    = GPIO_INTR_DISABLE;
    gpio_config(&ptt_cfg);
    gpio_set_level(kPttGpio, 1);   // PTT off

    // Register watchdog safe-off callback: direct GPIO, bypasses driver state.
    pakt::ptt_register_safe_off([](){ gpio_set_level(kPttGpio, 1); });
    ESP_LOGI("radio", "PTT GPIO%d configured (HIGH=off); watchdog safe-off registered",
             static_cast<int>(kPttGpio));

    // Configure UART1 at 9600 8N1 for SA818 AT commands.
    const uart_config_t uart_cfg = {
        .baud_rate  = kBaud,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_driver_install(kUartPort, 256, 0, 0, nullptr, 0);
    uart_param_config(kUartPort, &uart_cfg);
    uart_set_pin(kUartPort, kUartTxPin, kUartRxPin,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    // Construct concrete transport and radio driver.
    static pakt::Sa818UartTransport transport(kUartPort);
    static pakt::Sa818Radio radio(
        transport,
        [](bool on){ gpio_set_level(kPttGpio, on ? 0 : 1); }
    );

    if (!radio.init()) {
        ESP_LOGE("radio", "SA818 init failed – radio unavailable; PTT remains off");
        // Watchdog safe-off callback stays as direct GPIO lambda (safe).
        for (;;) { vTaskDelay(pdMS_TO_TICKS(5000)); }
    }

    ESP_LOGI("radio", "SA818 init OK");

    // Set APRS frequency (144.390 MHz simplex, 25 kHz wide, squelch 1).
    radio.set_freq(pakt::Sa818Radio::kAprsFreqHz, pakt::Sa818Radio::kAprsFreqHz);

    // Update watchdog safe-off to go through driver (cleaner state tracking).
    // The direct GPIO lambda remains registered until this point to guard init().
    pakt::ptt_register_safe_off([](){ radio.ptt(false); });

    // radio_task's run loop is currently a placeholder.
    // The transmit path will be wired here in the audio/modem integration step.
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
    //
    // AprsTaskContext owns TxScheduler and the BLE→scheduler ring buffer.
    // TODO(hardware): replace the stub RadioTxFn with the real AX.25/AFSK path.

    static constexpr uint32_t kTickMs = 1000;  // scheduler tick period

    static pakt::AprsTaskContext ctx(
        // RadioTxFn stub – returns true so scheduler advances state.
        // Replace with: encode APRS frame, push to audio/modem pipeline.
        [](const pakt::TxMessage &msg) -> bool {
            ESP_LOGI("aprs", "TX stub: dest=%s msg_id=%s text=%.20s",
                     msg.dest_callsign, msg.aprs_msg_id, msg.text);
            return true;
        },
        // NotifyFn: encode result JSON and push BLE notify.
        [](const char *msg_id, pakt::TxResultEvent event) {
            char buf[64];
            size_t n = pakt::TxResultEncoder::encode(msg_id, event, buf, sizeof(buf));
            if (n > 0) {
                pakt::BleServer::instance().notify_tx_result(
                    reinterpret_cast<const uint8_t *>(buf), n);
            }
        }
    );

    // PTT watchdog (FW-016): fires ptt_safe_off() if this task goes stale.
    // safe_fn runs in watchdog_task context when the heartbeat expires.
    // ptt_safe_off() calls the callback registered by radio_task (SA818 driver);
    // until that driver is wired it is a safe no-op (PTT default = OFF).
    static pakt::PttWatchdog watchdog(
        []() {
            const uint32_t now_ms =
                static_cast<uint32_t>(esp_timer_get_time() / 1000);
            ESP_LOGE("watchdog",
                     "PTT WATCHDOG TRIGGERED at t=%" PRIu32 " ms – calling ptt_safe_off()",
                     now_ms);
            pakt::ptt_safe_off();
        },
        pakt::PttWatchdog::kDefaultTimeoutMs
    );

    // Publish both pointers before entering the run loop.
    g_aprs_ctx     = &ctx;
    g_ptt_watchdog = &watchdog;

    ESP_LOGI("aprs", "task started (FW-016 watchdog armed, timeout=%" PRIu32 " ms)",
             pakt::PttWatchdog::kDefaultTimeoutMs);

    for (;;) {
        const uint32_t now_ms =
            static_cast<uint32_t>(esp_timer_get_time() / 1000);
        ctx.tick(now_ms);
        watchdog.heartbeat(now_ms);   // signal aprs_task is alive
        vTaskDelay(pdMS_TO_TICKS(kTickMs));
    }
}

static void watchdog_task(void *arg)
{
    // FW-016: PTT safe-off supervisor.
    // Runs at priority 6 (between aprs=5 and radio=7) so it can preempt aprs_task
    // if aprs_task hangs.  Ticks every 500 ms; fires at 10 s stale threshold.

    static constexpr uint32_t kTickMs = 500;

    ESP_LOGI("watchdog", "task started (FW-016, tick=%" PRIu32 " ms)", kTickMs);

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(kTickMs));

        if (!g_ptt_watchdog) continue;   // wait until aprs_task is ready

        const uint32_t now_ms =
            static_cast<uint32_t>(esp_timer_get_time() / 1000);
        g_ptt_watchdog->tick(now_ms);
    }
}

static void gps_task(void *arg)
{
    // Step 5: NMEA parser + stale-fix management (FW-005)
    //
    // TODO(hardware): replace uart_read_bytes stub with real UART driver for
    //                 NEO-M8N on the board's GPS UART port.
    //
    // The parser runs here; telemetry is published via BleServer::notify_gps()
    // once the BLE task is running and a client is subscribed.

    static constexpr uint32_t kStaleMs     = 5000;   // mark stale after 5 s silence
    static constexpr uint32_t kTickMs      = 100;    // task period
    static constexpr uint32_t kPublishMs   = 1000;   // publish GPS telemetry at 1 Hz

    ESP_LOGI("gps", "task started (FW-005)");

    pakt::NmeaParser parser;
    uint32_t last_valid_ms  = 0;
    uint32_t last_publish_ms = 0;

    for (;;) {
        const uint32_t now_ms = static_cast<uint32_t>(esp_timer_get_time() / 1000);

        // ── Stale-fix check ───────────────────────────────────────────────────
        if (parser.valid() && (now_ms - last_valid_ms) >= kStaleMs) {
            ESP_LOGW("gps", "GPS fix stale (>%u ms)", kStaleMs);
            parser.mark_stale();
        }

        // ── UART read placeholder ─────────────────────────────────────────────
        // Replace with:
        //   uint8_t byte;
        //   while (uart_read_bytes(GPS_UART_NUM, &byte, 1, 0) == 1) {
        //       if (parser.feed(byte)) last_valid_ms = now_ms;
        //   }

        // ── Publish GPS telemetry via BLE ─────────────────────────────────────
        if ((now_ms - last_publish_ms) >= kPublishMs && parser.valid()) {
            const pakt::GpsTelem &fix = parser.fix();
            char buf[256];
            size_t n = fix.to_json(buf, sizeof(buf));
            if (n > 0) {
                pakt::BleServer::instance().notify_gps(
                    reinterpret_cast<const uint8_t *>(buf), n);
            }
            last_publish_ms = now_ms;
        }

        vTaskDelay(pdMS_TO_TICKS(kTickMs));
    }
}

static void ble_task(void *arg)
{
    // Step 4: BLE GATT services + encrypted/bonded write policy.
    // Stub handlers – replaced when APRS logic (Step 6/7) is wired in.
    pakt::BleServer::Handlers handlers;
    handlers.on_config_read = [](uint8_t *buf, size_t max) -> size_t {
        // Return live config from the store; g_device_config is file-static with
        // static storage duration — no lambda capture needed (C++17 §6.7.1).
        return pakt::DeviceConfigStore::config_to_json(
            g_device_config.config(),
            reinterpret_cast<char *>(buf), max);
    };
    handlers.on_config_write = [](const uint8_t *data, size_t len) -> bool {
        pakt::ConfigFields fields;
        if (!pakt::PayloadValidator::validate_config_payload(data, len, &fields)) {
            ESP_LOGW("ble", "config write rejected: invalid payload");
            return false;
        }
        bool persisted = g_device_config.apply(fields);
        if (persisted) {
            ESP_LOGI("ble", "config write accepted: callsign=%s ssid=%u",
                     fields.callsign, fields.ssid);
        } else {
            // In-memory update still applied; NVS persist failed.
            // Runtime continues with new callsign; warn operator.
            ESP_LOGW("ble", "config write accepted (in-memory) but persist failed: "
                     "callsign=%s ssid=%u", fields.callsign, fields.ssid);
        }
        return true;   // BLE write accepted regardless of persist outcome
    };
    handlers.on_command = [](const uint8_t *, size_t) { return true; };
    handlers.on_tx_request = [](const uint8_t *data, size_t len) -> bool {
        pakt::TxRequestFields fields;
        if (!pakt::PayloadValidator::validate_tx_request_payload(data, len, &fields)) {
            ESP_LOGW("ble", "tx_request rejected: invalid payload");
            return false;
        }
        if (!g_aprs_ctx) {
            ESP_LOGW("ble", "tx_request rejected: APRS task not ready");
            return false;
        }
        if (!g_aprs_ctx->push_tx_request(fields)) {
            ESP_LOGW("ble", "tx_request rejected: ring buffer full");
            return false;
        }
        return true;
    };
    handlers.on_caps_read = [](uint8_t *buf, size_t max) -> size_t {
        auto caps = pakt::DeviceCapabilities::mvp_defaults();
        return caps.to_json(reinterpret_cast<char *>(buf), max);
    };

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
