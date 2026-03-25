// bench_profile_config.h
//
// Build-time bench/debug profile for the prototype firmware.
//
// Edit these toggles before building/flashing to choose which blocking bench
// stages run at boot. This gives us a simple, user-friendly way to focus on
// one debug slice without commenting code in multiple places.

#pragma once

#include <cstdint>

namespace pakt::benchcfg {

enum class RxInputMode : uint8_t {
    Left = 0,
    Right,
    Average,
    Stronger,
};

// Top-level benches.
// Audio and SA818 benches are disabled for the focused APRS RX test.
// Re-enable when verifying TX audio path or SA818 hardware bring-up.
inline constexpr bool kEnableAudioBench = false;
inline constexpr bool kEnableSa818Bench = false;
inline constexpr bool kEnableAprsBench  = true;

// Audio/codec debug rate.
// Supported here:
//   8000  Hz - original APRS bench rate (MCLK_MULTIPLE_1024, BCLK=256 kHz)
//   16000 Hz - higher-fidelity debug/capture mode (MCLK_MULTIPLE_512, BCLK=512 kHz)
// Using 8 kHz for the focused RX test: simpler BCLK, fewer wiring variables.
inline constexpr uint32_t kAudioSampleRateHz = 8000;

// APRS bench sub-stages.
// TX burst disabled: TX already verified; focused on RX decode.
// Auto-recorder enabled: captures 30 s of demod input starting ~3 s after audio init.
inline constexpr bool kEnableAprsStage0Loopback    = true;
inline constexpr bool kEnableAprsStageATxBurst     = false;
inline constexpr bool kEnableAprsStageBRxGainSweep = true;
inline constexpr bool kEnableAprsStageBPcmSnapshot = true;  // depends on Stage B
inline constexpr bool kEnableAprsStageCRxRecord    = true;
inline constexpr bool kAutoStartRxRecorderOnBoot   = true;
inline constexpr uint32_t kAutoStartRxRecorderDelayMs = 3000;

// RX sample-path debug controls.
// These affect the exact mono samples fed into the recorder and demodulator.
// Stronger: auto-selects whichever LINE_IN channel (L or R) has higher amplitude.
// Avoids silent input if SA818 AF_OUT is wired to the non-Left channel.
inline constexpr RxInputMode kRxInputMode = RxInputMode::Stronger;
inline constexpr bool kRxSwapStereoSlots = false;
inline constexpr bool kRxByteSwapSamples = false;
inline constexpr bool kRxEnableDcBlock   = true;
inline constexpr float kRxDcBlockPole    = 0.995f;
inline constexpr bool kRxLogChannelStats = true;
inline constexpr bool kLogSgtl5000Readback = true;

// Stage C recorder settings.
// Select the recorder ADC gain directly in dB.
// Must be a multiple of 1.5 dB and within the SGTL5000 ADC range used here.
// Common values:
//   0.0f  -> step 0
//   6.0f  -> step 4
//   12.0f -> step 8
//   18.0f -> step 12
inline constexpr float kAprsStageCRecordAdcGainDb = 18.0f;
inline constexpr uint8_t kAprsStageCRecordAdcGainStep =
    static_cast<uint8_t>(kAprsStageCRecordAdcGainDb / 1.5f);

} // namespace pakt::benchcfg
