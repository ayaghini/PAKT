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
- Status: in_progress
- Exit: desktop app can connect/pair/reconnect, exercise core GATT paths, and export debug logs
- Evidence (software done): app/desktop_test/: chunker.py (client-side BleChunker mirror with split + Reassembler), pakt_client.py (bleak async GATT client with transparent chunked writes and notify reassembly), main.py (interactive CLI: scan, connect, DIS read, config read/write, command, TX request, 30 s notify listen, timestamped log export). test_chunker.py (25 pytest tests). CI app-tests job added.
- Residual risk: full GATT validation (pair, write-rejection when unbonded, RX stream, TX result, telemetry, reconnect matrix) blocked until prototype hardware is available.

## Step 6 - App connectivity and config
- IDs: APP-001, APP-002, APP-003, APP-008
- Status: in_progress
- Exit: stable pair/connect/reconnect + config R/W
- Evidence (software done): app/desktop_test/: transport.py (BleTransport FSM: IDLE/SCANNING/CONNECTING/CONNECTED/RECONNECTING/ERROR, bounded reconnect: 3 attempts x 1 s, per architecture contract G, on_reconnected callback re-subscribes GATT notify), config_store.py (local JSON cache with save/load/validate/diff), pakt_client.py refactored to use BleTransport + ConfigStore (auto-cache on read, auth-error classification via is_auth_error()), main.py updated with state display, config diff preview, offline config view, and pairing guidance on auth errors. test_app.py: 45 pytest tests covering ConfigStore, BleTransport FSM, reconnect callbacks, and is_auth_error().
- Residual risk: reconnect matrix and config persistence checks against real hardware blocked until prototype available.

## Step 7 - Messaging end-to-end
- IDs: FW-010, APP-006
- Status: in_progress
- Exit: send->pending->ack/timeout flow works reliably
- Evidence (software done): TxMessage + TxScheduler (components/aprs_fsm/): static 8-slot queue, 5-retry policy at 20 s intervals, enqueue/tick/on_ack_received/cancel API, result callback fires on ACKED/TIMED_OUT/CANCELLED. 26 host unit tests in test_host/test_tx_scheduler.cpp. Python MessageTracker (app/desktop_test/message_tracker.py): MsgState FSM, on_sent/on_tx_result/cancel/pending/recent/clear_resolved API; 37 pytest tests in test_messaging.py. pakt_client.py routes tx_result notify into MessageTracker; main.py adds [9] message queue view. CI app-tests updated to include test_messaging.py.
- Residual risk: full send→ack/timeout flow requires hardware (firmware TxScheduler wired into APRS task, radio TX, and ack detection). BLE TX result notify format must be confirmed against firmware implementation when hardware is available.

## Step 8 - Telemetry and operator UX
- IDs: APP-004, APP-005, FW-015
- Status: in_progress
- Exit: status + RX stream + diagnostics visible and exportable
- Evidence (software done): Telemetry component (components/telemetry/): DeviceStatus, GpsTelem, PowerTelem, SysTelem structs + compact JSON serialisers (to_json, ≤ 240 B/frame). 18 host unit tests in test_host/test_telemetry.cpp. Python telemetry.py: typed parsers + summary formatters for all 4 channels, parse_notify dispatcher. diagnostics.py: DiagnosticsStore — 300-sample ring per channel, running stats (min/max/avg), export_dict/export_json. 40 pytest tests in test_telemetry_app.py. pakt_client.py routes telemetry notifies into DiagnosticsStore; rx_packet frames into add_rx_frame. main.py adds [T] telemetry snapshot and [X] export diagnostics report. CI app-tests updated.
- Residual risk: live telemetry stream, GPS fix, power readings, and system stats require prototype hardware. RX frame delivery via rx_packet notify requires APRS decode pipeline wired in firmware.

## Step 9 - MVP validation gates
- IDs: QA-003, QA-004, QA-006, DOC-001, DOC-003
- Status: in_progress
- Exit: all MVP gates pass (see `qa_gates.md`)
- Evidence (software done): gate_pass_matrix.md — full G0–G4 assessment with pass/partial/blocked per check item and 8-item residual risk table with mitigation owners. CI regression suite (QA-006) operational: firmware-build + host-tests + app-tests on every push/PR. DOC-001: docs/13_quickstart_guide.md — 9-step first-use guide covering installation, pairing, config, TX, telemetry, and log export. DOC-003: docs/14_pairing_security_policy.md — LE SC security model, AUTH_ERR resolution procedure, bond-reset flow, multi-client notes, regulatory notes. QA-003 (RF functional test) and QA-004 (BLE endurance matrix) blocked until hardware available.
- Residual risk: G1 (functional), G3 hardware portion (bonded-write rejection, PTT safe-off), and G4 blocked pending EVT prototype. PTT safe-off under fault (G3) is a P0 blocker for any TX-capable field use.

## Step 10 - Post-MVP interop
- IDs: INT-001, INT-003, DOC-004
- Status: in_progress
- Exit: capability negotiation + draft KISS-over-BLE interop
- Evidence (software done): DeviceCapabilities component (components/capability/): feature bitmask, JSON serialiser, mvp_defaults(), has() API; 16 host unit tests in test_capability.cpp. kDeviceCapabilities UUID (0xA0040000) added to BleUuids.h. Python capability.py: DeviceCapabilities parser (protocol, fw_ver, hw_rev, features frozenset), CapabilityNegotiator (read on connect, assumed_mvp() fallback, feature flag API, on_caps callback with CAPS_WARN for missing MVP features, reset on disconnect); 28 pytest tests in test_capability.py. pakt_client.py reads capabilities on connect, exposes capabilities property, logs CAPS/CAPS_WARN. CI app-tests updated. INT-003 draft spec: docs/16_kiss_over_ble_spec.md — KISS Service UUIDs, chunked frame transport, TX/RX paths, multi-client arbitration, capability flag, open questions. DOC-004: docs/15_interoperability_matrix.md — platform matrix for Windows/macOS/Linux/Android/iOS, KISS bridge compat table, frequency configs, known limitations, hardware validation checklist.
- Residual risk: Device Capabilities characteristic not yet wired into BleServer (requires hardware bring-up); CapabilityNegotiator falls back to assumed_mvp() until then. KISS profile deferred to M3.

## Step 11 - HF discovery track
- IDs: HF-001..HF-011
- Status: todo
- Exit: go/no-go decision record for production HF audio bridge
- Evidence: latency/jitter/battery measurements and explicit decision rationale
- Note: This is a discovery track that can run alongside Steps 9-10 if a second agent is available, but must not block MVP milestone closure. A single agent should complete Steps 0-10 before starting Step 11.

