# QA Gates (Must Pass)

Gate scope:
- **G0** — per-change gate: must pass before merging any PR or committing a step complete. Failure blocks the current change, not just milestone closure.
- **G1–G4** — milestone gates: must all pass before closing the MVP milestone. Any G1–G4 failure blocks milestone closure. Safety/security gate (G3) failures additionally block any further TX-capable changes.

## Gate G0 - Build/Test hygiene (every change)
- Firmware builds in CI/local.
- Unit tests for touched logic pass.
- No new critical warnings in touched modules.
- If board connected: firmware flashes and boots cleanly on target ESP32-S3.

## Gate G1 - Functional MVP
- Beacon TX decodes on known-good receiver.
- SGTL5000 `SYS_MCLK` and sample-rate configuration remains stable across reconnect/reinit cycles.
- RX decode stream reaches app correctly.
- Message send/ack/timeout states correct.
- KISS-over-BLE exchanges frames correctly with at least one reference client or bridge.
- Controlled-condition beacon decode target: >= 95%.

## Gate G2 - BLE reliability
- Desktop harness connect/reconnect and log export pass before mobile validation.
- 1 hour continuous RX BLE session stable.
- Reconnect works after link drop and device reboot.
- Chunk reassembly handles loss/duplicates/timeouts.
- Native BLE and KISS-over-BLE can coexist without breaking each other.

## Gate G3 - Security and safety
- Write endpoints blocked when not encrypted+bonded.
- Pairing window and bond-reset flow validated.
- Fault tests always return to `PTT=off`.

## Gate G4 - Power and field sanity
- No reset loop under repeated TX bursts.
- Battery telemetry is sane and monotonic under load.
- Field test script completed with issue log.

## Evidence required before closing milestone
- Test log summary
- Failures + fixes
- Residual risks + mitigation owner
- For connected runs: port/profile used and startup monitor excerpt

## Gate failure handling
- G0 failure: do not mark the step complete or merge; fix and re-run before proceeding.
- G1–G4 failure: do not close the MVP milestone; document the failure, assign an owner, and resolve before release.
- If a gate cannot be executed due to hardware access limits, mark as `blocked` with exact missing dependency.
- Do not treat partial checks as full pass.
- G3 (security/safety) failure additionally blocks any further TX-capable changes until resolved.
