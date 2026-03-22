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
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "nvs_flash.h"

#include "audio_bench_test.h"
#include "bench_profile_config.h"
#include "sa818_bench_test.h"
#include "pakt/BleServer.h"
#include "pakt/DeviceCapabilities.h"
#include "pakt/KissFramer.h"
#include "pakt/NmeaParser.h"
#include "pakt/AprsTaskContext.h"
#include "pakt/PayloadValidator.h"
#include "pakt/DeviceConfigStore.h"
#include "pakt/PttWatchdog.h"
#include "pakt/PttController.h"
#include "pakt/AfskDemodulator.h"
#include "pakt/AfskModulator.h"
#include "pakt/Aprs.h"
#include "pakt/Sa818Radio.h"
#include "NvsStorage.h"
#include "Sa818UartTransport.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "driver/uart.h"
#include "esp_heap_caps.h"
#include "esp_psram.h"
#include <atomic>
#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <cstring>

static const char *TAG = "pakt";

// Shared between aprs_task (owner) and ble_task (producer).
// Set by aprs_task before it enters its run loop; ble_task checks for null.
static pakt::AprsTaskContext *g_aprs_ctx = nullptr;

// PTT watchdog (FW-016). Pointer set by aprs_task before its run loop;
// watchdog_task calls tick() every 500 ms. Null until aprs_task is ready.
static pakt::PttWatchdog *g_ptt_watchdog = nullptr;

// ── TX pipeline shared state ──────────────────────────────────────────────────
//
// g_radio      – set by radio_task once Sa818Radio::init() succeeds.
// g_i2s_tx_chan– set by audio_task once the I2S TX channel is enabled.
// Both are read by aprs_task (afsk_tx_frame).  Written once; no mutex needed.
static pakt::Sa818Radio  *g_radio        = nullptr;
static i2s_chan_handle_t  g_i2s_tx_chan  = nullptr;

static constexpr uint32_t kAudioSampleRateHz = pakt::benchcfg::kAudioSampleRateHz;

// Rolling 1-second RX peak absolute value, updated by audio_task's demod loop.
// Read by sa818_bench Stage 6 (RX audio capture) via a non-capturing lambda.
// Atomic: audio_task writes, radio_task reads during bench window.
static std::atomic<int32_t>  g_rx_peak_abs{0};

// Cumulative demodulator diagnostics, published by audio_task every ~1 s.
// Read by aprs_task bench (Stage B) to distinguish "no signal" from "FCS failures".
//   g_demod_flags      : total 0x7E flag patterns detected (> 0 means AFSK lock)
//   g_demod_fcs_rejects: frames assembled but CRC failed (> 0 means near-decode)
static std::atomic<uint32_t> g_demod_flags{0};
static std::atomic<uint32_t> g_demod_fcs_rejects{0};

// Enhanced RX signal diagnostics, published by audio_task every ~1 s.
//   g_rx_mean_abs  : mean |sample| in last 1 s window (0-32767 scale).
//                   Bell 202 at full-scale sine would give ~20900; noise ~200-800.
//   g_rx_clip_count: samples with |x| > 28000 (≈85% FS) in last 1 s window.
//                   > 0 indicates ADC saturation / gain too high.
static std::atomic<uint32_t> g_rx_mean_abs{0};
static std::atomic<uint32_t> g_rx_clip_count{0};

// ADC gain request: 0–15 steps at +1.5 dB/step → 0 to +22.5 dB
// (SGTL5000 CHIP_ANA_ADC_CTRL, both channels).
// Set by aprs_task bench gain sweep; audio_task applies in its per-second tick.
static std::atomic<uint8_t>  g_adc_gain_req{0};

// PCM snapshot: 1024 raw mono int16 samples captured on demand.
// arm : aprs_task sets true; audio_task clears when collection starts.
// valid: audio_task sets true (release) when 1024 samples collected.
// aprs_task reads/logs the buffer then clears valid and arm for the next test.
static constexpr size_t kPcmCapLen =
    static_cast<size_t>(kAudioSampleRateHz * 128u / 1000u);  // 128 ms window
static int16_t           g_pcm_cap[kPcmCapLen];
static std::atomic<bool> g_pcm_cap_arm{false};
static std::atomic<bool> g_pcm_cap_valid{false};

// Full RX debug recorder: 30 s mono PCM at the configured audio sample rate,
// signed 16-bit.
// Backed by PSRAM when available so we can export the exact demod-input samples
// for offline analysis without 8-bit quantization loss.
static constexpr uint32_t kRxRecordSampleRateHz = kAudioSampleRateHz;
static constexpr uint32_t kRxRecordSeconds      = 30;
static constexpr size_t   kRxRecordSamples =
    static_cast<size_t>(kRxRecordSampleRateHz) * kRxRecordSeconds;
static int16_t            *g_rx_record_buf = nullptr;
static std::atomic<bool>   g_rx_record_arm{false};
static std::atomic<bool>   g_rx_record_active{false};
static std::atomic<bool>   g_rx_record_valid{false};
static std::atomic<size_t> g_rx_record_samples{0};

// PCM output buffer for AFSK modulation (global to avoid stack pressure).
// Max AX.25 = 330 B → ~44 800 samples at 16 kHz/1200 baud; keep margin above
// that so debug/sample-rate experiments do not truncate packet TX.
static constexpr size_t kAfskMaxPcmSamples = 60000;
static int16_t           g_tx_pcm_buf[kAfskMaxPcmSamples];

// Device config (callsign, ssid, radio settings).
// NVS backend attached in app_main() after nvs_flash_init().
// Until then (and if NVS init fails) updates are in-memory only.
static pakt::DeviceConfigStore g_device_config;
static pakt::NvsStorage        g_nvs_storage;

// ── Decoded AX.25 RX queue (audio_task → aprs_task) ──────────────────────────
//
// audio_task (producer): pushes raw AX.25 frames decoded by the AFSK modem.
// aprs_task  (consumer): drains the queue each tick; forwards to native BLE
//                        rx_packet notify and KISS RX notify.
//
// Thread model: SPSC — only audio_task writes head_, only aprs_task writes tail_.
// Depth=8 gives ~8 ms burst buffer at 1 packet/ms (typical APRS is << 1/s).
namespace {

struct Ax25RxQueue {
    static constexpr size_t kDepth    = 8;
    static constexpr size_t kMaxFrame = pakt::kKissMaxFrame;

    struct Entry { uint8_t data[kMaxFrame]; size_t len; };

    Entry  slots[kDepth]{};
    std::atomic<uint32_t> head_{0};
    std::atomic<uint32_t> tail_{0};

    // Producer (audio_task context): push a decoded AX.25 frame.
    bool push(const uint8_t *d, size_t l) {
        if (!d || l == 0 || l > kMaxFrame) return false;
        uint32_t h = head_.load(std::memory_order_relaxed);
        if ((h - tail_.load(std::memory_order_acquire)) >= kDepth) return false;
        auto &s = slots[h % kDepth];
        std::memcpy(s.data, d, l);
        s.len = l;
        head_.store(h + 1, std::memory_order_release);
        return true;
    }

    // Consumer (aprs_task context): pop one frame; returns false if empty.
    bool pop(uint8_t *out, size_t *out_len) {
        uint32_t t = tail_.load(std::memory_order_relaxed);
        if (t == head_.load(std::memory_order_acquire)) return false;
        const auto &s = slots[t % kDepth];
        std::memcpy(out, s.data, s.len);
        *out_len = s.len;
        tail_.store(t + 1, std::memory_order_release);
        return true;
    }
};

} // anonymous namespace

static Ax25RxQueue g_rx_ax25_queue;

static bool ensure_rx_record_buffer()
{
    if (g_rx_record_buf) return true;

    const size_t bytes = kRxRecordSamples * sizeof(int16_t);
    const size_t spiram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    const size_t spiram_largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
    const size_t internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const size_t internal_largest =
        heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    ESP_LOGI("aprs_bench",
             "RX recorder alloc request=%u bytes | PSRAM init=%s size=%u free=%u largest=%u | "
             "internal free=%u largest=%u",
             static_cast<unsigned>(bytes),
             esp_psram_is_initialized() ? "yes" : "no",
             static_cast<unsigned>(esp_psram_get_size()),
             static_cast<unsigned>(spiram_free),
             static_cast<unsigned>(spiram_largest),
             static_cast<unsigned>(internal_free),
             static_cast<unsigned>(internal_largest));

    void *p = heap_caps_malloc(kRxRecordSamples * sizeof(int16_t),
                               MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!p) {
        p = heap_caps_malloc(kRxRecordSamples * sizeof(int16_t), MALLOC_CAP_8BIT);
    }
    if (!p) {
        ESP_LOGE("aprs_bench", "RX recorder alloc failed (%u bytes)",
                 static_cast<unsigned>(bytes));
        return false;
    }
    g_rx_record_buf = static_cast<int16_t *>(p);
    ESP_LOGI("aprs_bench", "RX recorder buffer ready: %u samples (%u bytes) in %s",
             static_cast<unsigned>(kRxRecordSamples),
             static_cast<unsigned>(bytes),
             esp_ptr_external_ram(g_rx_record_buf) ? "PSRAM" : "internal RAM");
    return true;
}

static void write_le16(uint8_t *p, uint16_t v)
{
    p[0] = static_cast<uint8_t>(v & 0xFFu);
    p[1] = static_cast<uint8_t>((v >> 8) & 0xFFu);
}

static void write_le32(uint8_t *p, uint32_t v)
{
    p[0] = static_cast<uint8_t>(v & 0xFFu);
    p[1] = static_cast<uint8_t>((v >> 8) & 0xFFu);
    p[2] = static_cast<uint8_t>((v >> 16) & 0xFFu);
    p[3] = static_cast<uint8_t>((v >> 24) & 0xFFu);
}

static void make_wav_header(uint8_t header[44], uint32_t sample_rate_hz,
                            uint16_t channels, uint16_t bits_per_sample,
                            uint32_t data_bytes)
{
    std::memcpy(header + 0,  "RIFF", 4);
    write_le32(header + 4, 36u + data_bytes);
    std::memcpy(header + 8,  "WAVE", 4);
    std::memcpy(header + 12, "fmt ", 4);
    write_le32(header + 16, 16u);
    write_le16(header + 20, 1u);
    write_le16(header + 22, channels);
    write_le32(header + 24, sample_rate_hz);
    write_le32(header + 28, sample_rate_hz * channels * (bits_per_sample / 8u));
    write_le16(header + 32, static_cast<uint16_t>(channels * (bits_per_sample / 8u)));
    write_le16(header + 34, bits_per_sample);
    std::memcpy(header + 36, "data", 4);
    write_le32(header + 40, data_bytes);
}

