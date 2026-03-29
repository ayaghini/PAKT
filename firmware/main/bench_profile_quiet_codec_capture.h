// bench_profile_quiet_codec_capture.h
//
// Alternate low-noise capture profile for codec/system-path investigation.
// Intentionally minimizes non-audio system activity during RX recording.

#pragma once

#include <cstdint>

namespace pakt::benchcfg {

inline constexpr const char *kProfileName = "quiet_codec_capture";

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
inline constexpr bool kStartWatchdogTask = false;
inline constexpr bool kStartGpsTask      = false;
inline constexpr bool kStartAprsTask     = true;
inline constexpr bool kStartBleTask      = false;
inline constexpr bool kStartPowerTask    = false;

inline constexpr bool kQuietCaptureMode  = true;
inline constexpr LogVerbosity kLogVerbosity = LogVerbosity::Verbose;
inline constexpr bool kExportBinaryChunks = true;
inline constexpr uint32_t kExportFlushLineInterval = 8;
inline constexpr uint32_t kExportYieldMs = 10;
inline constexpr uint32_t kExportBinaryChunkBytes = 512;

inline constexpr uint32_t kAudioSampleRateHz = 16000;

inline constexpr bool kEnableAprsStage0Loopback    = false;
inline constexpr bool kEnableAprsStageATxBurst     = false;
inline constexpr bool kEnableAprsStageBRxGainSweep = false;
inline constexpr bool kEnableAprsStageBPcmSnapshot = false;
inline constexpr bool kEnableAprsStageCRxRecord    = true;
inline constexpr bool kAutoStartRxRecorderOnBoot   = false;
inline constexpr uint32_t kAutoStartRxRecorderDelayMs = 4000;
inline constexpr uint32_t kAprsStageCPreRollSeconds = 30;

inline constexpr RxInputMode kRxInputMode = RxInputMode::Average;
inline constexpr RxUnpackMode kRxUnpackMode = RxUnpackMode::Packed16Stereo;
inline constexpr bool kRxSwapStereoSlots = false;
inline constexpr bool kRxByteSwapSamples = false;
inline constexpr bool kRxEnableDcBlock   = true;
inline constexpr float kRxDcBlockPole    = 0.995f;
inline constexpr bool kRxLogChannelStats = true;   // confirm L vs R channel levels
inline constexpr bool kLogSgtl5000Readback = true;

inline constexpr float kAprsStageCRecordAdcGainDb = 1.5f;  // 1 step × 1.5 dB/step; 3 steps clipped
inline constexpr uint8_t kAprsStageCVolume = 4;            // SA818 AF_OUT volume (1–8); 8 clips at this signal level
inline constexpr uint8_t kAprsStageCRecordAdcGainStep =
    static_cast<uint8_t>(kAprsStageCRecordAdcGainDb / 1.5f);

} // namespace pakt::benchcfg
