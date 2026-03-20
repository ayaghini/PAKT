// audio_bench_test.cpp — Human-operated bench test for PJRC Teensy Audio Adapter Rev D
//
// ── Hardware context ──────────────────────────────────────────────────────────
// Board stack:  Adafruit ESP32-S3 Feather  +  PJRC Teensy Audio Adapter Rev D
// Codec:        SGTL5000, I2C addr 0x0A (only visible after MCLK is running)
//
// GPIO wiring (verified):
//   I2C SDA=GPIO3  SCL=GPIO4
//   MCLK=GPIO14  BCLK=GPIO8  WS=GPIO15  DOUT=GPIO12  DIN=GPIO10
//
// ── Signal paths ──────────────────────────────────────────────────────────────
// Output (Phase 1):
//   ESP32-S3 I2S TX → SGTL5000 I2S_IN → DAP (pass-through) → DAC → HP amp
//   → 2.2 µF DC-blocking caps → 3.5 mm TRS jack (tip=L, ring=R, sleeve=GND)
//   Left  channel = even I2S slots (I2S Philips, 16-bit stereo @ 8 kHz)
//   Right channel = odd  I2S slots
//   The 3.5 mm jack is a TRS OUTPUT jack only (no microphone on this connector).
//
// Input (Phase 2):
//   LINE_IN 3-pin header (L / R / GND) on PJRC board → SGTL5000 ADC
//   → SGTL5000 I2S_OUT → ESP32-S3 I2S RX
//   ADC source = LINE_IN is the only path exercised here.
//   (MIC_IN is a separate header on the PJRC board, not tested in this module.)
//
// ── Test sequence ─────────────────────────────────────────────────────────────
// Phase 1 – HP output test (~12 s):
//   2 s wait ("plug headphones into 3.5 mm TRS jack")
//   500 ms LEFT-only  1 kHz tone  (confirms L/R routing)
//   200 ms gap
//   500 ms RIGHT-only 1 kHz tone
//   200 ms gap
//   500 ms BOTH channels 1 kHz
//   200 ms gap
//   10-step sweep 200 Hz → 4 kHz, 300 ms per step, 100 ms gap
//
// Phase 2 – LINE_IN monitor (~11 s):
//   Prompt: inject signal into LINE_IN header
//   10 s live monitor: logs peak/RMS per second
//   Real-time passthrough: LINE_IN → HP output

#include "audio_bench_test.h"

#include "driver/i2s_std.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cinttypes>
#include <cmath>
#include <cstdint>
#include <cstring>

static const char *kTag = "audio_bench";

