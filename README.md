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
- host-side software evidence is strong, with current docs recording `364/364` host tests passing
- bench bring-up has confirmed SGTL5000 discovery and headphone output, shared I2C
  visibility for the MAX17048 and M9N, SA818 UART handshake/config, staged PTT/audio tests,
  and short APRS packet TX receipt on a separate receiver

Today’s bench work materially improved the RF picture:
- APRS packet TX has been bench-proven in at least one supervised setup
- RX analog/audio activity reaches the demodulator and now has stronger instrumentation
- RX debug now includes PSRAM-backed WAV export, selectable bench stages, selectable
  sample-path conditioning, and a `16 kHz` capture mode
- on-device APRS RX is still not proven; current evidence now points more strongly
  at the SGTL5000 / I2S / sample-capture path than at gross analog breadboard failure

The project is not MVP-complete yet because hardware validation still gates the milestone:
- BLE security and PTT fail-safe validation on hardware
- SA818 deviation and on-air receive/transmit calibration
- trusted Bell 202 source validation and on-device APRS RX proof
- third-party KISS client validation

## Project Status At A Glance

| Area | Status | Notes |
|---|---|---|
| Core firmware architecture | `strong` | AX.25/APRS, AFSK, BLE, KISS-over-BLE, scheduling, and host tooling are implemented |
| Bench bring-up | `in progress` | I2C, codec bring-up, SA818 UART/PTT, and staged audio tests are working |
| APRS TX over RF | `partially proven` | short APRS packet burst reportedly decoded on a separate receiver |
| APRS RX over RF | `not yet proven` | RX analog path is alive, but the prototype has not yet decoded a valid on-air APRS frame; current suspicion is in the codec/sample-capture path |
| BLE/KISS hardware validation | `pending` | software-complete enough for hardware validation |
| MVP milestone | `open` | blocked by RF validation, RX proof, BLE safety validation, and remaining hardware gates

## Progress Summary
- Software stack is largely implemented and test-backed.
- Prototype hardware can boot the codec/radio path and transmit APRS packets over RF.
- Demodulator instrumentation now exposes RX peak, flag, FCS-reject, and decode counters during bench work.
- Bench stages are now selectable through [bench_profile_config.h](/Users/macmini4/Desktop/PAKT/firmware/main/bench_profile_config.h), so debug sessions can run only the needed benches/stages instead of the full boot-time sequence.
- Full RX recorder/export now works through PSRAM-backed WAV capture, and the firmware can export the captured demod-input audio as base64 WAV over serial for offline analysis.
- The RX path is now also configurable at the firmware level for:
  - `8 kHz` or `16 kHz` audio rate
  - left/right/average/stronger channel selection
  - stereo-slot swap
  - sample byte swap
  - firmware DC blocking
- The repo now distinguishes clearly between:
  - audio-path alive
  - packet TX proven
  - on-device APRS RX still unproven

## Immediate Next Steps
1. Compare the new `16-bit / 16 kHz` captured WAVs against the scope captures to isolate where the digital path stops matching the analog waveform.
2. Continue SGTL5000/I2S capture-path debugging with the new sample-rate and sample-interpretation controls in `bench_profile_config.h`.
3. Measure and record SA818 TX deviation under the actual `LINE_OUT -> AF_IN` attenuation network.
4. Resume BLE bonded-write and PTT fail-safe validation on live hardware after RX capture-path closure.

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
- Phase: prototype hardware bring-up in progress, with packet TX proven and RX troubleshooting focused on codec/sample-capture validation
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
- Bench modules now in the firmware tree:
  - `firmware/main/audio_bench_test/`
  - `firmware/main/sa818_bench_test/`

## Next Steps
- Compare saved `16 kHz` WAV captures against the oscilloscope captures and narrow the SGTL5000/I2S mismatch
- Re-run APRS RX with the new capture controls and confirm at least one on-device APRS decode
- If needed, tune SGTL5000 input/gain/sample interpretation using the new sample-path debug controls
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
  `docs/aprs_mvp_docs/docs/16_kiss_over_ble_spec.md`, and
  `docs/aprs_mvp_docs/payload_contracts.md` aligned with the implementation.
- Prefer `firmware/main/main.cpp` and the component implementations as the source of truth for what is actually wired versus still hardware-gated.

## Build Directories
- `firmware/build_*` are the intended build output directories for the firmware project.
- Repo-root `build_feather_s3/` is also an ESP-IDF-generated build tree for the same `firmware/` project, not a second source tree.
- The metadata inside both `build_feather_s3/project_description.json` and `firmware/build_feather_s3/project_description.json` points back to `firmware/`; the difference is only the chosen build output path.