static void emit_base64_lines(const uint8_t *data, size_t len,
                              pakt::PttWatchdog *watchdog = nullptr)
{
    static constexpr char kB64[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    char line[65];
    size_t line_pos = 0;
    size_t emitted_lines = 0;
    const bool task_wdt_registered = (esp_task_wdt_status(nullptr) == ESP_OK);

    for (size_t i = 0; i < len; i += 3) {
        const size_t remain = len - i;
        const uint32_t a = data[i];
        const uint32_t b = remain > 1 ? data[i + 1] : 0;
        const uint32_t c = remain > 2 ? data[i + 2] : 0;
        const uint32_t v = (a << 16) | (b << 8) | c;

        line[line_pos++] = kB64[(v >> 18) & 0x3Fu];
        line[line_pos++] = kB64[(v >> 12) & 0x3Fu];
        line[line_pos++] = remain > 1 ? kB64[(v >> 6) & 0x3Fu] : '=';
        line[line_pos++] = remain > 2 ? kB64[v & 0x3Fu] : '=';

        if (line_pos == 64) {
            line[64] = '\0';
            printf("%s\n", line);
            line_pos = 0;
            ++emitted_lines;

            if ((emitted_lines % 64u) == 0u) {
                fflush(stdout);
                if (watchdog) {
                    watchdog->heartbeat(
                        static_cast<uint32_t>(esp_timer_get_time() / 1000));
                }
                if (task_wdt_registered) {
                    esp_task_wdt_reset();
                }
                vTaskDelay(pdMS_TO_TICKS(1));
            }
        }
    }

    if (line_pos > 0) {
        line[line_pos] = '\0';
        printf("%s\n", line);
        ++emitted_lines;
    }

    if ((emitted_lines % 64u) != 0u) {
        fflush(stdout);
        if (watchdog) {
            watchdog->heartbeat(
                static_cast<uint32_t>(esp_timer_get_time() / 1000));
        }
        if (task_wdt_registered) {
            esp_task_wdt_reset();
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

static void dump_rx_record_wav_base64(pakt::PttWatchdog *watchdog = nullptr)
{
    if (!g_rx_record_valid.load(std::memory_order_acquire) || !g_rx_record_buf) {
        ESP_LOGW("aprs_bench", "RX recorder dump skipped: no valid capture");
        return;
    }

    const size_t sample_count =
        g_rx_record_samples.load(std::memory_order_acquire);
    const uint32_t data_bytes =
        static_cast<uint32_t>(sample_count * sizeof(int16_t));
    uint8_t wav_header[44];
    make_wav_header(wav_header, kRxRecordSampleRateHz, 1, 16, data_bytes);

    ESP_LOGI("aprs_bench", "RX recorder dump starting: %u samples (%u bytes WAV data)",
             static_cast<unsigned>(sample_count),
             static_cast<unsigned>(data_bytes));
    ESP_LOGI("aprs_bench", "Capture format: mono 16-bit PCM WAV @ %u Hz",
             static_cast<unsigned>(kRxRecordSampleRateHz));
    ESP_LOGI("aprs_bench",
             "Copy everything between BEGIN/END markers to reconstruct the WAV file.");

    printf("-----BEGIN PAKT RX WAV BASE64-----\n");
    emit_base64_lines(wav_header, sizeof(wav_header), watchdog);
    emit_base64_lines(reinterpret_cast<const uint8_t *>(g_rx_record_buf), data_bytes, watchdog);
    printf("-----END PAKT RX WAV BASE64-----\n");
    fflush(stdout);

    ESP_LOGI("aprs_bench", "RX recorder dump complete.");
}

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
    xTaskCreate(watchdog_task, "watchdog", 4096, nullptr, 6, nullptr);
    xTaskCreate(gps_task,      "gps",      4096, nullptr, 5, nullptr);
    xTaskCreate(aprs_task,     "aprs",     6144, nullptr, 5, nullptr);
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
    static constexpr int         kUartTxPin = 13;   // SA818_RX_CTRL (ESP TX → SA818 RXD)
    static constexpr int         kUartRxPin = 9;    // SA818_TX_STAT (SA818 TXD → ESP RX)
    static constexpr int         kBaud      = 9600;
    static constexpr uint32_t    kSa818BootSettleMs = 2000;

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
    esp_err_t err = uart_driver_install(kUartPort, 256, 0, 0, nullptr, 0);
    if (err != ESP_OK) {
        ESP_LOGE("radio", "uart_driver_install(UART_NUM_%d) failed: %s",
                 static_cast<int>(kUartPort), esp_err_to_name(err));
        for (;;) { vTaskDelay(pdMS_TO_TICKS(5000)); }
    }
    err = uart_param_config(kUartPort, &uart_cfg);
    if (err != ESP_OK) {
        ESP_LOGE("radio", "uart_param_config(UART_NUM_%d) failed: %s",
                 static_cast<int>(kUartPort), esp_err_to_name(err));
        for (;;) { vTaskDelay(pdMS_TO_TICKS(5000)); }
    }
    err = uart_set_pin(kUartPort, kUartTxPin, kUartRxPin,
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        ESP_LOGE("radio", "uart_set_pin(UART_NUM_%d, tx=%d, rx=%d) failed: %s",
                 static_cast<int>(kUartPort), kUartTxPin, kUartRxPin,
                 esp_err_to_name(err));
        for (;;) { vTaskDelay(pdMS_TO_TICKS(5000)); }
    }
    uart_flush_input(kUartPort);
    ESP_LOGI("radio",
             "UART%d configured: tx=GPIO%d rx=GPIO%d baud=%d; waiting %u ms for SA818 boot",
             static_cast<int>(kUartPort), kUartTxPin, kUartRxPin, kBaud,
             static_cast<unsigned>(kSa818BootSettleMs));
    vTaskDelay(pdMS_TO_TICKS(kSa818BootSettleMs));

    // Construct concrete transport and radio driver.
    static pakt::Sa818UartTransport transport(kUartPort);
    static pakt::Sa818Radio radio(
        transport,
        [](bool on){ gpio_set_level(kPttGpio, on ? 0 : 1); }
    );

    // Run staged SA818 bench test before normal init.
    // Stage 5 (TX audio) waits for g_i2s_tx_chan (published by audio_task after audio_bench).
    // Stage 6 (RX capture) runs after Stage 5; by then the demod loop is live and
    // rx_peak_fn returns the rolling 1-s peak_abs from g_rx_peak_abs.
    if constexpr (pakt::benchcfg::kEnableSa818Bench) {
        pakt::bench::run_sa818_bench(
            transport, kPttGpio, kUartPort,
            static_cast<const volatile void *>(&g_i2s_tx_chan),
            kAudioSampleRateHz,
            /*tx_wait_ms=*/120000,
            /*rx_peak_fn=*/[]() -> int32_t {
                return g_rx_peak_abs.load(std::memory_order_relaxed);
            });
    } else {
        ESP_LOGI("radio", "SA818 bench disabled by bench_profile_config.h");
    }

    if (!radio.init()) {
        ESP_LOGE("radio", "SA818 init failed – radio unavailable; PTT remains off");
        // Watchdog safe-off callback stays as direct GPIO lambda (safe).
        for (;;) { vTaskDelay(pdMS_TO_TICKS(5000)); }
    }

    ESP_LOGI("radio", "SA818 init OK");

    // Set APRS frequency (144.390 MHz simplex, 25 kHz wide, squelch 1).
    radio.set_freq(144390000U, 144390000U);

    // Update watchdog safe-off to go through driver (cleaner state tracking).
    // The direct GPIO lambda remains registered until this point to guard init().
    pakt::ptt_register_safe_off([](){ radio.ptt(false); });

    // Publish Sa818Radio pointer so aprs_task can call afsk_tx_frame().
    // Written once here; read by aprs_task (single writer, single reader — safe).
    g_radio = &radio;
    ESP_LOGI("radio", "radio ready; g_radio published for TX pipeline");

    // radio_task monitor loop: SA818 is now managed by aprs_task TX path.
    for (;;) { vTaskDelay(pdMS_TO_TICKS(1000)); }
}

// ── SGTL5000 helpers ─────────────────────────────────────────────────────────
//
// GPIO assignments (bench remap for Adafruit Feather ESP32-S3):
//   I2C: SDA=GPIO3, SCL=GPIO4
//   I2S: MCLK=GPIO14, BCLK=GPIO8, WS=GPIO15, DOUT=GPIO12, DIN=GPIO10
//
// SGTL5000 7-bit I2C addresses:
//   0x0A for the fixed-address package, or 0x0A/0x2A depending on ADR0 strap.
// The Teensy audio board variant should resolve to one of these once SYS_MCLK is
// running and the codec has left reset.
// Register protocol: [reg_hi][reg_lo][val_hi][val_lo] (all 16-bit)

static esp_err_t sgtl5000_write(i2c_master_dev_handle_t dev, uint16_t reg, uint16_t val)
{
    uint8_t buf[4] = {
        static_cast<uint8_t>(reg >> 8),
        static_cast<uint8_t>(reg & 0xFF),
        static_cast<uint8_t>(val >> 8),
        static_cast<uint8_t>(val & 0xFF),
    };
    return i2c_master_transmit(dev, buf, sizeof(buf), /*timeout_ms=*/10);
}

static esp_err_t sgtl5000_read(i2c_master_dev_handle_t dev, uint16_t reg, uint16_t *out)
{
    if (!out) return ESP_ERR_INVALID_ARG;
    uint8_t reg_buf[2] = {
        static_cast<uint8_t>(reg >> 8),
        static_cast<uint8_t>(reg & 0xFF),
    };
    uint8_t val_buf[2] = {0, 0};
    const esp_err_t err = i2c_master_transmit_receive(
        dev, reg_buf, sizeof(reg_buf), val_buf, sizeof(val_buf), /*timeout_ms=*/10);
    if (err != ESP_OK) return err;
    *out = static_cast<uint16_t>((static_cast<uint16_t>(val_buf[0]) << 8) | val_buf[1]);
    return ESP_OK;
}

static const char *rx_input_mode_name()
{
    using pakt::benchcfg::RxInputMode;
    switch (pakt::benchcfg::kRxInputMode) {
    case RxInputMode::Left:    return "left";
    case RxInputMode::Right:   return "right";
    case RxInputMode::Average: return "average";
    case RxInputMode::Stronger:return "stronger";
    }
    return "unknown";
}

static int16_t maybe_byteswap_sample(int16_t s)
{
    if constexpr (!pakt::benchcfg::kRxByteSwapSamples) {
        return s;
    } else {
        const uint16_t u = static_cast<uint16_t>(s);
        return static_cast<int16_t>((u << 8) | (u >> 8));
    }
}

static int16_t select_rx_sample(int16_t left, int16_t right,
                                uint32_t *left_abs_sum = nullptr,
                                uint32_t *right_abs_sum = nullptr,
                                int32_t *left_peak = nullptr,
                                int32_t *right_peak = nullptr)
{
    left = maybe_byteswap_sample(left);
    right = maybe_byteswap_sample(right);

    const int32_t left_abs = left >= 0 ? left : -static_cast<int32_t>(left);
    const int32_t right_abs = right >= 0 ? right : -static_cast<int32_t>(right);
    if (left_abs_sum)  *left_abs_sum += static_cast<uint32_t>(left_abs);
    if (right_abs_sum) *right_abs_sum += static_cast<uint32_t>(right_abs);
    if (left_peak && left_abs > *left_peak)   *left_peak = left_abs;
    if (right_peak && right_abs > *right_peak) *right_peak = right_abs;

    using pakt::benchcfg::RxInputMode;
    switch (pakt::benchcfg::kRxInputMode) {
    case RxInputMode::Left:
        return left;
    case RxInputMode::Right:
        return right;
    case RxInputMode::Average:
        return static_cast<int16_t>((static_cast<int32_t>(left) + right) / 2);
    case RxInputMode::Stronger:
        return (left_abs >= right_abs) ? left : right;
    }
    return left;
}

static int16_t dc_block_sample(int16_t s)
{
    if constexpr (!pakt::benchcfg::kRxEnableDcBlock) {
        return s;
    } else {
        static float prev_x = 0.0f;
        static float prev_y = 0.0f;
        const float x = static_cast<float>(s);
        const float y = x - prev_x + pakt::benchcfg::kRxDcBlockPole * prev_y;
        prev_x = x;
        prev_y = y;
        const float clipped = y > 32767.0f ? 32767.0f : (y < -32768.0f ? -32768.0f : y);
        return static_cast<int16_t>(clipped);
    }
}

static void log_sgtl5000_readback(i2c_master_dev_handle_t dev)
{
    if constexpr (!pakt::benchcfg::kLogSgtl5000Readback) {
        return;
    }
    static constexpr struct {
        uint16_t reg;
        const char *name;
    } kRegs[] = {
        {0x0002, "CHIP_DIG_POWER"},
        {0x0004, "CHIP_CLK_CTRL"},
        {0x0006, "CHIP_I2S_CTRL"},
        {0x000A, "CHIP_SSS_CTRL"},
        {0x000E, "CHIP_ADCDAC_CTRL"},
        {0x0020, "CHIP_ANA_ADC_CTRL"},
        {0x0024, "CHIP_ANA_CTRL"},
        {0x0030, "CHIP_ANA_POWER"},
    };
    for (const auto &r : kRegs) {
        uint16_t val = 0;
        const esp_err_t err = sgtl5000_read(dev, r.reg, &val);
        if (err == ESP_OK) {
            ESP_LOGI("audio", "SGTL5000 readback %s (0x%04X) = 0x%04X",
                     r.name, r.reg, val);
        } else {
            ESP_LOGW("audio", "SGTL5000 readback %s (0x%04X) failed: %s",
                     r.name, r.reg, esp_err_to_name(err));
        }
    }
}

static void i2c_scan_bus(i2c_master_bus_handle_t bus)
{
    ESP_LOGI("i2c", "Scanning I2C bus for devices");
    bool found = false;
    for (uint8_t addr = 0x08; addr < 0x78; ++addr) {
        i2c_master_dev_handle_t probe = nullptr;
        i2c_device_config_t cfg = {};
        cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        cfg.device_address  = addr;
        cfg.scl_speed_hz    = 100000;
        if (i2c_master_bus_add_device(bus, &cfg, &probe) != ESP_OK) {
            continue;
        }

        const esp_err_t err = i2c_master_probe(bus, addr, 10);
        i2c_master_bus_rm_device(probe);

        if (err == ESP_OK) {
            ESP_LOGI("i2c", "Found device at 0x%02X", addr);
            found = true;
        }
    }

    if (!found) {
        ESP_LOGW("i2c", "No I2C devices detected");
    }
}

static void i2c_scan_bus_repeated(i2c_master_bus_handle_t bus,
                                  uint32_t duration_ms,
                                  uint32_t interval_ms)
{
    const int64_t deadline_us =
        esp_timer_get_time() + static_cast<int64_t>(duration_ms) * 1000;

    while (esp_timer_get_time() < deadline_us) {
        i2c_scan_bus(bus);
        vTaskDelay(pdMS_TO_TICKS(interval_ms));
    }
}

static bool i2c_probe_addr(i2c_master_bus_handle_t bus, uint8_t addr)
{
    return i2c_master_probe(bus, addr, 10) == ESP_OK;
}

struct AudioClockPlan {
    uint16_t clk_ctrl_reg;
    i2s_mclk_multiple_t mclk_multiple;
    uint32_t mclk_hz;
    uint32_t bclk_hz;
    const char *summary;
};

static bool make_audio_clock_plan(uint32_t sample_rate_hz, AudioClockPlan *out)
{
    if (!out) return false;
    switch (sample_rate_hz) {
    case 8000:
        *out = AudioClockPlan{
            .clk_ctrl_reg = 0x0020,                // SYS_FS=32 kHz, RATE_MODE=/4
            .mclk_multiple = I2S_MCLK_MULTIPLE_1024,
            .mclk_hz = 8192000,
            .bclk_hz = sample_rate_hz * 2u * 16u,
            .summary = "SYS_FS=32kHz RATE_MODE=/4 MCLK=8.192MHz",
        };
        return true;
    case 16000:
        *out = AudioClockPlan{
            .clk_ctrl_reg = 0x0010,                // SYS_FS=32 kHz, RATE_MODE=/2
            .mclk_multiple = I2S_MCLK_MULTIPLE_512,
            .mclk_hz = 8192000,
            .bclk_hz = sample_rate_hz * 2u * 16u,
            .summary = "SYS_FS=32kHz RATE_MODE=/2 MCLK=8.192MHz",
        };
        return true;
    default:
        return false;
    }
}

// audio_write_square_wave and audio_bench_self_test removed.
// Bench test logic lives in audio_bench_test/audio_bench_test.cpp.

// Power-up sequence for both ADC (RX: SA818 AF_OUT → line-in → I2S)
// and DAC (TX: I2S → DAC → line-out → SA818 AF_IN) paths at the configured
// sample rate.
//
// Clock plan:
//   8 kHz  -> SYS_FS=32 kHz, RATE_MODE=/4, MCLK=8.192 MHz
//   16 kHz -> SYS_FS=32 kHz, RATE_MODE=/2, MCLK=8.192 MHz
// Both modes keep the codec master clock at 8.192 MHz while changing the
// effective ADC/DAC sample rate through RATE_MODE.
//
// Gain calibration (ADC input level and DAC output level) is a hardware bring-up task.
// CHIP_ANA_POWER bit layout assumed from SGTL5000 datasheet §6.5:
//   bit 14: VCOAMP_POWERUP  bit 13: VAG_POWERUP  bit 12: ADC_MONO
//   bit 11: REFTOP_POWERUP  bit 9: DAC_POWERUP   bit 7: ADC_POWERUP
//   bit 6: LINEOUT_POWERUP  bit 5: LINEOUT_MONO
// Verify every bit against datasheet before first power-on.
static bool sgtl5000_init(i2c_master_dev_handle_t dev, uint32_t sample_rate_hz)
{
    AudioClockPlan clock_plan{};
    if (!make_audio_clock_plan(sample_rate_hz, &clock_plan)) {
        ESP_LOGE("audio", "Unsupported audio sample rate: %u Hz",
                 static_cast<unsigned>(sample_rate_hz));
        return false;
    }

    // 1. Partial analog power-up: VCOAMP (bias) only.
    //    Full power-up in step 8 after digital blocks are configured.
    if (sgtl5000_write(dev, 0x0030, 0x40A0) != ESP_OK) return false;
    vTaskDelay(pdMS_TO_TICKS(10));   // allow VCOAMP/VAG to ramp

    // 2. Reference voltages (VAG=1.575 V, bias nominal).
    if (sgtl5000_write(dev, 0x0028, 0x01F2) != ESP_OK) return false;

    // 3. Line-out control: output level and bias for SA818 AF_IN.
    if (sgtl5000_write(dev, 0x002C, 0x0F22) != ESP_OK) return false;

    // 4. Short-circuit protection (PJRC reference value).
    if (sgtl5000_write(dev, 0x003C, 0x4446) != ESP_OK) return false;

    // 5. Linear regulator: default.
    if (sgtl5000_write(dev, 0x0026, 0x006C) != ESP_OK) return false;

    // 6. Digital power: I2S_IN + I2S_OUT + ADC + DAC + DAP all on.
    //    0x0073 = bit6(ADC)+bit5(DAC)+bit4(DAP)+bit1(I2S_IN)+bit0(I2S_OUT)
    if (sgtl5000_write(dev, 0x0002, 0x0073) != ESP_OK) return false;

    // 7. Clock: selected from the build-time audio clock plan.
    if (sgtl5000_write(dev, 0x0004, clock_plan.clk_ctrl_reg) != ESP_OK) return false;

    // 8. I2S: slave mode (MS=0), 16-bit stereo Philips.
    //    CHIP_I2S_CTRL bit layout (SGTL5000 datasheet §6.3):
    //      [8]   MS       = 0 (slave; ESP32-S3 is master)
    //      [7]   SCLKFREQ = 0 (32×Fs; matches 16-bit stereo → 2×16 = 32 BCLK/frame)
    //      [5:4] DLEN     = 11 → 16-bit  ← field is at bits[5:4], NOT bits[3:2]
    //      [1:0] MODE     = 00 → I2S/Philips
    //    CHIP_I2S_CTRL = 0x0030
    if (sgtl5000_write(dev, 0x0006, 0x0030) != ESP_OK) return false;

    // 9. Signal routing (CHIP_SSS_CTRL 0x000A).
    //    Reset value = 0x0010 (PJRC keeps it).  Bit layout:
    //      [5:4] DAP_SELECT = 01 → I2S_IN feeds DAP (pass-through by default)
    //      [13:12] DAC_SELECT = 00 → DAP_OUT feeds DAC  (I2S→DAP→DAC→HP ✓)
    //      [1:0]  I2S_SELECT = 00 → ADC feeds I2S output (ADC→ESP RX path ✓)
    //    0x0010 = DAP_SELECT=I2S_IN; DAC takes from DAP output.
    //    (Our previous 0x1000 set bits[13:12]=01 = ADC→DAC, silencing HP output.)
    if (sgtl5000_write(dev, 0x000A, 0x0010) != ESP_OK) return false;

    // 10. ADC/DAC control: HPF active on ADC, no mute on ADC or DAC.
    if (sgtl5000_write(dev, 0x000E, 0x0000) != ESP_OK) return false;

    // 11. Analog control (CHIP_ANA_CTRL 0x0024):
    //    SELECT_HP=0 → HP driven by DAC (not line-in bypass).
    //    MUTE_HP=0   → HP unmuted.
    //    SELECT_ADC=1 (bit2) → ADC source = LINE_IN (3-pin header on PJRC board).
    //    EN_ZCD_HP=1 (bit1) → zero-cross detect for HP (matches PJRC).
    //    0x0006 = bit2 | bit1
    if (sgtl5000_write(dev, 0x0024, 0x0006) != ESP_OK) return false;

    // 12. Mic bias (CHIP_MIC_CTRL 0x002A): 3 V bias, 2 kΩ impedance.
    //    Needed for the separate MIC header on the PJRC board; harmless during HP test.
    if (sgtl5000_write(dev, 0x002A, 0x0254) != ESP_OK) return false;

    // 13. ADC volume: 0 dB starting point.
    if (sgtl5000_write(dev, 0x0020, 0x0000) != ESP_OK) return false;

    // 14. DAC volume: 0 dB both channels (0x3C3C per datasheet).
    if (sgtl5000_write(dev, 0x0010, 0x3C3C) != ESP_OK) return false;

    // 15. Line-out volume (CHIP_LINE_OUT_VOL 0x002E): PJRC reference level.
    if (sgtl5000_write(dev, 0x002E, 0x1D1D) != ESP_OK) return false;

    // 16. Full analog power-up (CHIP_ANA_POWER 0x0030) = 0x40FF.
    //    PJRC Teensy Audio Adapter Rev D has DC-blocking caps on HP output,
    //    so CAPLESS_HP_POWERUP (bit 9 = 0x0200) must NOT be set.
    //    (Previous value 0x42FF incorrectly set bit 9; HP amp may have misbehaved.)
    if (sgtl5000_write(dev, 0x0030, 0x40FF) != ESP_OK) return false;
    vTaskDelay(pdMS_TO_TICKS(5));

    ESP_LOGI("audio",
             "SGTL5000 init OK: %u Hz, I2S→DAP→DAC→HP, ADC=LINE_IN, %s",
             static_cast<unsigned>(sample_rate_hz),
             clock_plan.summary);
    return true;
}

// ── Goertzel tone energy estimator ───────────────────────────────────────────
// Computes normalised power at a given frequency in a short PCM block.
// Returns a value in [0, ~1]; a pure sine at FS amplitude ≈ 0.25.
static float goertzel_power(const int16_t *samples, size_t n, float freq_hz, float sr_hz)
{
    const float coeff = 2.0f * std::cos(2.0f * static_cast<float>(M_PI) * freq_hz / sr_hz);
    float s0 = 0.0f, s1 = 0.0f, s2 = 0.0f;
    float scale = 1.0f / 32768.0f;
    for (size_t i = 0; i < n; ++i) {
        s0 = coeff * s1 - s2 + static_cast<float>(samples[i]) * scale;
        s2 = s1;
        s1 = s0;
    }
    // Power = s1² + s2² - coeff·s1·s2, normalised by N²
    float power = (s1 * s1 + s2 * s2 - coeff * s1 * s2) / static_cast<float>(n * n);
    return power;
}

// ── Audio pipeline ────────────────────────────────────────────────────────────
//
// Called once by audio_task. Returns only on unrecoverable init failure.
// On success, enters the per-sample run loop and never returns.

static void audio_pipeline_run()
{
    static constexpr size_t   kDmaFrames     = 256;   // frames per DMA block
    const uint32_t kSampleRateHz = kAudioSampleRateHz;
    AudioClockPlan clock_plan{};
    if (!make_audio_clock_plan(kSampleRateHz, &clock_plan)) {
        ESP_LOGE("audio", "Unsupported configured sample rate: %u Hz",
                 static_cast<unsigned>(kSampleRateHz));
        return;
    }

    // ── I2C master bus ────────────────────────────────────────────────────────
    i2c_master_bus_handle_t i2c_bus = nullptr;
    {
        // Feather ESP32-S3 uses GPIO7 to enable QT/STEMMA power and pull-ups.
        gpio_config_t i2c_power_cfg = {};
        i2c_power_cfg.pin_bit_mask = (1ULL << GPIO_NUM_7);
        i2c_power_cfg.mode         = GPIO_MODE_OUTPUT;
        i2c_power_cfg.pull_up_en   = GPIO_PULLUP_DISABLE;
        i2c_power_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
        i2c_power_cfg.intr_type    = GPIO_INTR_DISABLE;
        gpio_config(&i2c_power_cfg);
        gpio_set_level(GPIO_NUM_7, 1);

        i2c_master_bus_config_t cfg = {};
        cfg.i2c_port            = I2C_NUM_0;
        cfg.sda_io_num          = GPIO_NUM_3;
        cfg.scl_io_num          = GPIO_NUM_4;
        cfg.clk_source          = I2C_CLK_SRC_DEFAULT;
        cfg.glitch_ignore_cnt   = 7;
        cfg.flags.enable_internal_pullup = false;
        if (i2c_new_master_bus(&cfg, &i2c_bus) != ESP_OK) {
            ESP_LOGE("audio", "I2C master bus init failed");
            return;
        }

        // Bench bring-up aid: rescan for 35 s so a host can attach late and still
        // observe every responding address on the shared audio/Qwiic bus.
        i2c_scan_bus_repeated(i2c_bus, 35000, 5000);
    }

    // ── I2S full-duplex channel (ESP32-S3 master, SGTL5000 slave) ────────────
    // Both TX (AFSK modulator → DAC → SA818 AF_IN) and RX (SA818 AF_OUT → ADC → demod)
    // share the same I2S port so they use one coherent MCLK/BCLK/WS.
    //
    // MCLK plan is selected by make_audio_clock_plan():
    //   8 kHz  -> 8.192 MHz MCLK, 256 kHz BCLK
    //   16 kHz -> 8.192 MHz MCLK, 512 kHz BCLK
    i2s_chan_handle_t tx_chan = nullptr;
    i2s_chan_handle_t rx_chan = nullptr;
    {
        i2s_chan_config_t cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
        cfg.dma_desc_num  = 4;
        cfg.dma_frame_num = kDmaFrames;
        if (i2s_new_channel(&cfg, &tx_chan, &rx_chan) != ESP_OK) {
            ESP_LOGE("audio", "I2S channel create failed");
            return;
        }
    }

    {
        i2s_std_config_t cfg      = {};
        cfg.clk_cfg               = I2S_STD_CLK_DEFAULT_CONFIG(kSampleRateHz);
        cfg.clk_cfg.mclk_multiple = clock_plan.mclk_multiple;
        cfg.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
        cfg.gpio_cfg.mclk = GPIO_NUM_14;
        cfg.gpio_cfg.bclk = GPIO_NUM_8;
        cfg.gpio_cfg.ws   = GPIO_NUM_15;
        cfg.gpio_cfg.dout = GPIO_NUM_12;  // ESP → codec DAC (AFSK TX path)
        cfg.gpio_cfg.din  = GPIO_NUM_10;  // codec ADC → ESP (AFSK RX path)

        // Configure both channels with the same clock/GPIO settings.
        // TX channel uses dout; RX channel uses din; both share mclk/bclk/ws.
        if (i2s_channel_init_std_mode(tx_chan, &cfg) != ESP_OK) {
            ESP_LOGE("audio", "I2S TX std mode init failed");
            return;
        }
        if (i2s_channel_init_std_mode(rx_chan, &cfg) != ESP_OK) {
            ESP_LOGE("audio", "I2S RX std mode init failed");
            return;
        }
    }

    if (i2s_channel_enable(tx_chan) != ESP_OK) {
        ESP_LOGE("audio", "I2S TX channel enable failed");
        return;
    }
    if (i2s_channel_enable(rx_chan) != ESP_OK) {
        ESP_LOGE("audio", "I2S RX channel enable failed");
        return;
    }

    // The SGTL5000 only leaves reset after SYS_MCLK is present.
    vTaskDelay(pdMS_TO_TICKS(5));
    ESP_LOGI("audio", "I2S clocks enabled; rescanning I2C bus with MCLK active");
    i2c_scan_bus(i2c_bus);

    // ── SGTL5000 I2C device ───────────────────────────────────────────────────
    static constexpr uint8_t kSgtl5000CandidateAddrs[] = {0x0A, 0x2A};
    uint8_t codec_addr = 0;
    for (uint8_t addr : kSgtl5000CandidateAddrs) {
        if (i2c_probe_addr(i2c_bus, addr)) {
            codec_addr = addr;
            break;
        }
    }
    if (codec_addr == 0) {
        ESP_LOGE("audio", "No SGTL5000 candidate address responded with MCLK active");
        return;
    }
    ESP_LOGI("audio", "Using SGTL5000 candidate address 0x%02X", codec_addr);

    i2c_master_dev_handle_t sgtl_dev = nullptr;
    {
        i2c_device_config_t cfg = {};
        cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        cfg.device_address  = codec_addr;
        cfg.scl_speed_hz    = 400000;
        if (i2c_master_bus_add_device(i2c_bus, &cfg, &sgtl_dev) != ESP_OK) {
            ESP_LOGE("audio", "SGTL5000 I2C device add failed");
            return;
        }
    }

    if (!sgtl5000_init(sgtl_dev, kSampleRateHz)) {
        ESP_LOGE("audio", "SGTL5000 init failed – check I2C wiring");
        return;
    }
    log_sgtl5000_readback(sgtl_dev);

    // Run human-operated bench test (HP output tones + LINE_IN input monitor).
    // The PJRC HP jack is TRS output-only; MIC is a separate header (not tested here).
    if constexpr (pakt::benchcfg::kEnableAudioBench) {
        pakt::bench::run_audio_bench(tx_chan, rx_chan, kSampleRateHz);
    } else {
        ESP_LOGI("audio", "Audio bench disabled by bench_profile_config.h");
    }

    // Publish TX handle so aprs_task can call afsk_tx_frame().
    g_i2s_tx_chan = tx_chan;

    // ── AfskDemodulator + sample loop ─────────────────────────────────────────
    // The demodulator callback runs in this task context (no RTOS crossing needed).
    pakt::AfskDemodulator demod(kSampleRateHz,
        [](const uint8_t *frame, size_t len) {
            // Log every decoded frame so bench work can see demod activity.
            ESP_LOGI("audio", "AFSK: decoded AX.25 frame (%u bytes) → queue",
                     static_cast<unsigned>(len));
            if (!g_rx_ax25_queue.push(frame, len)) {
                ESP_LOGW("audio", "RX AX.25 queue full – frame dropped");
            }
        });

    static int16_t rx_buf[kDmaFrames * 2];
    static int16_t mono_buf[kDmaFrames];

    ESP_LOGI("audio", "RX pipeline running: SGTL5000 + AfskDemodulator @ %u Hz",
             static_cast<unsigned>(kSampleRateHz));
    ESP_LOGI("audio",
             "RX sample path: mode=%s swap_slots=%s byte_swap=%s dc_block=%s pole=%.3f",
             rx_input_mode_name(),
             pakt::benchcfg::kRxSwapStereoSlots ? "yes" : "no",
             pakt::benchcfg::kRxByteSwapSamples ? "yes" : "no",
             pakt::benchcfg::kRxEnableDcBlock ? "yes" : "no",
             static_cast<double>(pakt::benchcfg::kRxDcBlockPole));

    // Rolling 1-second RX diagnostics.
    static constexpr int32_t kClipThresh = 28000; // ~85% of ±32767
    int32_t  rx_window_peak    = 0;
    uint32_t rx_window_samples = 0;
    uint32_t rx_window_abs_sum = 0;
    uint32_t rx_window_clips   = 0;
    uint32_t rx_left_abs_sum   = 0;
    uint32_t rx_right_abs_sum  = 0;
    int32_t  rx_left_peak      = 0;
    int32_t  rx_right_peak     = 0;

    // PCM snapshot state — filled across multiple I2S reads until kPcmCapLen is reached.
    size_t pcm_cap_pos        = 0;
    bool   pcm_cap_collecting = false;

    // Full RX recorder state — captures the actual mono demod input for offline analysis.
    size_t rx_record_pos        = 0;
    bool   rx_record_collecting = false;
    bool   rx_record_dumped     = false;
    bool   auto_record_armed    = false;
    const int64_t auto_record_arm_us =
        esp_timer_get_time() +
        static_cast<int64_t>(pakt::benchcfg::kAutoStartRxRecorderDelayMs) * 1000LL;

    // ADC gain tracking: apply change when aprs_task updates g_adc_gain_req.
    uint8_t applied_gain = 0;

    for (;;) {
        if constexpr (pakt::benchcfg::kEnableAprsStageCRxRecord &&
                      pakt::benchcfg::kAutoStartRxRecorderOnBoot) {
            if (!auto_record_armed &&
                !g_rx_record_active.load(std::memory_order_relaxed) &&
                !g_rx_record_valid.load(std::memory_order_relaxed) &&
                esp_timer_get_time() >= auto_record_arm_us &&
                ensure_rx_record_buffer()) {
                g_adc_gain_req.store(pakt::benchcfg::kAprsStageCRecordAdcGainStep,
                                     std::memory_order_relaxed);
                g_rx_record_samples.store(0, std::memory_order_relaxed);
                g_rx_record_valid.store(false, std::memory_order_relaxed);
                g_rx_record_arm.store(true, std::memory_order_release);
                auto_record_armed = true;
                ESP_LOGI("audio",
                         "AUTO RECORD: armed 30 s RX capture at +%.1f dB (%u Hz). SEND APRS NOW.",
                         static_cast<double>(pakt::benchcfg::kAprsStageCRecordAdcGainDb),
                         static_cast<unsigned>(kRxRecordSampleRateHz));
            }
            if (auto_record_armed &&
                !rx_record_dumped &&
                g_rx_record_valid.load(std::memory_order_acquire)) {
                dump_rx_record_wav_base64(nullptr);
                rx_record_dumped = true;
            }
        }

        size_t bytes_read = 0;
        esp_err_t err = i2s_channel_read(rx_chan, rx_buf, sizeof(rx_buf),
                                         &bytes_read, pdMS_TO_TICKS(50));
        if (err == ESP_OK && bytes_read > 0) {
            const size_t stereo_samples = bytes_read / sizeof(int16_t);
            const size_t frames = stereo_samples / 2;

            // Select the configured RX input view from the stereo I2S frame,
            // then feed that conditioned mono stream into the recorder/demod.
            for (size_t i = 0; i < frames; ++i) {
                int16_t left  = rx_buf[i * 2];
                int16_t right = rx_buf[i * 2 + 1];
                if constexpr (pakt::benchcfg::kRxSwapStereoSlots) {
                    const int16_t tmp = left;
                    left = right;
                    right = tmp;
                }
                mono_buf[i] = dc_block_sample(
                    select_rx_sample(left, right,
                                     &rx_left_abs_sum, &rx_right_abs_sum,
                                     &rx_left_peak, &rx_right_peak));
                const int32_t a = mono_buf[i] >= 0 ? mono_buf[i] : -mono_buf[i];
                if (a > rx_window_peak)  rx_window_peak = a;
                rx_window_abs_sum += static_cast<uint32_t>(a);
                if (a > kClipThresh)     ++rx_window_clips;
            }
            rx_window_samples += static_cast<uint32_t>(frames);

            // PCM snapshot: wait for a clear signal event (peak > 5000) before capturing.
            // Threshold of 5000 ensures noise floor (SA818 SQ=0 peak ~1000) cannot
            // trigger the capture; a real AFSK burst peaks > 10000 at nominal levels.
            if (!pcm_cap_collecting &&
                g_pcm_cap_arm.load(std::memory_order_relaxed) &&
                rx_window_peak > 5000) {
                pcm_cap_collecting = true;
                pcm_cap_pos = 0;
                g_pcm_cap_arm.store(false, std::memory_order_relaxed);
            }
            if (pcm_cap_collecting) {
                const size_t cap_copy =
                    frames < (kPcmCapLen - pcm_cap_pos)
                        ? frames
                        : (kPcmCapLen - pcm_cap_pos);
                std::memcpy(g_pcm_cap + pcm_cap_pos,
                            mono_buf, cap_copy * sizeof(int16_t));
                pcm_cap_pos += cap_copy;
                if (pcm_cap_pos >= kPcmCapLen) {
                    pcm_cap_collecting = false;
                    g_pcm_cap_valid.store(true, std::memory_order_release);
                }
            }

            // Full 30 s RX recording — starts when aprs_task arms it and captures
            // the same mono samples passed into the demodulator.
            if (!rx_record_collecting &&
                g_rx_record_arm.load(std::memory_order_relaxed) &&
                g_rx_record_buf) {
                rx_record_collecting = true;
                rx_record_pos = 0;
                g_rx_record_arm.store(false, std::memory_order_relaxed);
                g_rx_record_active.store(true, std::memory_order_release);
                g_rx_record_valid.store(false, std::memory_order_relaxed);
                g_rx_record_samples.store(0, std::memory_order_relaxed);
                ESP_LOGI("audio", "RX recorder started (%u s @ %u Hz)",
                         static_cast<unsigned>(kRxRecordSeconds),
                         static_cast<unsigned>(kRxRecordSampleRateHz));
            }
            if (rx_record_collecting) {
                const size_t cap_copy =
                    frames < (kRxRecordSamples - rx_record_pos)
                        ? frames
                        : (kRxRecordSamples - rx_record_pos);
                for (size_t i = 0; i < cap_copy; ++i) {
                    g_rx_record_buf[rx_record_pos + i] =
                        static_cast<int16_t>(mono_buf[i]);
                }
                rx_record_pos += cap_copy;
                g_rx_record_samples.store(rx_record_pos, std::memory_order_relaxed);
                if (rx_record_pos >= kRxRecordSamples) {
                    rx_record_collecting = false;
                    g_rx_record_active.store(false, std::memory_order_release);
                    g_rx_record_valid.store(true, std::memory_order_release);
                    ESP_LOGI("audio", "RX recorder complete: %u samples captured",
                             static_cast<unsigned>(rx_record_pos));
                }
            }

            // Publish all diagnostics every ~1 second of audio.
            if (rx_window_samples >= kSampleRateHz) {
                g_rx_peak_abs.store(rx_window_peak, std::memory_order_relaxed);
                g_rx_mean_abs.store(rx_window_abs_sum / rx_window_samples,
                                    std::memory_order_relaxed);
                g_rx_clip_count.store(rx_window_clips, std::memory_order_relaxed);
                const auto ds = demod.stats();
                g_demod_flags.store(ds.flags, std::memory_order_relaxed);
                g_demod_fcs_rejects.store(ds.fcs_rejects, std::memory_order_relaxed);

                // Apply ADC gain change if requested by bench sweep.
                const uint8_t req_gain =
                    g_adc_gain_req.load(std::memory_order_relaxed);
                if (req_gain != applied_gain) {
                    applied_gain = req_gain;
                    // CHIP_ANA_ADC_CTRL (0x0020): bits[7:4]=ADC_VOL_R, bits[3:0]=ADC_VOL_L
                    const uint16_t adc_ctrl =
                        static_cast<uint16_t>((req_gain << 4) | req_gain);
                    if (sgtl5000_write(sgtl_dev, 0x0020, adc_ctrl) == ESP_OK) {
                        ESP_LOGI("audio",
                                 "ADC gain set: step=%u (+%.1f dB per ch, reg=0x%04X)",
                                 req_gain, req_gain * 1.5f, adc_ctrl);
                    } else {
                        ESP_LOGW("audio", "ADC gain write failed (step=%u)", req_gain);
                    }
                }

                if constexpr (pakt::benchcfg::kRxLogChannelStats) {
                    ESP_LOGI("audio",
                             "RX channel stats: L(mean=%u peak=%d) R(mean=%u peak=%d) mono(mean=%u peak=%d clips=%u)",
                             rx_window_samples ? (rx_left_abs_sum / rx_window_samples) : 0u,
                             rx_left_peak,
                             rx_window_samples ? (rx_right_abs_sum / rx_window_samples) : 0u,
                             rx_right_peak,
                             rx_window_samples ? (rx_window_abs_sum / rx_window_samples) : 0u,
                             rx_window_peak,
                             rx_window_clips);
                }

                rx_window_peak    = 0;
                rx_window_abs_sum = 0;
                rx_window_clips   = 0;
                rx_window_samples = 0;
                rx_left_abs_sum   = 0;
                rx_right_abs_sum  = 0;
                rx_left_peak      = 0;
                rx_right_peak     = 0;
            }

            demod.process(mono_buf, frames);
        } else if (err != ESP_ERR_TIMEOUT) {
            ESP_LOGW("audio", "I2S read error: %s", esp_err_to_name(err));
        }
    }
}

// ── AFSK TX pipeline ──────────────────────────────────────────────────────────
//
// Transmit a raw AX.25 frame as Bell 202 AFSK audio via I2S → SGTL5000 DAC → SA818.
// Called only from aprs_task context (single consumer, no mutex needed).
// Returns false if radio or audio pipeline is not yet initialised (hardware blocked).
static bool afsk_tx_frame(const uint8_t *ax25, size_t len)
{
    if (!g_radio || !g_i2s_tx_chan) {
        ESP_LOGW("aprs", "afsk_tx: pipeline not ready (radio=%s i2s=%s)",
                 g_radio       ? "ok" : "null",
                 g_i2s_tx_chan ? "ok" : "null");
        return false;
    }

    pakt::AfskModulator mod(kAudioSampleRateHz);
    size_t n_samples = mod.modulate_frame(ax25, len,
                                          g_tx_pcm_buf, kAfskMaxPcmSamples);
    if (n_samples == 0) {
        ESP_LOGE("aprs", "afsk_tx: modulation failed (ax25_len=%u, buf=%u)",
                 static_cast<unsigned>(len),
                 static_cast<unsigned>(kAfskMaxPcmSamples));
        return false;
    }

    if (!g_radio->ptt(true)) {
        ESP_LOGE("aprs", "afsk_tx: PTT assert failed");
        return false;
    }

    // SA818 TX path ramp-up: allow PA and squelch to settle.
    vTaskDelay(pdMS_TO_TICKS(10));

    // I2S channel is stereo (16-bit L + 16-bit R per frame).
    // The AFSK modulator produces mono samples; duplicate each sample to both
    // channels so the SA818 audio input (connected to LINE_OUT L or R) receives
    // the correct signal at the correct sample rate.
    // Writing mono bytes directly would split odd/even samples across L/R channels,
    // doubling the apparent sample rate and making AFSK completely undecodable.
    static int16_t stereo_chunk[512]; // 256 stereo frames, task-static (safe: single task)
    static constexpr size_t kChunkFrames = sizeof(stereo_chunk) / (2 * sizeof(int16_t));

    const uint32_t timeout_ms =
        static_cast<uint32_t>(n_samples * 1000u / kAudioSampleRateHz) + 500u;
    esp_err_t err      = ESP_OK;
    size_t    bytes_written = 0;
    size_t    remaining = n_samples;
    size_t    offset    = 0;

    while (remaining > 0) {
        const size_t frames = remaining > kChunkFrames ? kChunkFrames : remaining;
        for (size_t i = 0; i < frames; ++i) {
            stereo_chunk[i * 2]     = g_tx_pcm_buf[offset + i];
            stereo_chunk[i * 2 + 1] = g_tx_pcm_buf[offset + i];
        }
        size_t chunk_written = 0;
        err = i2s_channel_write(g_i2s_tx_chan,
                                stereo_chunk,
                                frames * 2u * sizeof(int16_t),
                                &chunk_written,
                                pdMS_TO_TICKS(timeout_ms));
        if (err != ESP_OK) {
            ESP_LOGE("aprs", "afsk_tx: I2S write error: %s", esp_err_to_name(err));
            break;
        }
        // Track written bytes in mono-equivalent units for the check below.
        bytes_written += chunk_written / 2u;
        remaining -= frames;
        offset    += frames;
    }

    // Wait for DMA to drain (4 descs × 256 frames / sample_rate).
    const uint32_t drain_ms =
        static_cast<uint32_t>((4u * 256u * 1000u) / kAudioSampleRateHz) + 32u;
    vTaskDelay(pdMS_TO_TICKS(drain_ms));

    g_radio->ptt(false);

    bool ok = (err == ESP_OK) && (bytes_written == n_samples * sizeof(int16_t));
    ESP_LOGI("aprs", "afsk_tx: %s (%u samples, %u ms)",
             ok ? "ok" : "FAIL",
             static_cast<unsigned>(n_samples),
             static_cast<unsigned>(n_samples * 1000u / kAudioSampleRateHz));
    return ok;
}

static void audio_task(void *arg)
{
    audio_pipeline_run();
    // audio_pipeline_run() returns only on init failure.
    ESP_LOGE("audio", "audio pipeline failed to start; task idle");
    for (;;) { vTaskDelay(pdMS_TO_TICKS(1000)); }
}

static void aprs_task(void *arg)
{
    // Step 3 + Step 7: AX.25/APRS framing + TX retry FSM + AFSK TX pipeline.
    //
    // RadioTxFn: encodes APRS message → AX.25 → AFSK → I2S → SA818.
    // RawTxFn:   KISS AX.25 bytes → AFSK → I2S → SA818 (no retry).
    // Both call afsk_tx_frame(); returns false gracefully if radio/audio not ready.
    static constexpr uint32_t kTickMs = 1000;  // scheduler tick period

    static pakt::AprsTaskContext ctx(
        // RadioTxFn: encode APRS message → AX.25 UI frame → AFSK → I2S → SA818.
        // Returns false if radio or audio pipeline not yet ready (graceful degrade).
        [](const pakt::TxMessage &msg) -> bool {
            // Build APRS information field: ":DEST     :text{msg_id}"
            uint8_t info_buf[pakt::ax25::kMaxInfoLen];
            size_t  info_len = pakt::aprs::encode_message(
                msg.dest_callsign, msg.dest_ssid,
                msg.text, msg.aprs_msg_id,
                info_buf, sizeof(info_buf));
            if (info_len == 0) {
                ESP_LOGW("aprs", "APRS encode_message failed (dest=%s)", msg.dest_callsign);
                return false;
            }

            // Build AX.25 UI frame (dest=APZPKT tocall, src=device callsign).
            const auto &cfg_vals = g_device_config.config();
            pakt::ax25::Frame frame = pakt::aprs::make_ui_frame(
                cfg_vals.callsign, cfg_vals.ssid);
            std::memcpy(frame.info, info_buf, info_len);
            frame.info_len = info_len;

            uint8_t ax25_buf[pakt::ax25::kMaxEncodedLen];
            size_t  ax25_len = pakt::ax25::encode(frame, ax25_buf, sizeof(ax25_buf));
            if (ax25_len == 0) {
                ESP_LOGW("aprs", "ax25::encode failed");
                return false;
            }

            ESP_LOGI("aprs", "APRS TX: %s→%s id=%s len=%u",
                     cfg_vals.callsign, msg.dest_callsign,
                     msg.aprs_msg_id, static_cast<unsigned>(ax25_len));
            return afsk_tx_frame(ax25_buf, ax25_len);
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

    // KISS raw TX: raw AX.25 bytes from BLE client → AFSK → I2S → SA818.
    // No APRS retry; KISS is a raw pipe (TNC mode 0).
    ctx.set_raw_tx_fn([](const uint8_t *ax25, size_t len) -> bool {
        ESP_LOGI("aprs", "KISS raw TX: %u AX.25 bytes → AFSK", static_cast<unsigned>(len));
        return afsk_tx_frame(ax25, len);
    });

    // Publish both pointers before entering the run loop.
    g_aprs_ctx     = &ctx;
    g_ptt_watchdog = &watchdog;

    ESP_LOGI("aprs", "task started (FW-016 watchdog armed, timeout=%" PRIu32 " ms)",
             pakt::PttWatchdog::kDefaultTimeoutMs);

    // ── APRS bench: prototype TX+RX packet validation ────────────────────────
    //
    // Runs once before the normal run loop.  No BLE client is needed.
    //
    // Stage A – TX burst:
    //   Waits for g_radio + g_i2s_tx_chan (SA818 bench runs first in radio_task).
    //   Sends kTxBurstCount APRS position frames via the real AFSK TX path.
    //   Payload: N0CALL>APZPKT:!0000.00N/00000.00W>PAKT bench N/3
    //   Watch for Bell 202 tones on a receiver tuned to 144.390 MHz FM.
    //
    // Stage B – RX ADC gain sweep:
    //   Opens squelch (SQ=0) and tries 4 ADC gain levels (0/+6/+12/+18 dB).
    //   Each pass is 30 s; logs peak, mean_abs, clip_count, flag-rate, FCS rejects.
    //   Operator: transmit APRS packets (Bell 202 AFSK) on 144.390 MHz FM.
    //
    // Frequency: 144.390 MHz FM — set by SA818 bench earlier in radio_task.
    if constexpr (pakt::benchcfg::kEnableAprsBench) {
        static constexpr uint32_t kPipelineWaitMaxMs  = 200000; // 200 s
        static constexpr uint32_t kPipelineWaitStepMs = 500;
        static constexpr int      kTxBurstCount       = 3;
        static constexpr uint32_t kTxInterpacketMs    = 3000;
        static constexpr float kStageCRecordGainDb =
            pakt::benchcfg::kAprsStageCRecordAdcGainStep * 1.5f;

        ESP_LOGI("aprs_bench", "");
        ESP_LOGI("aprs_bench", "############################################");
        ESP_LOGI("aprs_bench", "  APRS BENCH: TX+RX PACKET VALIDATION");
        ESP_LOGI("aprs_bench", "  Frequency : 144.390 MHz FM (APRS)");
        ESP_LOGI("aprs_bench", "  Callsign  : from device config (default N0CALL)");
        ESP_LOGI("aprs_bench", "  Profile   : loopback=%s tx=%s rx_sweep=%s pcm=%s record=%s",
                 pakt::benchcfg::kEnableAprsStage0Loopback ? "on" : "off",
                 pakt::benchcfg::kEnableAprsStageATxBurst ? "on" : "off",
                 pakt::benchcfg::kEnableAprsStageBRxGainSweep ? "on" : "off",
                 pakt::benchcfg::kEnableAprsStageBPcmSnapshot ? "on" : "off",
                 pakt::benchcfg::kEnableAprsStageCRxRecord ? "on" : "off");
        ESP_LOGI("aprs_bench", "############################################");

        do {  // break = skip remaining stages on fatal error
            // ── Wait for radio + audio pipeline ──────────────────────────────
            ESP_LOGI("aprs_bench", "  Waiting for SA818 radio + I2S TX "
                     "(SA818 bench still running in radio_task)...");
            uint32_t wait_ms = 0;
            while (wait_ms < kPipelineWaitMaxMs) {
                if (g_radio && g_i2s_tx_chan) break;
                vTaskDelay(pdMS_TO_TICKS(kPipelineWaitStepMs));
                wait_ms += kPipelineWaitStepMs;
                watchdog.heartbeat(
                    static_cast<uint32_t>(esp_timer_get_time() / 1000));
            }
            if (!g_radio || !g_i2s_tx_chan) {
                ESP_LOGE("aprs_bench", "ABORT: pipeline not ready after %lu ms.",
                         static_cast<unsigned long>(kPipelineWaitMaxMs));
                break;
            }
            ESP_LOGI("aprs_bench", "  Pipeline ready (waited %lu ms).",
                     static_cast<unsigned long>(wait_ms));

            // ── Stage 0: Modem software loopback ─────────────────────────────
            // Verify the AFSK modem pipeline entirely in software — no RF, no I2S.
            // Modulate a known AX.25 frame to mono PCM, then feed that PCM
            // directly into a local AfskDemodulator.  If the callback fires and
            // the FCS-validated frame comes out, the modem software is correct.
            // PASS here means any RF RX failures are hardware-only (level /
            // bandwidth / no signal on-air), not modem bugs.
            if constexpr (pakt::benchcfg::kEnableAprsStage0Loopback) {
                ESP_LOGI("aprs_bench", "");
                ESP_LOGI("aprs_bench", "--------------------------------------------");
                ESP_LOGI("aprs_bench", "  STAGE 0: MODEM LOOPBACK (software, no RF)");
                ESP_LOGI("aprs_bench", "--------------------------------------------");
                {
                    bool lb_pass = false;
                    uint8_t lb_info[pakt::ax25::kMaxInfoLen];
                    size_t  lb_info_len = pakt::aprs::encode_position(
                        0.0f, 0.0f, '/', '>', "loopback",
                        lb_info, sizeof(lb_info));

                    pakt::ax25::Frame lb_frame =
                        pakt::aprs::make_ui_frame(
                            g_device_config.config().callsign,
                            g_device_config.config().ssid);
                    std::memcpy(lb_frame.info, lb_info, lb_info_len);
                    lb_frame.info_len = lb_info_len;

                    uint8_t lb_ax25[pakt::ax25::kMaxEncodedLen];
                    size_t  lb_ax25_len =
                        pakt::ax25::encode(lb_frame, lb_ax25, sizeof(lb_ax25));

                    if (lb_ax25_len == 0) {
                        ESP_LOGE("aprs_bench",
                                 "Stage 0: ax25::encode failed — loopback skipped");
                    } else {
                        pakt::AfskModulator lb_mod(kAudioSampleRateHz);
                        size_t lb_pcm_len = lb_mod.modulate_frame(
                            lb_ax25, lb_ax25_len,
                            g_tx_pcm_buf, kAfskMaxPcmSamples);

                        if (lb_pcm_len == 0) {
                            ESP_LOGE("aprs_bench",
                                     "Stage 0: modulate_frame returned 0 — loopback skipped");
                        } else {
                            ESP_LOGI("aprs_bench",
                                     "Stage 0: %u bytes AX.25 → %u PCM samples → demod...",
                                     static_cast<unsigned>(lb_ax25_len),
                                     static_cast<unsigned>(lb_pcm_len));

                            bool lb_decoded = false;
                            pakt::AfskDemodulator lb_demod(kAudioSampleRateHz,
                                [&lb_decoded](const uint8_t *, size_t) {
                                    lb_decoded = true;
                                });
                            lb_demod.process(g_tx_pcm_buf, lb_pcm_len);

                            if (lb_decoded) {
                                ESP_LOGI("aprs_bench",
                                         "Stage 0: PASS — modem loopback decoded frame OK");
                                lb_pass = true;
                            } else {
                                ESP_LOGE("aprs_bench",
                                         "Stage 0: FAIL — demodulator decoded 0 frames");
                                ESP_LOGE("aprs_bench",
                                         "  Modem software bug — RF RX results unreliable");
                            }
                        }
                    }
                    (void)lb_pass;
                }
            } else {
                ESP_LOGI("aprs_bench", "  STAGE 0 skipped by bench profile.");
            }

            // ── Stage A: TX burst ─────────────────────────────────────────────
            int tx_ok = 0, tx_fail = 0;
            if constexpr (pakt::benchcfg::kEnableAprsStageATxBurst) {
                ESP_LOGI("aprs_bench", "");
                ESP_LOGI("aprs_bench", "--------------------------------------------");
                ESP_LOGI("aprs_bench", "  STAGE A: TX BURST — %d APRS packets", kTxBurstCount);
                ESP_LOGI("aprs_bench", "  >>> ACTION: Monitor a receiver on 144.390 MHz FM.");
                ESP_LOGI("aprs_bench", "  >>> You should hear Bell 202 AFSK tones per packet.");
                ESP_LOGI("aprs_bench", "  >>> APRS TNC/app will decode N0CALL>APZPKT frames.");
                ESP_LOGI("aprs_bench", "--------------------------------------------");
                vTaskDelay(pdMS_TO_TICKS(2000));
                watchdog.heartbeat(static_cast<uint32_t>(esp_timer_get_time() / 1000));

                for (int i = 1; i <= kTxBurstCount; ++i) {
                    watchdog.heartbeat(
                        static_cast<uint32_t>(esp_timer_get_time() / 1000));

                    const auto &cfg = g_device_config.config();

                    char comment[48];
                    snprintf(comment, sizeof(comment), "PAKT bench %d/%d", i, kTxBurstCount);

                    uint8_t info_buf[pakt::ax25::kMaxInfoLen];
                    size_t info_len = pakt::aprs::encode_position(
                        0.0f, 0.0f, '/', '>', comment,
                        info_buf, sizeof(info_buf));

                    if (info_len == 0) {
                        ESP_LOGE("aprs_bench", "[%d/%d] encode_position failed", i, kTxBurstCount);
                        ++tx_fail;
                        continue;
                    }

                    pakt::ax25::Frame frame =
                        pakt::aprs::make_ui_frame(cfg.callsign, cfg.ssid);
                    std::memcpy(frame.info, info_buf, info_len);
                    frame.info_len = info_len;

                    uint8_t ax25_buf[pakt::ax25::kMaxEncodedLen];
                    size_t ax25_len = pakt::ax25::encode(frame, ax25_buf, sizeof(ax25_buf));
                    if (ax25_len == 0) {
                        ESP_LOGE("aprs_bench", "[%d/%d] ax25::encode failed", i, kTxBurstCount);
                        ++tx_fail;
                        continue;
                    }

                    pakt::ax25::Frame dbg_f;
                    char tnc2[256] = "<format unavail>";
                    if (pakt::ax25::decode(ax25_buf, ax25_len, dbg_f)) {
                        pakt::ax25::to_tnc2(dbg_f, tnc2, sizeof(tnc2));
                    }
                    ESP_LOGI("aprs_bench", "[%d/%d] TX: %s  (%u bytes AX.25)",
                             i, kTxBurstCount, tnc2, static_cast<unsigned>(ax25_len));

                    bool ok = afsk_tx_frame(ax25_buf, ax25_len);
                    if (ok) {
                        ++tx_ok;
                        ESP_LOGI("aprs_bench", "[%d/%d] TX done: afsk_tx ok", i, kTxBurstCount);
                    } else {
                        ++tx_fail;
                        ESP_LOGE("aprs_bench", "[%d/%d] TX FAIL: afsk_tx returned false",
                                 i, kTxBurstCount);
                    }

                    if (i < kTxBurstCount) {
                        for (uint32_t d = 0; d < kTxInterpacketMs; d += 500) {
                            vTaskDelay(pdMS_TO_TICKS(500));
                            watchdog.heartbeat(
                                static_cast<uint32_t>(esp_timer_get_time() / 1000));
                        }
                    }
                }
                ESP_LOGI("aprs_bench", "TX burst: %d ok  %d fail  (of %d)",
                         tx_ok, tx_fail, kTxBurstCount);
            } else {
                ESP_LOGI("aprs_bench", "  STAGE A skipped by bench profile.");
            }

            // ── Stage B: ADC gain sweep RX test ──────────────────────────────
            //
            // 4 passes × 30 s = 120 s total.
            // Each pass sets a different SGTL5000 ADC gain, then listens for
            // APRS packets.  Per-second log includes peak, mean_abs, clip_count,
            // flag rate, FCS rejects, and decoded count.
            //
            // Gain steps (CHIP_ANA_ADC_CTRL, 1.5 dB/step):
            //   Pass 1: 0  →  0.0 dB (baseline)
            //   Pass 2: 4  →  +6.0 dB
            //   Pass 3: 8  → +12.0 dB
            //   Pass 4: 12 → +18.0 dB
            //
            // Diagnostics interpretation:
            //   peak     > 500          → audio energy from SA818 AF_OUT
            //   mean_abs ~ 5000-15000   → healthy demod input range
            //   mean_abs < 500          → signal too weak; increase gain or check wiring
            //   clip_count > 100/s      → ADC saturating; gain too high or signal too loud
            //   dt >= 40                → AFSK burst; demod is locking on Bell 202 preamble
            //   fcs_rej > 0             → frames assembling but CRC bad (wrong level / distortion)
            //   decoded > 0             → PASS

            static constexpr uint8_t  kGainSteps[]     = {0, 4, 8, 12};
            static constexpr int      kNumPasses        = 4;
            static constexpr int32_t  kPassWindowMs     = 30000; // 30 s per pass
            static constexpr uint32_t kAfskBurstThresh  = 40;

            int total_decoded = 0;
            if constexpr (pakt::benchcfg::kEnableAprsStageBRxGainSweep) {
                ESP_LOGI("aprs_bench", "");
                ESP_LOGI("aprs_bench", "############################################");
                ESP_LOGI("aprs_bench", "  STAGE B: APRS RX — ADC GAIN SWEEP");
                ESP_LOGI("aprs_bench", "  4 passes × 30 s  (0 / +6 / +12 / +18 dB ADC gain)");
                ESP_LOGI("aprs_bench", "  Frequency : 144.390 MHz FM, no CTCSS.");
                ESP_LOGI("aprs_bench", "  Source    : APRSdroid, TNC, or any Bell 202 APRS TX.");
                ESP_LOGI("aprs_bench", "  Send 2-3 APRS packets per pass (every 10-15 s).");
                ESP_LOGI("aprs_bench", "  APRS data mode ONLY — do NOT transmit voice.");
                ESP_LOGI("aprs_bench", "############################################");
                ESP_LOGI("aprs_bench", "");
                ESP_LOGI("aprs_bench", "  Column key:");
                ESP_LOGI("aprs_bench", "    peak    : max |sample| this second (0-32767)");
                ESP_LOGI("aprs_bench", "    mean    : avg |sample| (< 500 = too quiet; > 20000 = near clipping)");
                ESP_LOGI("aprs_bench", "    clip    : samples/s above 85%% FS  (> 100 = saturation)");
                ESP_LOGI("aprs_bench", "    dt      : HDLC flags/s (noise baseline ~24; APRS burst > 40)");
                ESP_LOGI("aprs_bench", "    fcs_rej : frames assembled but CRC failed");
                ESP_LOGI("aprs_bench", "    dec     : valid decoded frames (goal > 0)");
                ESP_LOGI("aprs_bench", "");

                if (g_radio->set_squelch(0)) {
                    ESP_LOGI("aprs_bench", "  Squelch opened (SQ=0) for all passes.");
                } else {
                    ESP_LOGW("aprs_bench", "  set_squelch(0) failed; continuing with default SQ.");
                }

                // PCM snapshot will be armed at the start of each pass while not yet captured.
                for (int pass = 0; pass < kNumPasses; ++pass) {
                const uint8_t gain_step = kGainSteps[pass];
                const float   gain_db   = gain_step * 1.5f;

                // Request gain change; audio_task applies it in next per-second tick.
                g_adc_gain_req.store(gain_step, std::memory_order_relaxed);

                // (Re-)arm PCM capture at the start of every pass so that any
                // signal event in this pass overwrites an earlier noise capture.
                g_pcm_cap_valid.store(false, std::memory_order_relaxed);
                g_pcm_cap_arm.store(true, std::memory_order_relaxed);

                ESP_LOGI("aprs_bench", "");
                ESP_LOGI("aprs_bench", "--- PASS %d/%d: ADC gain = +%.1f dB (step=%u) ---",
                         pass + 1, kNumPasses, gain_db, gain_step);
                ESP_LOGI("aprs_bench", "  Waiting 2 s for gain to take effect...");
                vTaskDelay(pdMS_TO_TICKS(2000));
                watchdog.heartbeat(
                    static_cast<uint32_t>(esp_timer_get_time() / 1000));

                ESP_LOGI("aprs_bench", "");
                ESP_LOGI("aprs_bench",
                         "  >>> SEND 2-3 APRS PACKETS NOW on 144.390 MHz FM.");
                ESP_LOGI("aprs_bench",
                         "  >>> Bell 202 AFSK ONLY — not voice, not carrier.");
                ESP_LOGI("aprs_bench",
                         "  >>> Packet every 10-15 s during this 30 s window.");
                ESP_LOGI("aprs_bench", "");

                // Snapshot counters at pass start for deltas.
                const uint32_t flags_base   =
                    g_demod_flags.load(std::memory_order_relaxed);
                const uint32_t fcs_base     =
                    g_demod_fcs_rejects.load(std::memory_order_relaxed);
                uint32_t prev_flags_abs     = flags_base;
                int      pass_decoded       = 0;
                int      elapsed_s          = 0;
                const int64_t pass_end      =
                    esp_timer_get_time() + static_cast<int64_t>(kPassWindowMs) * 1000LL;

                while (esp_timer_get_time() < pass_end) {
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    ++elapsed_s;
                    watchdog.heartbeat(
                        static_cast<uint32_t>(esp_timer_get_time() / 1000));

                    // Drain decoded AX.25 frames from audio_task.
                    {
                        uint8_t ax25_frame[pakt::kKissMaxFrame];
                        size_t  ax25_len = 0;
                        while (g_rx_ax25_queue.pop(ax25_frame, &ax25_len)) {
                            ++pass_decoded;
                            ++total_decoded;
                            pakt::ax25::Frame f;
                            char tnc2_rx[256] = "<decode failed>";
                            if (pakt::ax25::decode(ax25_frame, ax25_len, f)) {
                                pakt::ax25::to_tnc2(f, tnc2_rx, sizeof(tnc2_rx));
                            }
                            ESP_LOGI("aprs_bench",
                                     "  *** [RX #%d pass=%d t=%ds gain=+%.1fdB] %s ***",
                                     total_decoded, pass + 1, elapsed_s,
                                     gain_db, tnc2_rx);
                        }
                    }

                    // Per-second diagnostics.
                    const int32_t  peak      = g_rx_peak_abs.load(std::memory_order_relaxed);
                    const uint32_t mean_abs  = g_rx_mean_abs.load(std::memory_order_relaxed);
                    const uint32_t clips     = g_rx_clip_count.load(std::memory_order_relaxed);
                    const uint32_t flags_abs = g_demod_flags.load(std::memory_order_relaxed);
                    const uint32_t fcs_abs   = g_demod_fcs_rejects.load(std::memory_order_relaxed);
                    const uint32_t flag_dt   = flags_abs - prev_flags_abs;
                    const uint32_t fcs_rej   = fcs_abs - fcs_base;
                    prev_flags_abs = flags_abs;

                    ESP_LOGI("aprs_bench",
                             "  [%2ds/30s] peak=%-6" PRId32
                             " mean=%-5" PRIu32
                             " clip=%-4" PRIu32
                             " dt=%-3" PRIu32
                             " fcs_rej=%-3" PRIu32
                             " dec=%d",
                             elapsed_s, peak, mean_abs, clips,
                             flag_dt, fcs_rej, pass_decoded);

                    if (flag_dt >= kAfskBurstThresh) {
                        ESP_LOGW("aprs_bench",
                                 "  !!! AFSK BURST: dt=%" PRIu32
                                 " flags/s at t=%ds gain=+%.1f dB !!!",
                                 flag_dt, elapsed_s, gain_db);
                    }
                    if (clips > 100) {
                        ESP_LOGW("aprs_bench",
                                 "  *** CLIPPING: %" PRIu32
                                 " samples/s > 85%% FS — gain too high ***",
                                 clips);
                    }

                    // Re-arm PCM capture whenever signal is present this second
                    // but we don't yet have a valid high-signal snapshot.
                    // This catches signal events that started after pass-start arm was consumed.
                    if (peak > 5000 &&
                        !g_pcm_cap_valid.load(std::memory_order_relaxed) &&
                        !g_pcm_cap_arm.load(std::memory_order_relaxed)) {
                        g_pcm_cap_arm.store(true, std::memory_order_relaxed);
                    }
                } // while pass window

                const uint32_t pass_flags   =
                    g_demod_flags.load(std::memory_order_relaxed) - flags_base;
                const uint32_t pass_fcs_rej =
                    g_demod_fcs_rejects.load(std::memory_order_relaxed) - fcs_base;
                ESP_LOGI("aprs_bench",
                         "  PASS %d SUMMARY: +%.1f dB  decoded=%d  flags=%" PRIu32
                         "  fcs_rej=%" PRIu32,
                         pass + 1, gain_db, pass_decoded,
                         pass_flags, pass_fcs_rej);

                if (total_decoded > 0) {
                    ESP_LOGI("aprs_bench", "  >>> APRS RX PASS — stopping gain sweep early.");
                    break;
                }
                } // for pass

                g_radio->set_squelch(1);
                g_adc_gain_req.store(0, std::memory_order_relaxed);
                ESP_LOGI("aprs_bench", "  Squelch restored (SQ=1). ADC gain reset to 0 dB.");
            } else {
                ESP_LOGI("aprs_bench", "  STAGE B skipped by bench profile.");
            }

            // ── PCM snapshot dump ─────────────────────────────────────────────
            if constexpr (pakt::benchcfg::kEnableAprsStageBRxGainSweep &&
                          pakt::benchcfg::kEnableAprsStageBPcmSnapshot) {
                // Log captured raw PCM for offline waveform inspection.
                // If no Bell 202 arrived, the capture still reveals noise or carrier shape.
                if (g_pcm_cap_valid.load(std::memory_order_acquire)) {
                // Goertzel spectral analysis: quantify Bell 202 tone energy in the capture.
                // A clean 1200 Hz mark tone gives mark_pwr >> space_pwr; vice versa for space.
                // If neither is >> the other and both are near noise, signal is not AFSK.
                const float mark_pwr  = goertzel_power(
                    g_pcm_cap, kPcmCapLen, 1200.0f, static_cast<float>(kAudioSampleRateHz));
                const float space_pwr = goertzel_power(
                    g_pcm_cap, kPcmCapLen, 2200.0f, static_cast<float>(kAudioSampleRateHz));
                const float noise_pwr = goertzel_power(
                    g_pcm_cap, kPcmCapLen,  900.0f, static_cast<float>(kAudioSampleRateHz));

                ESP_LOGI("aprs_bench", "");
                ESP_LOGI("aprs_bench",
                         "--- PCM SNAPSHOT: %u samples @ %u Hz = ~128 ms ---",
                         static_cast<unsigned>(kPcmCapLen),
                         static_cast<unsigned>(kAudioSampleRateHz));
                ESP_LOGI("aprs_bench",
                         "  Tone analysis (Goertzel, normalised):");
                ESP_LOGI("aprs_bench",
                         "    1200 Hz (mark)  power = %.6f%s",
                         mark_pwr,  mark_pwr  > 0.001f ? "  << MARK TONE PRESENT"  : "");
                ESP_LOGI("aprs_bench",
                         "    2200 Hz (space) power = %.6f%s",
                         space_pwr, space_pwr > 0.001f ? "  << SPACE TONE PRESENT" : "");
                ESP_LOGI("aprs_bench",
                         "    900 Hz  (noise) power = %.6f  (reference)",
                         noise_pwr);
                if (mark_pwr > 0.001f || space_pwr > 0.001f) {
                    ESP_LOGI("aprs_bench",
                             "  >>> AFSK TONES DETECTED in capture — demod input OK.");
                } else {
                    ESP_LOGW("aprs_bench",
                             "  >>> NO AFSK TONES in capture — signal is NOT Bell 202.");
                    ESP_LOGW("aprs_bench",
                             "      Check: APRS source is in AFSK/audio mode (not APRS-IS).");
                    ESP_LOGW("aprs_bench",
                             "      Check: HT VOX or PTT cable actually modulating RF.");
                }
                ESP_LOGI("aprs_bench",
                         "    (signed int16, selected mono path, %u Hz — copy for offline analysis)",
                         static_cast<unsigned>(kAudioSampleRateHz));
                // 16 samples per line.
                char line[160];
                for (size_t i = 0; i < kPcmCapLen; i += 16) {
                    int pos = 0;
                    for (size_t j = 0; j < 16 && (i + j) < kPcmCapLen; ++j) {
                        pos += snprintf(line + pos, sizeof(line) - (size_t)pos,
                                        "%d,",
                                        static_cast<int>(g_pcm_cap[i + j]));
                    }
                    if (pos > 0 && line[pos - 1] == ',') line[pos - 1] = '\0';
                    ESP_LOGI("aprs_bench", "  %s", line);
                }
                ESP_LOGI("aprs_bench", "--- END PCM SNAPSHOT ---");
                } else {
                    ESP_LOGW("aprs_bench",
                             "  PCM snapshot not captured (no signal during capture window).");
                }
            } else if constexpr (pakt::benchcfg::kEnableAprsStageBPcmSnapshot) {
                ESP_LOGI("aprs_bench",
                         "  PCM snapshot skipped because Stage B RX sweep is disabled.");
            }

            // ── Full 30 s RX recorder dump ──────────────────────────────────
            if constexpr (pakt::benchcfg::kEnableAprsStageCRxRecord) {
                if (ensure_rx_record_buffer()) {
                ESP_LOGI("aprs_bench", "");
                ESP_LOGI("aprs_bench", "############################################");
                ESP_LOGI("aprs_bench", "  STAGE C: FULL RX RECORDING — %u s",
                         static_cast<unsigned>(kRxRecordSeconds));
                ESP_LOGI("aprs_bench", "############################################");
                ESP_LOGI("aprs_bench", "  Captures mono PCM at the actual demod input.");
                ESP_LOGI("aprs_bench", "  ADC gain   : +%.1f dB (step=%u)",
                         kStageCRecordGainDb,
                         static_cast<unsigned>(pakt::benchcfg::kAprsStageCRecordAdcGainStep));
                ESP_LOGI("aprs_bench", "  Operator: send APRS packets during this 30 s window.");
                ESP_LOGI("aprs_bench", "  Export: serial will emit a base64 WAV after capture.");

                g_radio->set_squelch(0);
                g_adc_gain_req.store(pakt::benchcfg::kAprsStageCRecordAdcGainStep,
                                     std::memory_order_relaxed);
                vTaskDelay(pdMS_TO_TICKS(2000));
                watchdog.heartbeat(
                    static_cast<uint32_t>(esp_timer_get_time() / 1000));
                g_rx_record_valid.store(false, std::memory_order_relaxed);
                g_rx_record_samples.store(0, std::memory_order_relaxed);
                g_rx_record_arm.store(true, std::memory_order_release);

                for (uint32_t s = 0; s < kRxRecordSeconds; ++s) {
                    if (s == 0) {
                        ESP_LOGI("aprs_bench",
                                 "  >>> SEND APRS NOW — 3-5 packets across this window.");
                    } else if (s % 10 == 0) {
                        ESP_LOGI("aprs_bench",
                                 "  >>> Continue sending APRS packets (%u/%u s).",
                                 static_cast<unsigned>(s),
                                 static_cast<unsigned>(kRxRecordSeconds));
                    }
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    watchdog.heartbeat(
                        static_cast<uint32_t>(esp_timer_get_time() / 1000));

                    const size_t samples =
                        g_rx_record_samples.load(std::memory_order_relaxed);
                    const int32_t peak =
                        g_rx_peak_abs.load(std::memory_order_relaxed);
                    const uint32_t mean_abs =
                        g_rx_mean_abs.load(std::memory_order_relaxed);
                    ESP_LOGI("aprs_bench",
                             "  [rec %2us/%us] samples=%-6u peak=%-6" PRId32
                             " mean=%-5" PRIu32 " active=%s",
                             static_cast<unsigned>(s + 1),
                             static_cast<unsigned>(kRxRecordSeconds),
                             static_cast<unsigned>(samples),
                             peak, mean_abs,
                             g_rx_record_active.load(std::memory_order_acquire) ? "yes" : "no");
                }

                g_radio->set_squelch(1);
                if (g_rx_record_active.load(std::memory_order_acquire)) {
                    ESP_LOGW("aprs_bench",
                             "  Recorder still active after nominal window; waiting briefly...");
                    while (g_rx_record_active.load(std::memory_order_acquire)) {
                        vTaskDelay(pdMS_TO_TICKS(250));
                    }
                }

                if (g_rx_record_valid.load(std::memory_order_acquire)) {
                    dump_rx_record_wav_base64(&watchdog);
                } else {
                    ESP_LOGW("aprs_bench",
                             "  Full RX recording unavailable — capture did not complete.");
                }
                g_radio->set_squelch(1);
                g_adc_gain_req.store(0, std::memory_order_relaxed);
                } else {
                    ESP_LOGW("aprs_bench", "  Full RX recording unavailable — recorder buffer alloc failed.");
                }
            } else {
                ESP_LOGI("aprs_bench", "  STAGE C skipped by bench profile.");
            }

            // ── Final summary + diagnosis ─────────────────────────────────────
            ESP_LOGI("aprs_bench", "");
            ESP_LOGI("aprs_bench", "############################################");
            ESP_LOGI("aprs_bench", "  APRS BENCH RESULT — GAIN SWEEP");
            if constexpr (pakt::benchcfg::kEnableAprsStageATxBurst) {
                if (tx_ok == kTxBurstCount) {
                    ESP_LOGI("aprs_bench", "  TX path: PASS (%d/%d)", tx_ok, kTxBurstCount);
                } else {
                    ESP_LOGE("aprs_bench", "  TX path: FAIL (%d/%d)", tx_ok, kTxBurstCount);
                }
            } else {
                ESP_LOGI("aprs_bench", "  TX path: SKIP");
            }
            if constexpr (pakt::benchcfg::kEnableAprsStageBRxGainSweep) {
                if (total_decoded > 0) {
                    ESP_LOGI("aprs_bench",
                             "  RX path: PASS  (%d frame(s) decoded)", total_decoded);
                } else {
                    ESP_LOGW("aprs_bench", "  RX path: FAIL  (0 frames decoded across all passes)");
                    ESP_LOGW("aprs_bench", "");
                    ESP_LOGW("aprs_bench", "  Diagnosis guide:");
                    ESP_LOGW("aprs_bench", "  peak=0 across ALL passes:");
                    ESP_LOGW("aprs_bench", "    → SA818 AF_OUT not reaching SGTL5000 LINE_IN.");
                    ESP_LOGW("aprs_bench", "    → Check AC coupling cap on AF_RX_COUPLED net.");
                    ESP_LOGW("aprs_bench", "    → Check SA818 AF_OUT wiring to PJRC LINE_IN header.");
                    ESP_LOGW("aprs_bench", "  peak > 500 but mean_abs < 200:");
                    ESP_LOGW("aprs_bench", "    → Signal present but too weak even at max gain.");
                    ESP_LOGW("aprs_bench", "    → Increase SA818 AF_OUT (AT+DMOSETVOLUME=8).");
                    ESP_LOGW("aprs_bench", "    → Check RC LPF cutoff on AF_RX path (not too low).");
                    ESP_LOGW("aprs_bench", "  clip_count high (> 100/s) in every pass:");
                    ESP_LOGW("aprs_bench", "    → ADC saturating at all gain levels.");
                    ESP_LOGW("aprs_bench", "    → SA818 AF_OUT too loud; add series resistor.");
                    ESP_LOGW("aprs_bench", "  mean_abs healthy (500-15000) but dt never > 40:");
                    ESP_LOGW("aprs_bench", "    → Signal level OK but NOT Bell 202 AFSK.");
                    ESP_LOGW("aprs_bench", "    → Verify APRS source is sending data packets,");
                    ESP_LOGW("aprs_bench", "      not voice.  Check APRSdroid Audio mode setting.");
                    ESP_LOGW("aprs_bench", "  dt > 40 but fcs_rej > 0 and decoded = 0:");
                    ESP_LOGW("aprs_bench", "    → AFSK preamble seen but frame data corrupted.");
                    ESP_LOGW("aprs_bench", "    → Check SA818 RX deviation / bandwidth.");
                    ESP_LOGW("aprs_bench", "    → Check AC coupling cap value (should be 1 uF).");
                    ESP_LOGW("aprs_bench", "    → Review PCM snapshot above for distortion.");
                }
            } else {
                ESP_LOGI("aprs_bench", "  RX path: SKIP");
            }
            ESP_LOGI("aprs_bench", "  Normal APRS pipeline now active.");
            ESP_LOGI("aprs_bench", "############################################");
            ESP_LOGI("aprs_bench", "");
        } while (false);
    } else {
        ESP_LOGI("aprs_bench", "APRS bench disabled by bench_profile_config.h");
    }
    // ── End APRS bench ────────────────────────────────────────────────────────

    for (;;) {
        const uint32_t now_ms =
            static_cast<uint32_t>(esp_timer_get_time() / 1000);
        ctx.tick(now_ms);
        watchdog.heartbeat(now_ms);   // signal aprs_task is alive

        // Drain decoded AX.25 frames pushed by audio_task and forward to BLE clients.
        // Producer call site: audio_task's TODO comment above (FW-004 + FW-006).
        // Until the audio pipeline is wired this queue is always empty (safe no-op).
        {
            uint8_t ax25_frame[pakt::kKissMaxFrame];
            size_t  ax25_len = 0;
            while (g_rx_ax25_queue.pop(ax25_frame, &ax25_len)) {
                // Log every decoded packet as TNC2 for serial bench monitoring.
                {
                    pakt::ax25::Frame rx_frame;
                    char tnc2_log[256] = "<decode failed>";
                    if (pakt::ax25::decode(ax25_frame, ax25_len, rx_frame)) {
                        pakt::ax25::to_tnc2(rx_frame, tnc2_log, sizeof(tnc2_log));
                    }
                    ESP_LOGI("aprs", "RX packet: %s  (%u bytes)",
                             tnc2_log, static_cast<unsigned>(ax25_len));
                }

                // 1. Native rx_packet notify (PAKT BLE clients, e.g. desktop app)
                pakt::BleServer::instance().notify_rx_packet(ax25_frame, ax25_len);

                // 2. KISS RX notify (KISS TNC clients, e.g. APRSdroid / Xastir)
                //    KissFramer::encode adds FEND delimiters and byte-escaping.
                //    notify_kiss_rx applies INT-002 chunking (spec §3.2).
                uint8_t kiss_buf[pakt::kKissMaxFrame * 2 + 4];
                int kiss_len = pakt::KissFramer::encode(
                    ax25_frame, ax25_len, kiss_buf, sizeof(kiss_buf));
                if (kiss_len > 0) {
                    pakt::BleServer::instance().notify_kiss_rx(
                        kiss_buf, static_cast<size_t>(kiss_len));
                }
            }
        }

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
            ESP_LOGW("gps", "GPS fix stale (>%lu ms)",
                     static_cast<unsigned long>(kStaleMs));
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
    handlers.on_kiss_tx = [](const uint8_t *data, size_t len) -> bool {
        // Decode the inbound KISS frame into raw AX.25 bytes.
        uint8_t ax25_buf[pakt::kKissMaxFrame];
        uint8_t cmd = 0;
        int ax25_len = pakt::KissFramer::decode(
            data, len, ax25_buf, sizeof(ax25_buf), &cmd);
        if (ax25_len < 0) {
            ESP_LOGW("ble", "KISS TX: malformed frame (len=%u)", (unsigned)len);
            return false;
        }
        if (ax25_len == 0) {
            // Non-data frame (e.g. 0x0F return-from-KISS): valid, no-op in MVP.
            ESP_LOGI("ble", "KISS TX: non-data frame cmd=0x%02X (no-op)", cmd);
            return true;
        }
        // Data frame: push raw AX.25 to the shared TX ring consumed by aprs_task.
        // aprs_task drains via set_raw_tx_fn (stub → real AFSK pipeline on hardware).
        if (!g_aprs_ctx) {
            ESP_LOGW("ble", "KISS TX: aprs_task not ready, dropping %d-byte frame", ax25_len);
            return false;
        }
        if (!g_aprs_ctx->push_kiss_ax25(ax25_buf, static_cast<size_t>(ax25_len))) {
            ESP_LOGW("ble", "KISS TX: TX queue full, dropping %d-byte frame", ax25_len);
            return false;
        }
        ESP_LOGD("ble", "KISS TX: enqueued %d AX.25 bytes", ax25_len);
        return true;
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
