// bench_profile_config.h
//
// Build-time bench/debug profile selector.
//
// Default profile remains the current working bench image.
// To build the alternate low-noise codec-capture image, configure CMake with:
//   -DPAKT_PROFILE_QUIET_CODEC_CAPTURE=ON

#pragma once

#if defined(PAKT_BENCH_PROFILE_QUIET_CODEC_CAPTURE)
#include "bench_profile_quiet_codec_capture.h"
#else
#include "bench_profile_current.h"
#endif
