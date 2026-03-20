// sa818_bench_test.cpp — Staged SA818-V VHF radio module bench test
//
// ── Hardware assumptions ──────────────────────────────────────────────────────
// SA818-V UART protocol: 9600 8N1, AT command set.
// PTT: active-low GPIO (LOW = TX asserted). SA818 PTT pin is active-low.
// AF_OUT: nominally 200–500 mV p-p into a moderate load; AC-coupled to LINE_IN.
// AF_IN:  accepts 0–2 V p-p; SGTL5000 LINE_OUT → AC coupling + attenuation.
// I2S TX writes stereo (L+R both equal) so either LINE_OUT L or R carries signal.
//
// ── SA818-V AT command reference ─────────────────────────────────────────────
//   AT+DMOCONNECT\r\n             → +DMOCONNECT:0\r\n        (handshake OK)
//   AT+DMOVERQ\r\n                → +DMOVERQ:<version>\r\n   (firmware version)
//   AT+DMOSETGROUP=BW,TF,RF,CTCSS_TX,SQ,CTCSS_RX\r\n
//                                 → +DMOSETGROUP:0\r\n       (config OK)
//   AT+DMOSETVOLUME=N\r\n         → +DMOSETVOLUME:0\r\n      (volume OK, N=1-8)
//
// ── TX audio caution ─────────────────────────────────────────────────────────
// Stage 5 (TX audio) asserts PTT for ~14 seconds. Before running:
//   - Connect a 50 Ω dummy load OR a proper antenna to the SA818 SMA connector.
//   - Ensure compliance with local amateur radio regulations (licensed operator).
//   - Each tone is at ~30% FS LINE_OUT level (conservative for SA818 AF_IN).
//   - Do NOT run Stage 5 without a load; the SA818 PA can be damaged by open TX.

#include "sa818_bench_test.h"

#include "driver/i2s_std.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cinttypes>
#include <cstdio>
#include <cstring>

static const char *kTag = "sa818_bench";

