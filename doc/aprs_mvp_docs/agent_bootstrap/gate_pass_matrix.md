# MVP Gate Pass Matrix

Generated: 2026-03-09
Agent step: Step 9 (QA-003, QA-004, QA-006, DOC-001, DOC-003)

Legend:
- `pass`    — verified in software / CI; no hardware required
- `blocked` — requires prototype hardware; recorded with exact dependency
- `partial` — software portion verified; hardware portion blocked
- `n/a`     — gate item not applicable at this stage

---

## Gate G0 — Build/Test hygiene (per-change gate)

| Check | Status | Notes |
|---|---|---|
| Firmware builds in CI | **pass** | `firmware-build` CI job: ESP-IDF v5.3.2, target esp32s3, `idf.py build` |
| Host unit tests pass in CI | **pass** | `host-tests` CI job: CMake+Ninja, 200+ tests across ax25/aprs/modem/ble_chunker/tx_scheduler/telemetry/gps/payload_validator/tx_integration/ptt_watchdog/config_store |
| Python app tests pass in CI | **pass** | `app-tests` CI job: pytest, test_chunker + test_app + test_messaging + test_telemetry_app |
| No new critical warnings in touched modules | **pass** | `-Wall -Wextra -Wpedantic` in host tests; ESP-IDF build clean |
| Firmware flashes and boots on target | **blocked** | Dependency: prototype hardware (ESP32-S3 board) not yet available |

**G0 assessment:** Software checks pass. Hardware flash/boot check blocked pending prototype.

---

## Gate G1 — Functional MVP

| Check | Status | Notes |
|---|---|---|
| Beacon TX decodes on known-good receiver | **blocked** | Requires SA818 + antenna + reference TNC. Dependency: HW-010 (EVT build) |
| SGTL5000 SYS_MCLK + sample-rate stable across reconnect | **blocked** | Requires SGTL5000 codec on hardware. Dependency: FW-004 (I2S driver, Step 2) |
| RX decode stream reaches app | **blocked** | Requires hardware RX path. Dependency: FW-006, FW-004 |
| Message send/ack/timeout states correct | **partial** | TxScheduler FSM + MessageTracker fully tested in software (26+37 tests). End-to-end ack flow requires hardware. |
| Controlled-condition beacon decode ≥ 95% | **blocked** | Requires hardware RX pipeline and corpus test rig. Dependency: FW-006, FW-004, HW-010 |

**G1 assessment:** All functional checks blocked pending hardware. Software foundations (modem, AX.25, APRS, BLE, TX FSM) fully implemented and unit-tested.

---

## Gate G2 — BLE reliability

| Check | Status | Notes |
|---|---|---|
| Desktop harness connect/reconnect and log export | **partial** | BleTransport FSM (3-attempt reconnect), config R/W, log export fully implemented and tested (45 tests). Hardware BLE validation blocked. |
| 1 hour continuous RX BLE session stable | **blocked** | Requires hardware. Dependency: QA-004 (BLE endurance matrix) |
| Reconnect works after link drop and device reboot | **partial** | BleTransport reconnect FSM implemented (RECONNECTING→CONNECTED/ERROR with on_reconnected callback, re-subscription). Validated in test_app.py mocks. Real link drop testing blocked pending hardware. |
| Chunk reassembly handles loss/duplicates/timeouts | **pass** | BleChunker (C++) and chunker.py (Python) both handle duplicate chunks, LRU slot eviction, and timeouts. 18 C++ + 25 Python tests pass. |

**G2 assessment:** Chunker robustness and transport FSM fully tested in software. Hardware endurance runs blocked.

---

## Gate G3 — Security and safety

| Check | Status | Notes |
|---|---|---|
| Write endpoints blocked when not encrypted+bonded | **partial** | BleServer enforces `ble_gap_conn_find()` → `sec_state.encrypted && sec_state.bonded` before config/command/TX writes; returns `BLE_ATT_ERR_INSUFFICIENT_AUTHEN`. App classifies auth errors via `is_auth_error()` (8 tests). Hardware GATT validation blocked. |
| Pairing window and bond-reset flow validated | **partial** | Desktop app `_print_pairing_help()` guides user through OS Bluetooth → Remove Device → reconnect flow. Firmware LE SC (`sm_sc=1`, `sm_bonding=1`, `sm_io_cap=BLE_SM_IO_CAP_NO_IO`) configured. In-device pairing window validation requires hardware. |
| Fault tests always return to PTT=off | **partial** | FW-016 PttWatchdog software-complete: 21 host tests, wired into `watchdog_task` (priority 6). PttController safe-off callback registered by `radio_task` on boot (direct GPIO before init, upgraded to `radio.ptt(false)` after successful SA818 init). Hardware fault injection (power removal, BLE drop during TX) requires HW-010. |

