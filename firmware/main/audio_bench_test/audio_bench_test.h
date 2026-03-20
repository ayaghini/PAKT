// audio_bench_test.h — PJRC Teensy Audio Adapter Rev D (SGTL5000) bench test
//
// ── Hardware facts (verified against PJRC Rev D schematic) ────────────────────
//   HP_OUT  : 3.5 mm TRS jack (tip=L, ring=R, sleeve=GND) — OUTPUT ONLY.
//             Driven by SGTL5000 HP amp via 2.2 µF DC-blocking caps.
//             This is NOT a TRRS combo jack; there is no mic on the HP jack.
//   LINE_IN : Separate 3-pin header (L / R / GND) on PJRC board.
//             ADC source set to LINE_IN by sgtl5000_init() (CHIP_ANA_CTRL bit2=1).
//             Accepts line-level signals (~1 Vrms).
//   MIC_IN  : Separate 2-pin or 3-pin MIC header on PJRC board.
//             NOT connected to the HP 3.5 mm jack.
//             Activated by clearing CHIP_ANA_CTRL bit2 (not done in bench test).
//
// ── Usage ─────────────────────────────────────────────────────────────────────
//   After sgtl5000_init() succeeds, call run_audio_bench().
//   The function blocks for ~25 seconds then returns normally.
//   To disable: remove the call in audio_pipeline_run().

#pragma once

#include "driver/i2s_std.h"
#include <cstdint>

namespace pakt::bench {

// Run the full bench-test sequence (~25 s, blocking).
//
//   tx_chan        – I2S TX channel (ESP32-S3 → SGTL5000 DAC → HP jack)
//   rx_chan        – I2S RX channel (SGTL5000 ADC LINE_IN → ESP32-S3)
//   sample_rate_hz – must match codec config (8000 Hz)
void run_audio_bench(i2s_chan_handle_t tx_chan,
                     i2s_chan_handle_t rx_chan,
                     uint32_t         sample_rate_hz);

} // namespace pakt::bench
