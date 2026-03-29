# PAKT

## Executive Summary
PAKT is an ESP32-S3-based APRS pocket TNC/tracker built around an SA818 VHF radio,
SGTL5000 audio codec, GPS, and BLE. The MVP now includes both the native PAKT BLE
protocol and KISS-over-BLE so existing APRS software can use the device as a BLE TNC.

The software foundation is substantially in place, and the current bench prototype
has now cleared the core codec and radio bring-up steps:
- firmware includes AX.25/APRS framing, AFSK modulator/demodulator, BLE GATT services,
  KISS-over-BLE, APRS TX scheduling, telemetry payloads, SGTL5000 audio pipeline bring-up,
  staged SA818 bench tests, and SA818 control paths
- the desktop test app and KISS bridge are implemented as reference host tools
- an in-repo iPhone-only SwiftUI app track now exists under `app/ios/`, built against the same native BLE protocol as the desktop client
- host-side software evidence is strong, with current docs recording `364/364` host tests passing
- bench bring-up has confirmed SGTL5000 discovery and headphone output, shared I2C
  visibility for the MAX17048 and M9N, SA818 UART handshake/config, staged PTT/audio tests,
  and short APRS packet TX receipt on a separate receiver

Recent bench work materially improved the RF picture:
- SA818 UART handshake/config recovered and is now responding again on hardware
- staged PTT test and 10-tone TX bench are confirmed working on the current prototype
- APRS packet TX is now bench-proven again on the current hardware state, with packets
  received externally during the restored full test flow
- RX analog/audio activity reaches the demodulator and now has stronger instrumentation
- RX debug now includes PSRAM-backed WAV export, selectable bench stages, selectable
  sample-path conditioning, and a `16 kHz` capture mode
- a separate quiet-capture firmware profile now supports repeated `30 s` low-noise
  captures with a countdown and framed binary export suitable for handoff analysis
- on-device APRS RX is now proven on hardware after fixing a half-rate I2S capture bug
  and reducing Stage C SA818 receive volume; the prototype now decodes APRS frames on-device
- the APRS message-ack path is now wired into `TxScheduler`, so on-air APRS acks
  can complete the firmware message FSM instead of always timing out
- the GPS path now prefers the shared Feather I2C/STEMMA bus (`u-blox M9N @ 0x42`)
  with `UART2` (`GPIO17/GPIO18`, `38400` baud) retained as a fallback; raw NMEA
  traffic is now logged/published even before a valid fix
- GPS over the shared Feather I2C/STEMMA bus is now working on the current
  prototype, including live telemetry delivery to the app; this path should now
  be treated as the default GPS integration baseline rather than an open bring-up item

The project is not MVP-complete yet because hardware validation still gates the milestone:
- BLE security and PTT fail-safe validation on hardware
- SA818 deviation and on-air receive/transmit calibration
- trusted Bell 202 source validation and on-device APRS RX proof
- third-party KISS client validation

## Project Status At A Glance

| Area | Status | Notes |
|---|---|---|
| Core firmware architecture | `strong` | AX.25/APRS, AFSK, BLE, KISS-over-BLE, scheduling, and host tooling are implemented |
| Bench bring-up | `in progress` | I2C, codec bring-up, SA818 UART/config, PTT, staged TX audio, and APRS TX bench are working |
| APRS TX over RF | `proven on current prototype` | tone sequence and APRS packets were heard/received again after SA818 recovery on 2026-03-21 |
| APRS RX over RF | `proven on current prototype` | quiet-profile RX now decodes APRS on-device after fixing the half-rate capture bug and reducing Stage C volume |
| BLE/KISS hardware validation | `pending` | software-complete enough for hardware validation |
| GPS over shared I2C | `proven on current prototype` | `u-blox M9N @ 0x42` is detected after startup, hot-switches to I2C/DDC, and publishes live telemetry to the app |
| MVP milestone | `open` | blocked by BLE safety validation, radio calibration/repeatability, and remaining hardware gates