namespace pakt::bench {

// ── Internal: square-wave tone writer ────────────────────────────────────────
// Writes `ms` milliseconds of a square-wave tone at `hz` Hz.
// left_en / right_en independently gate each stereo channel.
static void write_tone(i2s_chan_handle_t tx,
                       uint32_t         sr,
                       uint32_t         hz,
                       uint32_t         ms,
                       bool             left_en,
                       bool             right_en)
{
    int16_t buf[512]; // 256 stereo frames; bench-init only, stack-local is fine
    const uint32_t half_p    = sr / (hz * 2u) > 0u ? sr / (hz * 2u) : 1u;
    uint32_t       remaining = (sr * ms) / 1000u;
    uint32_t       phase     = 0;

    while (remaining > 0u) {
        const size_t frames = remaining > 256u ? 256u : remaining;
        for (size_t i = 0; i < frames; ++i) {
            const int16_t s = ((phase / half_p) & 1u) == 0u ? 14000 : -14000;
            buf[i * 2]     = left_en  ? s : 0;
            buf[i * 2 + 1] = right_en ? s : 0;
            ++phase;
        }
        size_t written = 0;
        i2s_channel_write(tx, buf, frames * 2u * sizeof(int16_t),
                          &written, pdMS_TO_TICKS(300));
        remaining -= static_cast<uint32_t>(frames);
    }
}

// ── Internal: silence writer ──────────────────────────────────────────────────
static void write_silence(i2s_chan_handle_t tx, uint32_t ms, uint32_t sr)
{
    int16_t buf[512]; // bench-init only, stack-local is fine
    memset(buf, 0, sizeof(buf));
    uint32_t remaining = (sr * ms) / 1000u;
    while (remaining > 0u) {
        const size_t frames = remaining > 256u ? 256u : remaining;
        size_t written = 0;
        i2s_channel_write(tx, buf, frames * 2u * sizeof(int16_t),
                          &written, pdMS_TO_TICKS(300));
        remaining -= static_cast<uint32_t>(frames);
    }
}

// ── Phase 1: headphone output test ───────────────────────────────────────────
static void run_output_test(i2s_chan_handle_t tx, uint32_t sr)
{
    ESP_LOGI(kTag, "");
    ESP_LOGI(kTag, "==============================================");
    ESP_LOGI(kTag, "  PHASE 1: HEADPHONE OUTPUT TEST");
    ESP_LOGI(kTag, "==============================================");
    ESP_LOGI(kTag, ">>> ACTION: Plug 3.5mm TRS headphones into the PJRC audio board jack.");
    ESP_LOGI(kTag, ">>> (Standard stereo headphones — TRS, tip=L ring=R sleeve=GND)");
    ESP_LOGI(kTag, ">>> You will hear: LEFT, RIGHT, BOTH channels, then a frequency sweep.");
    ESP_LOGI(kTag, ">>> Starting in 2 seconds...");
    vTaskDelay(pdMS_TO_TICKS(2000));

    ESP_LOGI(kTag, "  [L only] 1 kHz, 500 ms  — left ear only");
    write_tone(tx, sr, 1000, 500, true, false);
    write_silence(tx, 200, sr);

    ESP_LOGI(kTag, "  [R only] 1 kHz, 500 ms  — right ear only");
    write_tone(tx, sr, 1000, 500, false, true);
    write_silence(tx, 200, sr);

    ESP_LOGI(kTag, "  [L+R]    1 kHz, 500 ms  — both ears");
    write_tone(tx, sr, 1000, 500, true, true);
    write_silence(tx, 200, sr);

    ESP_LOGI(kTag, "  [sweep]  200 Hz -> 4000 Hz (both channels)");
    static constexpr uint32_t kFreqs[] = {
        200, 400, 600, 800, 1000, 1500, 2000, 2500, 3000, 4000
    };
    for (uint32_t f : kFreqs) {
        ESP_LOGI(kTag, "           %4lu Hz", static_cast<unsigned long>(f));
        write_tone(tx, sr, f, 300, true, true);
        write_silence(tx, 100, sr);
    }

    ESP_LOGI(kTag, "");
    ESP_LOGI(kTag, ">>> PHASE 1 DONE.");
    ESP_LOGI(kTag, ">>> PASS : heard distinct L / R tones + sweep on both ears.");
    ESP_LOGI(kTag, ">>> FAILS:");
    ESP_LOGI(kTag, ">>>   No sound        -> CHIP_SSS_CTRL routing or CHIP_ANA_POWER issue.");
    ESP_LOGI(kTag, ">>>                      Verify sgtl5000_init() wrote 0x0010 to 0x000A");
    ESP_LOGI(kTag, ">>>                      and 0x40FF to 0x0030.");
    ESP_LOGI(kTag, ">>>   One side silent -> I2S DOUT wiring (GPIO12) or L/R slot swap.");
    ESP_LOGI(kTag, ">>>   Garbled / buzz  -> CHIP_I2S_CTRL DLEN mismatch (need 0x0030).");
}

// ── Phase 2: LINE_IN monitor with real-time passthrough ──────────────────────
static void run_input_test(i2s_chan_handle_t tx, i2s_chan_handle_t rx, uint32_t sr)
{
    ESP_LOGI(kTag, "");
    ESP_LOGI(kTag, "==============================================");
    ESP_LOGI(kTag, "  PHASE 2: LINE_IN INPUT MONITOR (10 seconds)");
    ESP_LOGI(kTag, "==============================================");
    // The PJRC Teensy Audio Adapter Rev D has a separate 3-pin LINE_IN header
    // (L / R / GND).  That is the ADC source configured by sgtl5000_init().
    // The 3.5 mm HP jack has no microphone — it is output-only (TRS, not TRRS).
    // For a microphone test, use the separate MIC header on the PJRC board and
    // re-configure CHIP_ANA_CTRL bit2=0 (MIC_IN) before running that test.
    ESP_LOGI(kTag, ">>> ADC source: LINE_IN (3-pin L/R/GND header on PJRC board).");
    ESP_LOGI(kTag, ">>> ACTION: Connect an audio signal to the LINE_IN header now.");
    ESP_LOGI(kTag, ">>>         e.g. phone headphone out → LINE_IN L+GND (line level ~1 Vrms).");
    ESP_LOGI(kTag, ">>> The captured audio will play back through your headphones in real time.");
    ESP_LOGI(kTag, ">>> Monitoring for 10 seconds...");
    vTaskDelay(pdMS_TO_TICKS(500));

    int16_t io_buf[512]; // 256 stereo frames; bench-init only, stack-local is fine

    const int64_t t_start    = esp_timer_get_time();
    const int64_t t_end      = t_start + 10LL * 1000000LL;
    int64_t       t_next_log = t_start + 1000000LL;
    int32_t       window_n   = 1;
    int32_t       peak       = 0;
    int64_t       sum_sq     = 0;
    int32_t       sample_cnt = 0;

    while (esp_timer_get_time() < t_end) {
        size_t bytes_read = 0;
        esp_err_t err = i2s_channel_read(rx, io_buf, sizeof(io_buf),
                                         &bytes_read, pdMS_TO_TICKS(50));
        if (err == ESP_OK && bytes_read > 0) {
            const size_t n = bytes_read / sizeof(int16_t);
            for (size_t i = 0; i < n; ++i) {
                const int32_t s = io_buf[i];
                const int32_t a = s >= 0 ? s : -s;
                if (a > peak) peak = a;
                sum_sq     += static_cast<int64_t>(s) * s;
                sample_cnt += 1;
            }
            // Real-time passthrough → HP output.
            size_t written = 0;
            i2s_channel_write(tx, io_buf, bytes_read, &written, pdMS_TO_TICKS(50));
        }

        if (esp_timer_get_time() >= t_next_log) {
            const float   mean_sq = sample_cnt > 0
                ? static_cast<float>(sum_sq) / static_cast<float>(sample_cnt)
                : 0.0f;
            const int32_t rms = static_cast<int32_t>(sqrtf(mean_sq));
            ESP_LOGI(kTag, "  [t+%lds] peak=%" PRId32 "  rms=%" PRId32
                     "  (full-scale=32767; >500 = signal detected)",
                     static_cast<long>(window_n), peak, rms);
            peak = 0; sum_sq = 0; sample_cnt = 0;
            ++window_n;
            t_next_log += 1000000LL;
        }
    }

    ESP_LOGI(kTag, "");
    ESP_LOGI(kTag, ">>> PHASE 2 DONE.");
    ESP_LOGI(kTag, ">>> PASS : peak > 500 while signal was present; heard in headphones.");
    ESP_LOGI(kTag, ">>> FAILS:");
    ESP_LOGI(kTag, ">>>   peak always < 50  -> no signal at LINE_IN; check header wiring.");
    ESP_LOGI(kTag, ">>>   peak OK, no audio -> ADC→I2S_OUT→ESP read path; check RX I2S.");
    ESP_LOGI(kTag, ">>>   no passthrough HP -> CHIP_SSS_CTRL routing (0x000A) or TX path.");
}

// ── Public API ────────────────────────────────────────────────────────────────

void run_audio_bench(i2s_chan_handle_t tx_chan,
                     i2s_chan_handle_t rx_chan,
                     uint32_t         sample_rate_hz)
{
    ESP_LOGI(kTag, "");
    ESP_LOGI(kTag, "##############################################");
    ESP_LOGI(kTag, "  PAKT AUDIO BENCH TEST");
    ESP_LOGI(kTag, "  SGTL5000 / Teensy Audio Adapter Rev D");
    ESP_LOGI(kTag, "  Sample rate: %lu Hz",
             static_cast<unsigned long>(sample_rate_hz));
    ESP_LOGI(kTag, "##############################################");

    run_output_test(tx_chan, sample_rate_hz);
    run_input_test(tx_chan, rx_chan, sample_rate_hz);

    ESP_LOGI(kTag, "");
    ESP_LOGI(kTag, "##############################################");
    ESP_LOGI(kTag, "  AUDIO BENCH TEST COMPLETE");
    ESP_LOGI(kTag, "  Normal audio pipeline resuming.");
    ESP_LOGI(kTag, "##############################################");
    ESP_LOGI(kTag, "");
}

} // namespace pakt::bench
