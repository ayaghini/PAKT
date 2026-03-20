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

#include "audio_bench_test.h"
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
#include <atomic>
#include <cinttypes>
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

// PCM output buffer for AFSK modulation (global to avoid stack pressure).
// Max AX.25 = 330 B → ~22 400 samples at 8 kHz/1200 baud; 25 600 adds margin.
static constexpr size_t kAfskMaxPcmSamples = 20000;
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

    // Run staged SA818 bench test before normal init.
    // Pass &g_i2s_tx_chan so Stage 6 (TX audio) can wait for audio_task to publish the handle.
    pakt::bench::run_sa818_bench(transport, kPttGpio, kUartPort,
                                 static_cast<const volatile void *>(&g_i2s_tx_chan),
                                 8000);

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

// audio_write_square_wave and audio_bench_self_test removed.
// Bench test logic lives in audio_bench_test/audio_bench_test.cpp.

// Power-up sequence for both ADC (RX: SA818 AF_OUT → line-in → I2S)
// and DAC (TX: I2S → DAC → line-out → SA818 AF_IN) paths at 8 kHz.
//
// Clock plan (consistent with I2S master MCLK_MULTIPLE_1024 @ 8 kHz):
//   MCLK = 1024 × 8000 = 8.192 MHz delivered by ESP32-S3 I2S master.
//   CHIP_CLK_CTRL: SYS_FS=32 kHz (MCLK÷256=32 kHz), RATE_MODE=÷4 → 8 kHz ADC/DAC.
//   CHIP_CLK_CTRL = (RATE_MODE=2 @ bits[5:4]) | (SYS_FS=0 @ bits[3:2]) | (MCLK_FREQ=0 @ bits[1:0])
//                 = (2<<4)|(0<<2)|(0<<0) = 0x0020
//
// Gain calibration (ADC input level and DAC output level) is a hardware bring-up task.
// CHIP_ANA_POWER bit layout assumed from SGTL5000 datasheet §6.5:
//   bit 14: VCOAMP_POWERUP  bit 13: VAG_POWERUP  bit 12: ADC_MONO
//   bit 11: REFTOP_POWERUP  bit 9: DAC_POWERUP   bit 7: ADC_POWERUP
//   bit 6: LINEOUT_POWERUP  bit 5: LINEOUT_MONO
// Verify every bit against datasheet before first power-on.
static bool sgtl5000_init(i2c_master_dev_handle_t dev)
{
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

    // 7. Clock: SYS_FS=32 kHz (bits[3:2]=0), RATE_MODE=÷4 (bits[5:4]=2),
    //    MCLK_FREQ=256×SYS_FS (bits[1:0]=0) → effective rate = 32000/4 = 8000 Hz.
    //    MCLK input = 256 × 32000 = 8.192 MHz, matching I2S MCLK_MULTIPLE_1024 × 8 kHz.
    //    CHIP_CLK_CTRL = (2<<4)|(0<<2)|(0<<0) = 0x0020
    if (sgtl5000_write(dev, 0x0004, 0x0020) != ESP_OK) return false;

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

    ESP_LOGI("audio", "SGTL5000 init OK: 8 kHz, I2S→DAP→DAC→HP, ADC=LINE_IN, MCLK=8.192 MHz");
    return true;
}

// ── Audio pipeline ────────────────────────────────────────────────────────────
//
// Called once by audio_task. Returns only on unrecoverable init failure.
// On success, enters the per-sample run loop and never returns.

static void audio_pipeline_run()
{
    static constexpr uint32_t kSampleRateHz  = 8000;
    static constexpr size_t   kDmaFrames     = 256;   // frames per DMA block

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
    // MCLK plan: MCLK_MULTIPLE_1024 × 8000 Hz = 8.192 MHz.
    //   SGTL5000 CHIP_CLK_CTRL uses 256 × SYS_FS (32 kHz) = 8.192 MHz → ADC/DAC = 8 kHz.
    //   BCLK = 8000 × 2 ch × 16 bit = 256 kHz; MCLK/BCLK = 32 (integer, no jitter).
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
        // MCLK = 1024 × 8 kHz = 8.192 MHz — matches SGTL5000 CLK_CTRL (SYS_FS=32kHz, 256×).
        cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_1024;
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

    if (!sgtl5000_init(sgtl_dev)) {
        ESP_LOGE("audio", "SGTL5000 init failed – check I2C wiring");
        return;
    }

    // Run human-operated bench test (HP output tones + LINE_IN input monitor).
    // The PJRC HP jack is TRS output-only; MIC is a separate header (not tested here).
    pakt::bench::run_audio_bench(tx_chan, rx_chan, kSampleRateHz);

    // Publish TX handle so aprs_task can call afsk_tx_frame().
    g_i2s_tx_chan = tx_chan;

    // ── AfskDemodulator + sample loop ─────────────────────────────────────────
    // The demodulator callback runs in this task context (no RTOS crossing needed).
    pakt::AfskDemodulator demod(kSampleRateHz,
        [](const uint8_t *frame, size_t len) {
            if (!g_rx_ax25_queue.push(frame, len)) {
                ESP_LOGW("audio", "RX AX.25 queue full – frame dropped");
            }
        });

    static int16_t rx_buf[kDmaFrames * 2];
    static int16_t mono_buf[kDmaFrames];

    ESP_LOGI("audio", "RX pipeline running: SGTL5000 + AfskDemodulator @ %u Hz",
             static_cast<unsigned>(kSampleRateHz));

    for (;;) {
        size_t bytes_read = 0;
        esp_err_t err = i2s_channel_read(rx_chan, rx_buf, sizeof(rx_buf),
                                         &bytes_read, pdMS_TO_TICKS(50));
        if (err == ESP_OK && bytes_read > 0) {
            const size_t stereo_samples = bytes_read / sizeof(int16_t);
            const size_t frames = stereo_samples / 2;
            for (size_t i = 0; i < frames; ++i) {
                mono_buf[i] = rx_buf[i * 2];
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

    pakt::AfskModulator mod(8000);
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

    const uint32_t timeout_ms = static_cast<uint32_t>(n_samples * 1000u / 8000u) + 500u;
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

    // Wait for DMA to drain (4 descs × 256 frames / 8000 Hz ≈ 128 ms).
    vTaskDelay(pdMS_TO_TICKS(150));

    g_radio->ptt(false);

    bool ok = (err == ESP_OK) && (bytes_written == n_samples * sizeof(int16_t));
    ESP_LOGI("aprs", "afsk_tx: %s (%u samples, %u ms)",
             ok ? "ok" : "FAIL",
             static_cast<unsigned>(n_samples),
             static_cast<unsigned>(n_samples * 1000u / 8000u));
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