**G3 assessment:** BLE security policy fully implemented in firmware and app. FW-016 watchdog software-complete (PttWatchdog + PttController). Hardware validation of bonded-write rejection and PTT fault injection still requires HW-010.
**Note:** G3 failure would block further TX-capable changes; this check is flagged as a P0 priority for first hardware bring-up.

---

## Gate G4 — Power and field sanity

| Check | Status | Notes |
|---|---|---|
| No reset loop under repeated TX bursts | **blocked** | Requires hardware. Dependency: FW-016, HW-010 |
| Battery telemetry sane and monotonic under load | **blocked** | Requires MAX17048 fuel gauge and battery. Dependency: HW-004, HW-011 |
| Field test script completed with issue log | **blocked** | Requires assembled EVT unit. Dependency: QA-005, HW-010 |

**G4 assessment:** All G4 checks blocked pending hardware.

---

## Regression suite (QA-006)

The CI pipeline constitutes the regression suite for firmware releases:

| Job | Trigger | Contents |
|---|---|---|
| `firmware-build` | every push/PR | Full ESP-IDF idf.py build (ESP32-S3, v5.3.2) |
| `host-tests` | every push/PR | CMake host unit tests — ax25, aprs, afsk modem, BLE chunker, TX scheduler, telemetry serialisers |
| `app-tests` | every push/PR | pytest — chunker, BLE transport FSM, config store, message tracker, telemetry parsers, diagnostics |

**QA-006 assessment:** CI regression gate is implemented and enforced on every PR and push to `main`. Release gate = all three CI jobs green.

---

## Residual risks and mitigation owners

| Risk | Severity | Mitigation | Owner/When |
|---|---|---|---|
| Bell 202 demodulator timing/level calibration on real audio path | High | Transition-tracking sync implemented; test with known-corpus audio before EVT sign-off | Firmware / Step 2+3 hardware bring-up |
| SGTL5000 I2S MCLK stability under thermal stress | High | sdkconfig.defaults pins I2S config; bench test with underrun/overrun counters at Step 2 | Firmware / Step 2 |
| SA818 audio deviation calibration (TX deviation) | High | Identified as top risk; needs bench measurement with reference calibrated TNC | RF / Step 1 hardware |
| BleServer encrypted+bonded write rejection (G3 hardware) | High | Firmware implementation complete; P0 priority for first hardware bring-up session | Firmware / Step 4 hardware |
| PTT stuck-on under BLE fault or task crash | High | FW-016 software-complete: PttWatchdog + PttController + watchdog_task wired (21 host tests). Sa818Radio (FW-003) software-complete: 18 host tests; radio_task registers direct-GPIO watchdog callback before init then upgrades to radio.ptt(false) after init. Hardware fault injection test remains blocked. | Firmware / G3 hardware bring-up |
| TxScheduler wired into APRS task (radio TX stub) | Low | TxScheduler IS integrated via AprsTaskContext in aprs_task (ctx.tick()), 26 host tests pass. RadioTxFn is a stub (returns true without real AX.25/AFSK TX). Real TX path wired when SA818+audio drivers are ready. | Firmware / Step 7 hardware |
| TX result notify wire format not confirmed against firmware | Medium | Python MessageTracker parses `{"msg_id":"...","status":"..."}` — must verify against actual BleServer notify on hardware | Firmware+App / Step 7 hardware |
| GPS parser (FW-005) software done; UART integration blocked | Medium | NmeaParser implemented (GPRMC/GPGGA, checksum, Unix timestamp, stale-fix); 37 host tests pass. gps_task UART stub must be replaced with real driver on hardware before GPS fields are live. | Firmware / Step 4b hardware bring-up |

---

## Summary

**Software-complete:** Steps 0, 1(FW-016 + FW-003 SA818 driver software), 3, 4, 4b, 5, 6, 7, 8, 10 (all software portions)
**Hardware-blocked:** Steps 1(SA818 electrical validation), 2 (audio pipeline), full end-to-end validation of Steps 3–8, G1, G2(endurance), G3(hardware), G4
**MVP milestone gate:** OPEN — G0(software) passes; G0(hardware), G1, G3, G4 blocked pending EVT prototype

**Next unblocking action:** Prototype hardware bring-up (HW-010). Priority order for first bring-up session:
1. Flash/boot check (G0)
2. BLE connect + bonded-write rejection (G3 critical path)
3. PTT safe-off fault test (G3 critical path)
4. SA818 UART and PTT validation (Step 1 / FW-003)
5. SGTL5000 I2S MCLK stability (Step 2 / FW-004)
