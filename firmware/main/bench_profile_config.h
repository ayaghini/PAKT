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

// Top-level benches.
inline constexpr bool kEnableAudioBench = false;
inline constexpr bool kEnableSa818Bench = false;
inline constexpr bool kEnableAprsBench  = true;

// APRS bench sub-stages.
inline constexpr bool kEnableAprsStage0Loopback    = false;
inline constexpr bool kEnableAprsStageATxBurst     = false;
inline constexpr bool kEnableAprsStageBRxGainSweep = false;
inline constexpr bool kEnableAprsStageBPcmSnapshot = false;  // depends on Stage B
inline constexpr bool kEnableAprsStageCRxRecord    = true;

// Stage C recorder settings.
// Select the recorder ADC gain directly in dB.
// Must be a multiple of 1.5 dB and within the SGTL5000 ADC range used here.
// Common values:
//   0.0f  -> step 0
//   6.0f  -> step 4
//   12.0f -> step 8
//   18.0f -> step 12
inline constexpr float kAprsStageCRecordAdcGainDb = 12.0f;
inline constexpr uint8_t kAprsStageCRecordAdcGainStep =
    static_cast<uint8_t>(kAprsStageCRecordAdcGainDb / 1.5f);

} // namespace pakt::benchcfg
