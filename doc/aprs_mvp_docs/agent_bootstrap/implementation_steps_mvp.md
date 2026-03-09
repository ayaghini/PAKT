# MVP Implementation Steps (Agent Sequence)

Status legend: `todo | in_progress | blocked | done`
Evidence legend: `code + tests + gate summary + residual risks`

## Step 0 - Project readiness
- IDs: PM-001, PM-002, FW-001
- Status: done
- Exit: reproducible build confirmed; board detected; toolchain version pinned; CI baseline passing
- Evidence: firmware/CMakeLists.txt (ESP-IDF v5.3.2, target esp32s3); sdkconfig.defaults with pinned BLE/FreeRTOS config; .github/workflows/ci.yml with firmware-build and host-tests jobs
- Connected-board add-on: command profile selected + first successful flash/boot log capture

## Step 1 - Hardware control baseline
- IDs: HW-002, FW-002, FW-003
- Status: in_progress
- Exit: SA818 driver works on bench; PTT safe-off verified
- Evidence (FW-002 done): IAudioIO/IRadioControl/IPacketLink/IStorage interfaces in firmware/components/pakt_hal/include/pakt/; mocks in mock/; 40 host unit tests passing in firmware/test_host/. FW-003 (SA818 driver) blocked pending hardware.
- Residual risk: FW-003 SA818 driver and HW-002 electrical validation blocked until prototype hardware is available.

## Step 2 - Audio pipeline baseline (SGTL5000)
- IDs: FW-004, QA-002
- Status: todo
- Exit: stable I2S read/write at target sample rate without sustained underrun; SGTL5000 `SYS_MCLK` validation complete
- Evidence: sample-rate verification, underrun/overrun counters, reinit stability note

## Step 3 - APRS modem core
- IDs: FW-006, FW-007, FW-008, FW-009
- Status: in_progress
- Exit: known-vector encode/decode passes; reference receiver decodes TX frames
- Evidence (software done): AX.25 codec (components/ax25/), APRS helpers (components/aprs/), AfskModulator + AfskDemodulator (components/modem/); full encode->modulate->demodulate->decode round-trip test suite in test_host/. Reference receiver TX decode blocked until hardware is available.
- Residual risk: Bell 202 demodulator uses transition-tracking sync (no PLL); real-world timing and level calibration will need tuning on hardware. Audio deviation calibration is a known top risk.

## Step 4 - BLE services and security
- IDs: FW-011, FW-012, INT-002
- Status: in_progress
- Exit: full GATT endpoints + encrypted/bonded write policy validated
- Evidence (software done): ble_services component (components/ble_services/): BleChunker (pure C++ splitter/reassembler), BleServer (NimBLE GATT server with all 9 characteristics across 3 services, encrypted+bonded write enforcement, rate-limited notify). BleChunker host unit tests in test_host/test_ble_chunker.cpp. BleServer requires hardware for full GATT/security validation.
- Residual risk: BleServer encrypted+bonded write rejection and GATT endpoint validation blocked until prototype hardware is available. Stub handlers in main.cpp ble_task will be replaced when APRS logic is wired in (Steps 6-7).

## Step 5 - Desktop BLE test app baseline
- IDs: APP-000
- Status: todo
- Exit: desktop app can connect/pair/reconnect, exercise core GATT paths, and export debug logs
- Evidence: desktop session logs for config, RX stream, TX result, telemetry, reconnect

## Step 6 - App connectivity and config
- IDs: APP-001, APP-002, APP-003, APP-008
- Status: todo
- Exit: stable pair/connect/reconnect + config R/W
- Evidence: reconnect matrix summary, config persistence checks

## Step 7 - Messaging end-to-end
- IDs: FW-010, APP-006
- Status: todo
- Exit: send->pending->ack/timeout flow works reliably
- Evidence: message trace samples for ack and timeout paths

## Step 8 - Telemetry and operator UX
- IDs: APP-004, APP-005, FW-015
- Status: todo
- Exit: status + RX stream + diagnostics visible and exportable
- Evidence: telemetry payload examples, export artifact sample

## Step 9 - MVP validation gates
- IDs: QA-003, QA-004, QA-006, DOC-001, DOC-003
- Status: todo
- Exit: all MVP gates pass (see `qa_gates.md`)
- Evidence: gate-by-gate pass matrix with unresolved risks list

## Step 10 - Post-MVP interop
- IDs: INT-001, INT-003, DOC-004
- Status: todo
- Exit: capability negotiation + draft KISS-over-BLE interop
- Evidence: negotiation behavior summary, compatibility notes

## Step 11 - HF discovery track
- IDs: HF-001..HF-011
- Status: todo
- Exit: go/no-go decision record for production HF audio bridge
- Evidence: latency/jitter/battery measurements and explicit decision rationale
- Note: This is a discovery track that can run alongside Steps 9-10 if a second agent is available, but must not block MVP milestone closure. A single agent should complete Steps 0-10 before starting Step 11.
