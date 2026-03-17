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
- IDs: HW-002, FW-002, FW-003, FW-016
- Status: in_progress
- Exit: SA818 driver works on bench; PTT safe-off verified
- Evidence (FW-002 done): IAudioIO/IRadioControl/IPacketLink/IStorage interfaces in firmware/components/pakt_hal/include/pakt/; mocks in mock/; 40 host unit tests passing in firmware/test_host/. FW-003 (SA818 driver) software-complete (see below).
- Evidence (FW-003 done — software): Sa818Radio component (firmware/components/radio_sa818/): ISa818Transport (injectable UART abstraction), Sa818CommandFormatter (connect/set_group AT builders), Sa818ResponseParser (Ok/Error/Unknown classifier), Sa818Radio (IRadioControl impl; idempotent set_freq; force_ptt_off on any UART failure). Sa818UartTransport (firmware/main/Sa818UartTransport.h): ESP-IDF UART1 concrete transport, excluded from host tests. radio_task wired: GPIO11 configured as PTT output (default HIGH=off), direct-GPIO safe-off callback registered before init(), uart_driver_install + SA818 init + APRS freq set, safe-off callback upgraded to radio.ptt(false) after successful init. 18 host unit tests in test_host/test_sa818.cpp covering formatter, parser, and Sa818Radio state machine (PTT before/after init, init success/failure/timeout, set_freq idempotency, set_freq failure, ptt(false) in error state).
- Evidence (FW-016 done — software): PttWatchdog + PttController components (firmware/components/safety_watchdog/): pure C++ IDLE→ARMED→TRIGGERED FSM; 10 s default timeout; safe_fn fires exactly once via compare_exchange_strong; heartbeat() recovery path; PttController provides settable safe-off hook (ptt_register_safe_off / ptt_safe_off / ptt_is_registered). 21 host unit tests in test_host/test_ptt_watchdog.cpp covering all state transitions, wrap-around arithmetic, force_safe idempotency, PttController no-op/registered/watchdog-integration/state-transition cases, and RadioControlMock integration. watchdog_task wired into main.cpp at priority 6 (ticks every 500 ms); aprs_task calls heartbeat() each loop iteration; safe_fn calls pakt::ptt_safe_off(). radio_task now registers direct-GPIO safe-off before SA818 init and upgrades to `radio.ptt(false)` after successful init.
- Residual risk: FW-003 SA818 electrical validation (UART handshake, PTT polarity, audio deviation) blocked until prototype hardware is available (bench checklist step 5). FW-016 hardware portion (hardware PTT fault injection) blocked pending EVT prototype. Sa818Radio software integration is complete.

## Step 2 - Audio pipeline baseline (SGTL5000)
- IDs: FW-004, QA-002
- Status: in_progress
- Exit: stable I2S read/write at target sample rate without sustained underrun; SGTL5000 `SYS_MCLK` validation complete
- Evidence (software done): `audio_pipeline_run()` in `firmware/main/main.cpp` now performs SGTL5000 I2C init, full-duplex I2S channel setup, RX-side `AfskDemodulator` processing into `g_rx_ax25_queue`, and TX-side publication of `g_i2s_tx_chan` for shared `afsk_tx_frame()` use. Clocking was corrected to an 8 kHz plan using `I2S_MCLK_MULTIPLE_1024` and SGTL5000 `SYS_FS = 32 kHz, RATE_MODE = ÷4`, giving `8.192 MHz` MCLK. Runtime bring-up remains hardware-gated.
- Residual risk: verify actual MCLK/BCLK/WS on the bench, confirm SGTL5000 lock, check underrun/overrun behavior, and calibrate ADC/DAC gain on hardware.

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

