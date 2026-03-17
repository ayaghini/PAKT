# PAKT

## Executive Summary
PAKT is an ESP32-S3-based APRS pocket TNC/tracker built around an SA818 VHF radio,
SGTL5000 audio codec, GPS, and BLE. The MVP now includes both the native PAKT BLE
protocol and KISS-over-BLE so existing APRS software can use the device as a BLE TNC.

The software foundation is substantially in place:
- firmware includes AX.25/APRS framing, AFSK modulator/demodulator, BLE GATT services,
  KISS-over-BLE, APRS TX scheduling, telemetry payloads, SGTL5000 audio pipeline bring-up,
  and SA818 control paths
- the desktop test app and KISS bridge are implemented as reference host tools
- host-side software evidence is strong, with current docs recording `364/364` host tests passing

The project is not MVP-complete yet because hardware validation still gates the milestone:
- first ESP32-S3 prototype bring-up
- BLE security and PTT fail-safe validation on hardware
- SA818 electrical/audio validation
- SGTL5000 MCLK and audio calibration on hardware
- end-to-end APRS RF validation
- third-party KISS client validation

## Current Status
- Phase: implementation complete for most software paths, pre-hardware bring-up for MVP closure
- Firmware build flow: ESP-IDF with `idf.py`
- Host test flow: raw CMake only for `firmware/test_host`
- Canonical wire-format source: `doc/aprs_mvp_docs/payload_contracts.md`
- Canonical implementation/risk ledger: `doc/aprs_mvp_docs/agent_bootstrap/audit.md`

## Start Here
- `doc/aprs_mvp_docs/README.md`
- `doc/aprs_mvp_docs/agent_bootstrap/README.md`
- `doc/aprs_mvp_docs/agent_bootstrap/implementation_steps_mvp.md`
- `doc/aprs_mvp_docs/agent_bootstrap/gate_pass_matrix.md`
- `doc/aprs_mvp_docs/agent_bootstrap/audit.md`
- `doc/aprs_mvp_docs/docs/02_mvp_scope.md`
- `doc/aprs_mvp_docs/docs/05_ble_gatt_spec.md`
- `doc/aprs_mvp_docs/docs/16_kiss_over_ble_spec.md`
- `doc/aprs_mvp_docs/payload_contracts.md`
- `docs/dev_setup.md`
- `docs/bench_bringup_checklist.md`
- `hardware/prototyping_wiring.md`
- `hardware/prototype_breakout_wiring_plan.md`

## Repository Layout
- `firmware/`: ESP-IDF firmware, host tests, and embedded integration code
- `app/desktop_test/`: desktop BLE reference client and KISS bridge/harness
- `doc/aprs_mvp_docs/docs/`: product, architecture, protocol, firmware, test, and risk docs
- `doc/aprs_mvp_docs/agent_bootstrap/`: low-token onboarding pack, implementation sequence, gates, and audit log
- `doc/aprs_mvp_docs/hardware/`: hardware interface notes and placeholder BOM
- `docs/`: developer setup and bench bring-up procedures
- `hardware/`: practical prototyping wiring and hardware rationale
- `handoff/agent_integration_package/`: external-agent handoff package for host-software integration work

## Working Rules
- Use ESP-IDF `idf.py` for firmware builds; do not use raw CMake as the direct firmware entrypoint.
- Use raw CMake only for pure-software host tests under `firmware/test_host`.
- Keep `doc/aprs_mvp_docs/docs/05_ble_gatt_spec.md`,
  `doc/aprs_mvp_docs/docs/16_kiss_over_ble_spec.md`, and
  `doc/aprs_mvp_docs/payload_contracts.md` aligned with the implementation.
- Prefer `firmware/main/main.cpp` and the component implementations as the source of truth for what is actually wired versus still hardware-gated.
