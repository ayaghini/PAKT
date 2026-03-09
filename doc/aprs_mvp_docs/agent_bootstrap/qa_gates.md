# QA Gates (Must Pass)

## Gate G0 - Build/Test hygiene (every change)
- Firmware builds in CI/local.
- Unit tests for touched logic pass.
- No new critical warnings in touched modules.

## Gate G1 - Functional MVP
- Beacon TX decodes on known-good receiver.
- SGTL5000 `SYS_MCLK` and sample-rate configuration remains stable across reconnect/reinit cycles.
- RX decode stream reaches app correctly.
- Message send/ack/timeout states correct.
- Controlled-condition beacon decode target: >= 95%.

## Gate G2 - BLE reliability
- 1 hour continuous RX BLE session stable.
- Reconnect works after link drop and device reboot.
- Chunk reassembly handles loss/duplicates/timeouts.

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

## Gate failure handling
- If a gate cannot be executed due to hardware access limits, mark as `blocked` with exact missing dependency.
- Do not treat partial checks as full pass.
- Any safety/security gate failure blocks milestone closure even if functional gates pass.