## Progress Summary
- Software stack is largely implemented and test-backed.
- Prototype hardware can boot the codec/radio path, recover SA818 control, transmit APRS packets over RF, and now decode APRS on-device.
- Main firmware is now in production mode by default: recorder/export bench stages are disabled on boot while the continuous APRS RX path remains active.
- Demodulator instrumentation now exposes RX peak, flag, FCS-reject, and decode counters during bench work.
- Bench stages are now selectable through [bench_profile_config.h](/Users/macmini4/Desktop/PAKT/firmware/main/bench_profile_config.h), so debug sessions can run only the needed benches/stages instead of the full boot-time sequence.
- Full RX recorder/export now works through PSRAM-backed WAV capture, and the quiet profile can export captured demod-input audio through a framed binary serial stream for offline reconstruction.
- A real RX blocker was found and fixed: the audio pipeline was unpacking only half of each 2048-byte I2S read, effectively feeding the demodulator at `8 kHz` while configured for `16 kHz`.
- After fixing that bug and lowering quiet-profile Stage C SA818 volume from `8` to `4`, the device decoded multiple on-air APRS frames in the same `30 s` window.
- GPS parsing is no longer a stub: `gps_task` now reads the M9N over the shared I2C bus when present, falls back to UART if needed, and publishes BLE GPS telemetry even in the no-fix state.
- GPS over the shared Feather I2C/STEMMA bus is now confirmed working in the current hardware setup, and live GPS data is reaching the app.
- APRS ack handling is no longer a dead end: received APRS acks can now call `ctx.notify_ack()` and complete the message FSM in firmware.
- The RX path is now also configurable at the firmware level for:
  - `8 kHz` or `16 kHz` audio rate
  - left/right/average/stronger channel selection
  - stereo-slot swap
  - sample byte swap
  - firmware DC blocking
- The current best debugging workflow is the separate quiet-capture profile.
- The repo now distinguishes clearly between:
  - audio-path alive
  - packet TX proven
  - on-device APRS RX now proven on the corrected quiet profile

## Immediate Next Steps
1. Validate BLE encrypted+bonded write enforcement and PTT fail-safe behavior on live hardware.
2. Verify the newly wired APRS ack path end-to-end with real on-air message acknowledgements.
3. Validate native BLE + KISS-over-BLE behavior on hardware, including reconnect and at least one real client path.
4. Bring up the iPhone app against the same firmware build: connect, view APRS RX/GPS/status, send TX requests, change radio settings, and enable the BLE debug stream.
5. Keep the shared-I2C GPS path as the default baseline and only use UART fallback for alternate wiring or recovery.

## Bench Profile
- Bench/debug stage selection is controlled in [bench_profile_config.h](/Users/macmini4/Desktop/PAKT/firmware/main/bench_profile_config.h).
- Current toggles allow independent enable/disable of:
  - `audio_bench`
  - `sa818_bench`
  - `aprs_bench`
  - APRS Stage 0 loopback
  - APRS Stage A TX burst
  - APRS Stage B RX gain sweep
  - APRS PCM snapshot dump
  - APRS Stage C full RX recorder/export
- Stage C also has a selectable ADC gain step so targeted RX captures can be taken at the most useful receive setting.
- The same file now controls the audio debug sample rate and RX sample interpretation path.

## Current Status
- Phase: prototype hardware bring-up in progress, with packet TX, on-device APRS RX, and shared-I2C GPS telemetry now proven on the current prototype; remaining work is calibration, repeatability, app validation, and safety validation
- Firmware build flow: ESP-IDF with `idf.py`
- Host test flow: raw CMake only for `firmware/test_host`
- Canonical wire-format source: `docs/aprs_mvp_docs/payload_contracts.md`
- Canonical implementation/risk ledger: `docs/aprs_mvp_docs/agent_bootstrap/audit.md`

## Bench-Proven Baseline
- Feather target: Adafruit ESP32-S3 Feather `4 MB flash / 2 MB PSRAM`
- Shared I2C bus: `SGTL5000 @ 0x0A`, `MAX17048 @ 0x36`, `u-blox M9N @ 0x42`
- Audio bring-up rule: enable `SYS_MCLK` before SGTL5000 I2C init
- Verified audio wiring:
  - `GPIO3` `I2C_SDA`
  - `GPIO4` `I2C_SCL`
  - `GPIO8` `I2S_BCLK`
  - `GPIO15` `I2S_WS`
  - `GPIO12` `I2S_DOUT`
  - `GPIO10` `I2S_DIN`
  - `GPIO14` `I2S_MCLK`
- Verified radio control wiring:
  - `GPIO13` `SA818_RX_CTRL`
  - `GPIO9` `SA818_TX_STAT`
  - `GPIO11` `SA818_PTT`
