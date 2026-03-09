# MVP Implementation Steps (Agent Sequence)

Status legend: `todo | in_progress | blocked | done`
Evidence legend: `code + tests + gate summary + residual risks`

## Step 0 - Project readiness
- IDs: PM-001, PM-002, FW-001
- Status: todo
- Exit: reproducible build + board + DoD/DoR checklists
- Evidence: build log, toolchain pinning summary, checklist location

## Step 1 - Hardware control baseline
- IDs: HW-002, FW-002, FW-003
- Status: todo
- Exit: SA818 driver works on bench; PTT safe-off verified
- Evidence: SA818 command test results, PTT fault-path test note

## Step 2 - Audio pipeline baseline (SGTL5000)
- IDs: FW-004, QA-002
- Status: todo
- Exit: stable I2S read/write at target sample rate without sustained underrun; SGTL5000 `SYS_MCLK` validation complete
- Evidence: sample-rate verification, underrun/overrun counters, reinit stability note

## Step 3 - APRS modem core
- IDs: FW-006, FW-007, FW-008, FW-009
- Status: todo
- Exit: known-vector encode/decode passes; reference receiver decodes TX frames
- Evidence: vector test results, TX decode proof summary

## Step 4 - BLE services and security
- IDs: FW-011, FW-012, INT-002
- Status: todo
- Exit: full GATT endpoints + encrypted/bonded write policy validated
- Evidence: write rejection when unbonded, chunking/reassembly test results

## Step 5 - App connectivity and config
- IDs: APP-001, APP-002, APP-003, APP-008
- Status: todo
- Exit: stable pair/connect/reconnect + config R/W
- Evidence: reconnect matrix summary, config persistence checks

## Step 6 - Messaging end-to-end
- IDs: FW-010, APP-006
- Status: todo
- Exit: send->pending->ack/timeout flow works reliably
- Evidence: message trace samples for ack and timeout paths

## Step 7 - Telemetry and operator UX
- IDs: APP-004, APP-005, FW-015
- Status: todo
- Exit: status + RX stream + diagnostics visible and exportable
- Evidence: telemetry payload examples, export artifact sample

## Step 8 - MVP validation gates
- IDs: QA-003, QA-004, QA-006, DOC-001, DOC-003
- Status: todo
- Exit: all MVP gates pass (see `qa_gates.md`)
- Evidence: gate-by-gate pass matrix with unresolved risks list

## Step 9 - Post-MVP interop
- IDs: INT-001, INT-003, DOC-004
- Status: todo
- Exit: capability negotiation + draft KISS-over-BLE interop
- Evidence: negotiation behavior summary, compatibility notes

## Step 10 - HF discovery track (parallel)
- IDs: HF-001..HF-011
- Status: todo
- Exit: go/no-go decision record for production HF audio bridge
- Evidence: latency/jitter/battery measurements and explicit decision rationale