## Step 4b - GPS/NMEA parser (FW-005)
- IDs: FW-005
- Status: done (software; hardware integration pending)
- Exit: GPRMC/GPGGA parsed correctly; stale-fix management operational; GpsTelem populated
- Evidence: firmware/components/gps/ — NmeaParser (feed() byte-stream + process() batch interface, GPRMC/GNRMC + GPGGA/GNGGA, checksum verification, decimal-degree conversion, Unix timestamp from date+time, stale-fix mark_stale/reset). 37 host unit tests in test_host/test_nmea_parser.cpp. gps_task in main.cpp updated to use NmeaParser with 5 s stale-fix timeout and 1 Hz BLE notify publish (UART read stubbed pending hardware).
- Residual risk: UART byte-stream path (NEO-M8N on board's GPS port) blocked until hardware; timing/level validated on hardware. gps_task UART stub must be replaced before real GPS data flows.

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
- Evidence (DeviceConfigStore + NvsStorage done — software): DeviceConfigStore (firmware/components/payload_codec/include/pakt/DeviceConfigStore.h): header-only class; set_storage(IStorage*) attaches backend after construction; apply(ConfigFields&) updates in-memory DeviceConfig and calls storage_->save() if backend set; load() populates from storage on startup; in-memory always updated regardless of persist result. NvsStorage (firmware/main/NvsStorage.h): concrete IStorage using ESP-IDF NVS blob API (namespace "pakt_cfg", key "device_config", schema_version guard, nvs_commit() on every save). app_main() NVS boot path: nvs_flash_init() → erase+reinit if needed → set_storage + load() with explicit log for loaded/defaults/failure outcomes; tasks created after config is ready. Wired in main.cpp on_config_write: apply() called after PayloadValidator acceptance; logs persist success or in-memory-only warning. Wired on_config_read to `DeviceConfigStore::config_to_json(g_device_config.config(), ...)`. 7 host tests in test_host/test_config_store.cpp covering in-memory update, storage backend save, persist failure, load-without-storage defaults, and config_to_json behavior.
- Residual risk: full send→ack/timeout flow requires hardware (firmware TxScheduler wired into APRS task, radio TX, and ack detection). BLE TX result notify format must be confirmed against firmware implementation when hardware is available. NVS persistence for DeviceConfigStore requires hardware (flash + ESP-IDF NVS driver) — NvsStorage code is complete but untested on-device.

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

## Step 10 - MVP KISS interop
- IDs: INT-001, INT-003, INT-004, FW-018, APP-013, DOC-004
- Status: in_progress
- Exit: capability negotiation live, KISS-over-BLE implemented, and at least one reference KISS client or bridge validated
- Evidence (software done): DeviceCapabilities component (components/capability/): feature bitmask, JSON serialiser, mvp_defaults(), has() API; 16 host unit tests in test_capability.cpp. kDeviceCapabilities UUID (0xA0040000) added to BleUuids.h and now wired into BleServer GATT table as a read-only characteristic (2026-03-14): BleServer::Handlers gained on_caps_read callback, aprs_chars[] gained a BLE_GATT_CHR_F_READ entry for uuid_dev_caps, aprs_access_cb handles the read and falls back to `{}` if the handler is not set, main.cpp ble_task wires on_caps_read to DeviceCapabilities::mvp_defaults().to_json(). Python capability.py: DeviceCapabilities parser (protocol, fw_ver, hw_rev, features frozenset), CapabilityNegotiator (read on connect, assumed_mvp() fallback, feature flag API, on_caps callback with CAPS_WARN for missing MVP features, reset on disconnect); 28 pytest tests in test_capability.py. pakt_client.py reads capabilities on connect, exposes capabilities property, logs CAPS/CAPS_WARN.
- Evidence (software done 2026-03-16): KISS-over-BLE software path is now substantially implemented end-to-end. `KissFramer` is implemented and host-tested; KISS UUIDs and GATT service are present in `BleServer`; `handlers.on_kiss_tx` decodes inbound KISS and enqueues raw AX.25 into `AprsTaskContext`; the shared TX path uses `afsk_tx_frame()`; `aprs_task` drains decoded AX.25 frames from `g_rx_ax25_queue` and forwards them to both native `rx_packet` and KISS RX notifications; `notify_kiss_rx()` applies INT-002 chunking; desktop `kiss_bridge.py` handles multi-chunk RX reassembly. Capability negotiation exposes `kiss_ble`, and the Python side has KISS capability and bridge coverage.
- Remaining implementation required (hardware-gated):
  - Validate KISS TX and KISS RX with physical BLE + RF/audio hardware
  - Validate with third-party KISS client on hardware (APRSdroid, Direwolf, YAAC, Xastir)
  - Update DOC-004 interop matrix with real evidence
- Primary docs: `docs/16_kiss_over_ble_spec.md`, `docs/17_mvp_gap_analysis.md`, `docs/15_interoperability_matrix.md`.
- Residual risk: CapabilityNegotiator fallback to assumed_mvp() will be exercised until hardware validates the actual BLE read. KISS is now MVP-critical rather than deferred. Third-party client interoperability and BLE/RF behavior remain hardware-gated.

## Step 11 - HF discovery track
- IDs: HF-001..HF-011
- Status: todo
- Exit: go/no-go decision record for production HF audio bridge
- Evidence: latency/jitter/battery measurements and explicit decision rationale
- Note: This is a discovery track that can run alongside Steps 9-10 if a second agent is available, but must not block MVP milestone closure. A single agent should complete Steps 0-10 before starting Step 11.