namespace pakt::bench {

// ── Helpers ───────────────────────────────────────────────────────────────────

// Send an AT command and read the response.
// Logs the raw command and raw response bytes.
// Returns number of bytes read (0 = no response within timeout).
static size_t at_exchange(pakt::ISa818Transport &t,
                           const char *cmd,
                           char       *resp,
                           size_t      resp_len,
                           uint32_t    timeout_ms = 1500)
{
    ESP_LOGI(kTag, "  TX: %s", cmd); // cmd includes \r\n but ESP log trims trailing whitespace
    t.write(cmd, strlen(cmd));
    size_t n = t.read(resp, resp_len - 1, timeout_ms);
    resp[n] = '\0';

    if (n == 0) {
        ESP_LOGW(kTag, "  RX: <no response within %lu ms>",
                 static_cast<unsigned long>(timeout_ms));
    } else {
        // Log printable summary; replace control chars with '.' for display.
        char display[64];
        size_t di = 0;
        for (size_t i = 0; i < n && di < sizeof(display) - 1; ++i) {
            char c = resp[i];
            display[di++] = (c >= 0x20 && c < 0x7F) ? c : '.';
        }
        display[di] = '\0';
        ESP_LOGI(kTag, "  RX: \"%s\" (%u bytes)", display, static_cast<unsigned>(n));
    }
    return n;
}

// Check whether a response starts with prefix and has status ':0'.
static bool resp_ok(const char *resp, const char *prefix)
{
    if (!resp || !prefix) return false;
    if (strncmp(resp, prefix, strlen(prefix)) != 0) return false;
    const char *colon = strchr(resp, ':');
    return colon && colon[1] == '0';
}

// Write a square-wave tone to the I2S TX channel (stereo L=R).
static void write_tone_stereo(i2s_chan_handle_t tx,
                               uint32_t         sr,
                               uint32_t         hz,
                               uint32_t         ms)
{
    int16_t buf[512]; // 256 stereo frames
    const uint32_t half_p    = sr / (hz * 2u) > 0u ? sr / (hz * 2u) : 1u;
    uint32_t       remaining = (sr * ms) / 1000u;
    uint32_t       phase     = 0;

    while (remaining > 0u) {
        const size_t frames = remaining > 256u ? 256u : remaining;
        for (size_t i = 0; i < frames; ++i) {
            // ~30% full-scale to keep SA818 AF_IN within safe range.
            const int16_t s = ((phase / half_p) & 1u) == 0u ? 10000 : -10000;
            buf[i * 2]     = s;
            buf[i * 2 + 1] = s;
            ++phase;
        }
        size_t written = 0;
        i2s_channel_write(tx, buf, frames * 2u * sizeof(int16_t),
                          &written, pdMS_TO_TICKS(300));
        remaining -= static_cast<uint32_t>(frames);
    }
}

// Write silence (stereo) to the I2S TX channel.
static void write_silence_stereo(i2s_chan_handle_t tx, uint32_t ms, uint32_t sr)
{
    int16_t buf[512];
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

// ── Stage implementations ─────────────────────────────────────────────────────

static bool stage1_uart_handshake(pakt::ISa818Transport &t)
{
    ESP_LOGI(kTag, "");
    ESP_LOGI(kTag, "--- STAGE 1: UART HANDSHAKE (AT+DMOCONNECT) ---");

    char resp[64];
    for (int attempt = 1; attempt <= 3; ++attempt) {
        ESP_LOGI(kTag, "  Attempt %d/3 ...", attempt);
        size_t n = at_exchange(t, "AT+DMOCONNECT\r\n", resp, sizeof(resp));
        if (n > 0 && resp_ok(resp, "+DMOCONNECT")) {
            ESP_LOGI(kTag, "  >>> PASS: SA818 handshake OK on attempt %d", attempt);
            return true;
        }
        if (attempt < 3) {
            ESP_LOGW(kTag, "  No valid response; waiting 500 ms before retry...");
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }

    ESP_LOGE(kTag, "  >>> FAIL: SA818 did not respond to AT+DMOCONNECT after 3 attempts.");
    ESP_LOGE(kTag, "       Check: power to SA818, TX=GPIO13->SA818_RXD, "
                   "RX=GPIO9->SA818_TXD, GND common.");
    return false;
}

static void stage2_version_query(pakt::ISa818Transport &t)
{
    ESP_LOGI(kTag, "");
    ESP_LOGI(kTag, "--- STAGE 2: VERSION QUERY (AT+DMOVERQ, informational) ---");
    char resp[64];
    size_t n = at_exchange(t, "AT+DMOVERQ\r\n", resp, sizeof(resp), 1500);
    if (n == 0) {
        ESP_LOGW(kTag, "  No response (module may not support AT+DMOVERQ — not a failure).");
    } else {
        ESP_LOGI(kTag, "  Firmware info logged above.");
    }
}

static bool stage3_frequency_config(pakt::ISa818Transport &t)
{
    ESP_LOGI(kTag, "");
    ESP_LOGI(kTag, "--- STAGE 3: FREQUENCY CONFIGURATION ---");
    ESP_LOGI(kTag, "  Target: 144.390 MHz, 25 kHz BW, squelch 1, no CTCSS");
    // AT+DMOSETGROUP=BW,TXF,RXF,CTCSS_TX,SQ,CTCSS_RX
    // BW=1 (25 kHz), TXF=RXF=144.3900, CTCSS=0000 (none), SQ=1
    const char *cmd = "AT+DMOSETGROUP=1,144.3900,144.3900,0000,1,0000\r\n";
    char resp[64];
    size_t n = at_exchange(t, cmd, resp, sizeof(resp));

    if (n > 0 && resp_ok(resp, "+DMOSETGROUP")) {
        ESP_LOGI(kTag, "  >>> PASS: SA818 accepted frequency configuration.");
        return true;
    }
    ESP_LOGE(kTag, "  >>> FAIL: SA818 rejected or did not respond to DMOSETGROUP.");
    ESP_LOGE(kTag, "       Check that frequency is in the SA818-V VHF range (136-174 MHz).");
    return false;
}

static void stage4_ptt_toggle(gpio_num_t ptt_gpio)
{
    ESP_LOGI(kTag, "");
    ESP_LOGI(kTag, "--- STAGE 4: PTT TOGGLE TEST ---");
    ESP_LOGI(kTag, "  NOTE: This asserts PTT (GPIO%d) for 500 ms with NO audio.",
             static_cast<int>(ptt_gpio));
    ESP_LOGI(kTag, "        SA818 will transmit a carrier briefly — ensure dummy load or");
    ESP_LOGI(kTag, "        antenna is connected, and comply with local regulations.");
    ESP_LOGI(kTag, "  Asserting PTT (GPIO LOW) ...");

    gpio_set_level(ptt_gpio, 0);   // PTT active-low: LOW = TX
    ESP_LOGI(kTag, "  PTT ASSERTED. SA818 LED should show TX. Holding 500 ms...");
    vTaskDelay(pdMS_TO_TICKS(500));

    gpio_set_level(ptt_gpio, 1);   // PTT HIGH = RX/idle
    ESP_LOGI(kTag, "  PTT DEASSERTED. SA818 should return to RX.");
    ESP_LOGI(kTag, "  >>> PASS: PTT GPIO toggled. Verify visually (SA818 TX LED / APC pin).");
}

// Stage 5: 10-tone stepped TX sequence.
//
// Frequencies (600–2400 Hz in 200 Hz steps) span the APRS audio band and are
// individually distinguishable on any FM receiver.  Each tone is 1 s; gaps are
// 300 ms of silence (PTT stays asserted throughout).
// Total on-air time: ~14 s.
static void stage5_tx_audio_sequence(gpio_num_t           ptt_gpio,
                                      const volatile void *p_i2s_tx,
                                      uint32_t             sample_rate_hz,
                                      uint32_t             tx_wait_ms)
{
    ESP_LOGI(kTag, "");
    ESP_LOGI(kTag, "--- STAGE 5: TX AUDIO PATH — 10-TONE SEQUENCE (LINE_OUT -> SA818 AF_IN) ---");

    if (!p_i2s_tx) {
        ESP_LOGW(kTag, "  Skipped: I2S TX handle pointer is null (pass &g_i2s_tx_chan).");
        return;
    }

    // Wait for audio_task to publish the I2S TX handle.
    ESP_LOGI(kTag, "  Waiting up to %lu ms for I2S TX to be ready (audio_task must init first)...",
             static_cast<unsigned long>(tx_wait_ms));

    const int64_t deadline = esp_timer_get_time()
                             + static_cast<int64_t>(tx_wait_ms) * 1000LL;
    i2s_chan_handle_t tx = nullptr;
    while (esp_timer_get_time() < deadline) {
        tx = *reinterpret_cast<const volatile i2s_chan_handle_t *>(p_i2s_tx);
        if (tx) break;
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    if (!tx) {
        ESP_LOGW(kTag, "  Timeout waiting for I2S TX; skipping TX audio stage.");
        return;
    }
    ESP_LOGI(kTag, "  I2S TX handle ready.");

    ESP_LOGI(kTag, "");
    ESP_LOGI(kTag, "  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    ESP_LOGI(kTag, "  !! TX AUDIO TEST — SA818 WILL TRANSMIT ON 144.390 MHz       !!");
    ESP_LOGI(kTag, "  !! ACTION: Connect 50 ohm dummy load OR antenna NOW          !!");
    ESP_LOGI(kTag, "  !! Monitor on a nearby FM receiver tuned to 144.390 MHz      !!");
    ESP_LOGI(kTag, "  !! You should hear 10 distinct ascending tones (tone 1..10)  !!");
    ESP_LOGI(kTag, "  !! Comply with local amateur radio regulations.              !!");
    ESP_LOGI(kTag, "  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    ESP_LOGI(kTag, "  Transmitting in:");
    for (int c = 5; c > 0; --c) {
        ESP_LOGI(kTag, "    %d ...", c);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // 10 stepped tones: 600 Hz → 2400 Hz in 200 Hz increments.
    static const uint32_t kFreqs[10] = {
        600, 800, 1000, 1200, 1400, 1600, 1800, 2000, 2200, 2400
    };
    static const uint32_t kToneDurationMs = 1000;  // 1 s per tone
    static const uint32_t kGapMs          = 300;   // 300 ms silence between tones

    ESP_LOGI(kTag, "  Asserting PTT for tone sequence (~14 s)...");
    gpio_set_level(ptt_gpio, 0);    // PTT active-low: LOW = TX
    vTaskDelay(pdMS_TO_TICKS(100)); // PA ramp-up

    for (int i = 0; i < 10; ++i) {
        ESP_LOGI(kTag, "  TONE %d/10: %lu Hz — 1 s",
                 i + 1,
                 static_cast<unsigned long>(kFreqs[i]));
        write_tone_stereo(tx, sample_rate_hz, kFreqs[i], kToneDurationMs);
        if (i < 9) {
            write_silence_stereo(tx, kGapMs, sample_rate_hz);
        }
    }

    // Drain DMA (4 descs × 256 frames / 8 kHz ≈ 128 ms).
    vTaskDelay(pdMS_TO_TICKS(150));

    gpio_set_level(ptt_gpio, 1);    // PTT HIGH = RX/idle
    ESP_LOGI(kTag, "  PTT deasserted. TX sequence complete.");

    ESP_LOGI(kTag, "");
    ESP_LOGI(kTag, "  >>> PASS if receiver heard 10 distinct ascending tones on 144.390 MHz.");
    ESP_LOGI(kTag, "  >>> FAIL clues:");
    ESP_LOGI(kTag, "       No signal heard     -> check LINE_OUT L -> SA818 AF_IN wiring + AC cap.");
    ESP_LOGI(kTag, "       Carrier but no tones -> AF_IN path open; check coupling/attenuation.");
    ESP_LOGI(kTag, "       All tones same pitch -> I2S sample rate mismatch; check MCLK config.");
    ESP_LOGI(kTag, "       Overdeviated         -> add attenuation between LINE_OUT and AF_IN.");
}

// Stage 6: RX audio capture.
//
// Opens SA818 squelch so any on-frequency signal passes AF_OUT → LINE_IN.
// Uses rx_peak_fn (if provided) to log the rolling 1-s peak_abs from the
// audio_task demod loop.  This stage runs AFTER stage5_tx_audio_sequence,
// so the audio demod loop is already running and rx_peak_fn returns live data.
//
// Operator action: key a nearby HT on 144.390 MHz and speak for several
// seconds; the log should show peak_abs > 500 while the radio is transmitting.
static void stage6_rx_audio_capture(pakt::ISa818Transport &t,
                                     RxPeakFn              rx_peak_fn)
{
    ESP_LOGI(kTag, "");
    ESP_LOGI(kTag, "--- STAGE 6: RX AUDIO PATH (SA818 AF_OUT -> LINE_IN -> ADC) ---");

    // Open squelch (SQ=0) and set max volume so any on-freq FM signal passes
    // SA818 AF_OUT regardless of signal strength.
    ESP_LOGI(kTag, "  Setting squelch=0 (open) and volume=8 (max AF_OUT level)...");
    {
        char resp[64];
        at_exchange(t, "AT+DMOSETGROUP=1,144.3900,144.3900,0000,0,0000\r\n",
                    resp, sizeof(resp));
        at_exchange(t, "AT+DMOSETVOLUME=8\r\n", resp, sizeof(resp));
    }

    ESP_LOGI(kTag, "");
    ESP_LOGI(kTag, "  >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>");
    ESP_LOGI(kTag, "  >> RX CAPTURE: 20-second operator window starting         <<");
    ESP_LOGI(kTag, "  >> ACTION: Key your handheld radio on 144.390 MHz FM NOW  <<");
    ESP_LOGI(kTag, "  >> Speak continuously or generate any FM audio signal.    <<");
    ESP_LOGI(kTag, "  >> Hold PTT for at least 5-10 seconds.                    <<");
    ESP_LOGI(kTag, "  >> Watch for  rx_peak_abs > 500  in the log below.        <<");
    ESP_LOGI(kTag, "  >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>");
    vTaskDelay(pdMS_TO_TICKS(1000));  // 1 s grace before capture window

    int32_t max_peak        = 0;
    int32_t signal_seconds  = 0;

    for (int s = 0; s < 20; ++s) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        const int32_t peak = rx_peak_fn ? rx_peak_fn() : 0;
        if (peak > max_peak) max_peak = peak;
        if (peak > 500) ++signal_seconds;
        ESP_LOGI(kTag, "  [t+%2d s] rx_peak_abs=%" PRId32 "  %s",
                 s + 1, peak,
                 peak > 500  ? "<<< SIGNAL DETECTED" :
                 peak > 100  ? "(low level)"          : "(silent / no signal)");
    }

    ESP_LOGI(kTag, "");
    ESP_LOGI(kTag, "  RX CAPTURE RESULT: max_peak=%" PRId32
             "  signal_seconds=%d/20  (threshold > 500)",
             max_peak, signal_seconds);

    if (signal_seconds >= 2) {
        ESP_LOGI(kTag, "  >>> PASS: RX audio path confirmed — signal captured from SA818 AF_OUT.");
    } else if (max_peak > 100) {
        ESP_LOGW(kTag, "  >>> MARGINAL: Low-level signal only; check AF_RX_COUPLED wiring/level.");
        ESP_LOGW(kTag, "       Possible: attenuation too high, or operator transmitted briefly.");
    } else {
        ESP_LOGE(kTag, "  >>> FAIL: No significant RX signal detected in 20-second window.");
        if (!rx_peak_fn) {
            ESP_LOGE(kTag, "       (rx_peak_fn not provided — stats came from fallback zero).");
            ESP_LOGE(kTag, "       Pass the demod-loop peak callback in main.cpp to get live data.");
        } else {
            ESP_LOGE(kTag, "       Check: SA818 AF_OUT -> SGTL5000 LINE_IN_L AC coupling cap.");
            ESP_LOGE(kTag, "       Check: Operator transmitted on 144.390 MHz FM.");
            ESP_LOGE(kTag, "       Check: Squelch defeated (SQ=0 set above).");
            ESP_LOGE(kTag, "       Check: audio_task demod loop running (g_i2s_tx_chan published).");
        }
    }

    // Restore squelch=1.
    ESP_LOGI(kTag, "  Restoring squelch=1...");
    {
        char resp[64];
        at_exchange(t, "AT+DMOSETGROUP=1,144.3900,144.3900,0000,1,0000\r\n",
                    resp, sizeof(resp));
    }
    ESP_LOGI(kTag, "  Stage 6 complete.");
}

// ── Public API ────────────────────────────────────────────────────────────────

void run_sa818_bench(pakt::ISa818Transport &transport,
                     gpio_num_t             ptt_gpio,
                     uart_port_t            uart_port,
                     const volatile void   *p_i2s_tx,
                     uint32_t               sample_rate_hz,
                     uint32_t               tx_wait_ms,
                     RxPeakFn               rx_peak_fn)
{
    ESP_LOGI(kTag, "");
    ESP_LOGI(kTag, "##############################################");
    ESP_LOGI(kTag, "  SA818-V STAGED BENCH TEST");
    ESP_LOGI(kTag, "  UART port : UART_NUM_%d", static_cast<int>(uart_port));
    ESP_LOGI(kTag, "  PTT GPIO  : GPIO%d (active-low)", static_cast<int>(ptt_gpio));
    ESP_LOGI(kTag, "  Baud      : 9600 8N1");
    ESP_LOGI(kTag, "  Audio path: LINE_OUT <-> SA818 AF_IN/AF_OUT");
    ESP_LOGI(kTag, "  rx_peak_fn: %s", rx_peak_fn ? "provided" : "not provided");
    ESP_LOGI(kTag, "##############################################");

    // Stage 1 is required; skip audio stages if comms fail.
    bool comms_ok = stage1_uart_handshake(transport);
    stage2_version_query(transport);

    bool cfg_ok = false;
    if (comms_ok) {
        cfg_ok = stage3_frequency_config(transport);
    } else {
        ESP_LOGW(kTag, "  Skipping stages 3-6: no UART communication with SA818.");
    }

    if (comms_ok) {
        stage4_ptt_toggle(ptt_gpio);
        // Stage 5 (TX) runs first: waits for I2S to be ready, then transmits
        // the 10-tone sequence.  After this returns, audio_task's demod loop
        // is live, so Stage 6 (RX) can read live peak stats via rx_peak_fn.
        stage5_tx_audio_sequence(ptt_gpio, p_i2s_tx, sample_rate_hz, tx_wait_ms);
        stage6_rx_audio_capture(transport, rx_peak_fn);
    }

    ESP_LOGI(kTag, "");
    ESP_LOGI(kTag, "##############################################");
    ESP_LOGI(kTag, "  SA818 BENCH TEST COMPLETE");
    ESP_LOGI(kTag, "  UART comms : %s", comms_ok ? "PASS" : "FAIL");
    ESP_LOGI(kTag, "  Freq config: %s", cfg_ok   ? "PASS" : (comms_ok ? "FAIL" : "skip"));
    ESP_LOGI(kTag, "  Normal radio pipeline starting...");
    ESP_LOGI(kTag, "##############################################");
    ESP_LOGI(kTag, "");
}

} // namespace pakt::bench