- Restored and re-verified on 2026-03-21:
  - `AT+DMOCONNECT` handshake passes
  - `AT+DMOSETGROUP` frequency config passes
  - staged PTT test passes
  - 10-tone TX bench is audible on a receiver
  - APRS packet TX is received externally
- Bench modules now in the firmware tree:
  - `firmware/main/audio_bench_test/`
  - `firmware/main/sa818_bench_test/`

## Next Steps
- Measure TX deviation and formalize the corrected RX baseline in the bench docs
- Re-run APRS RX across multiple sessions to confirm repeatable decode margin on the corrected quiet profile
- If needed, tune SGTL5000 input/gain/sample interpretation using the new sample-path debug controls, but the core on-device RX proof now exists
- Measure and record SA818 TX deviation with the actual `LINE_OUT -> AF_IN` attenuation in place
- Validate BLE security enforcement and PTT watchdog behavior on the live hardware stack
- Freeze the harness values into the PCB-facing docs once AF levels, deviation, and PTT topology are measured

## Start Here
- `docs/README.md`
- `docs/aprs_mvp_docs/README.md`
- `docs/aprs_mvp_docs/agent_bootstrap/README.md`
- `docs/aprs_mvp_docs/agent_bootstrap/implementation_steps_mvp.md`
- `docs/aprs_mvp_docs/agent_bootstrap/gate_pass_matrix.md`
- `docs/aprs_mvp_docs/agent_bootstrap/audit.md`
- `docs/aprs_mvp_docs/docs/02_mvp_scope.md`
- `docs/aprs_mvp_docs/docs/05_ble_gatt_spec.md`
- `docs/aprs_mvp_docs/docs/19_ble_integration_reference.md`
- `docs/aprs_mvp_docs/docs/16_kiss_over_ble_spec.md`
- `docs/aprs_mvp_docs/payload_contracts.md`
- `docs/dev_setup.md`
- `docs/bench_bringup_checklist.md`
- `hardware/prototyping_wiring.md`
- `hardware/prototype_breakout_wiring_plan.md`

## Repository Layout
- `docs/`: unified documentation home
- `firmware/`: ESP-IDF firmware, host tests, and embedded integration code
- `app/desktop_test/`: desktop BLE reference client and KISS bridge/harness
- `docs/aprs_mvp_docs/docs/`: product, architecture, protocol, firmware, test, and risk docs
- `docs/aprs_mvp_docs/agent_bootstrap/`: low-token onboarding pack, implementation sequence, gates, and audit log
- `docs/aprs_mvp_docs/hardware/`: hardware interface notes and placeholder BOM
- `docs/hardware/`: hardware source library, CAD asset tracking, and reference links
- `docs/dev_setup.md` + `docs/bench_bringup_checklist.md`: operator/developer companion docs
- `hardware/`: practical prototyping wiring and hardware rationale
- `handoff/agent_integration_package/`: external-agent handoff package for host-software integration work

## Working Rules
- Use ESP-IDF `idf.py` for firmware builds; do not use raw CMake as the direct firmware entrypoint.
- Use raw CMake only for pure-software host tests under `firmware/test_host`.
- Keep `docs/aprs_mvp_docs/docs/05_ble_gatt_spec.md`,
- `docs/aprs_mvp_docs/docs/19_ble_integration_reference.md`,
- `docs/aprs_mvp_docs/agent_bootstrap/ble_protocol_quickref.md`,
  `docs/aprs_mvp_docs/docs/16_kiss_over_ble_spec.md`, and
  `docs/aprs_mvp_docs/payload_contracts.md` aligned with the implementation.
- If BLE UUIDs, payload fields, command semantics, chunking, or client behavior changes,
  update all relevant protocol docs in the same change so the desktop app, iPhone app,
  firmware, and future integrations do not drift.
- Prefer `firmware/main/main.cpp` and the component implementations as the source of truth for what is actually wired versus still hardware-gated.

## Build Directories
- `firmware/build_*` are the intended build output directories for the firmware project.
- Repo-root `build_feather_s3/` is also an ESP-IDF-generated build tree for the same `firmware/` project, not a second source tree.
- The metadata inside both `build_feather_s3/project_description.json` and `firmware/build_feather_s3/project_description.json` points back to `firmware/`; the difference is only the chosen build output path.
