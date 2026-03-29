// bench_profile_current.h
//
// Default firmware profile: all tasks live, APRS bench preamble enabled,
// all bench stages (TX burst / RX sweep / recorder) disabled.
// The AFSK demodulator runs continuously; decoded frames reach BLE clients
// via both native rx_packet notify and KISS RX notify.

#pragma once

#include <cstdint>

namespace pakt::benchcfg {

inline constexpr const char *kProfileName = "current";

enum class LogVerbosity : uint8_t {
    Quiet = 0,
    Normal,
    Verbose,
};

enum class RxInputMode : uint8_t {
    Left = 0,
    Right,
    Average,
    Stronger,
};

enum class RxUnpackMode : uint8_t {
    Packed16Stereo = 0,
    Slot32Low16Stereo,
    Slot32High16Stereo,
};

inline constexpr bool kEnableAudioBench = false;
inline constexpr bool kEnableSa818Bench = false;
inline constexpr bool kEnableAprsBench  = true;

inline constexpr bool kStartAudioTask    = true;
inline constexpr bool kStartRadioTask    = true;
inline constexpr bool kStartWatchdogTask = true;
inline constexpr bool kStartGpsTask      = true;
inline constexpr bool kStartAprsTask     = true;
inline constexpr bool kStartBleTask      = true;
inline constexpr bool kStartPowerTask    = true;

inline constexpr bool kQuietCaptureMode  = false;
inline constexpr LogVerbosity kLogVerbosity = LogVerbosity::Normal;
inline constexpr bool kExportBinaryChunks = false;
inline constexpr uint32_t kExportFlushLineInterval = 64;
inline constexpr uint32_t kExportYieldMs = 1;
inline constexpr uint32_t kExportBinaryChunkBytes = 1024;

inline constexpr uint32_t kAudioSampleRateHz = 16000;

inline constexpr bool kEnableAprsStage0Loopback    = false;
inline constexpr bool kEnableAprsStageATxBurst     = false;
inline constexpr bool kEnableAprsStageBRxGainSweep = false;
inline constexpr bool kEnableAprsStageBPcmSnapshot = false;
inline constexpr bool kEnableAprsStageCRxRecord    = false;
inline constexpr bool kAutoStartRxRecorderOnBoot   = false;
inline constexpr uint32_t kAutoStartRxRecorderDelayMs = 4000;
inline constexpr uint32_t kAprsStageCPreRollSeconds = 0;

inline constexpr RxInputMode kRxInputMode = RxInputMode::Left;
inline constexpr RxUnpackMode kRxUnpackMode = RxUnpackMode::Packed16Stereo;
inline constexpr bool kRxSwapStereoSlots = false;
inline constexpr bool kRxByteSwapSamples = false;
inline constexpr bool kRxEnableDcBlock   = true;
inline constexpr float kRxDcBlockPole    = 0.995f;
inline constexpr bool kRxLogChannelStats = true;
inline constexpr bool kLogSgtl5000Readback = true;

inline constexpr float kAprsStageCRecordAdcGainDb = 0.0f;
inline constexpr uint8_t kAprsStageCRecordAdcGainStep =
    static_cast<uint8_t>(kAprsStageCRecordAdcGainDb / 1.5f);
inline constexpr uint8_t kAprsStageCVolume = 4;  // SA818 AF_OUT volume (1–8); 8 clips at typical signal level

} // namespace pakt::benchcfg
