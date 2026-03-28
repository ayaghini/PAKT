# MVP Gate Pass Matrix

Generated: 2026-03-27
Agent step: Step 9+ prototype radio-audio follow-up, quiet-profile RX capture fix, and on-device APRS RX proof

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
| Host unit tests pass in CI | **pass** | `host-tests` CI job: CMake+Ninja, 364/364 tests pass. AFSK round-trip fixed (pass 3): ones_count 6→7, biquad Q 3.5→1.5. TX buffer sizing tests added (pass 4–5): 6 new tests; `modulate_frame()` truncation signaling corrected (pass 5): inline before-write check correctly distinguishes truncation from exact-fit success. |
| Python app tests pass in CI | **pass** | `app-tests` CI job: pytest, test_chunker + test_app + test_messaging + test_telemetry_app |
| No new critical warnings in touched modules | **pass** | `-Wall -Wextra -Wpedantic` in host tests; ESP-IDF build clean |
| Firmware flashes and boots on target | **blocked** | Dependency: prototype hardware (ESP32-S3 board) not yet available |

**G0 assessment:** All software checks pass. Host test binary status recorded here is 364/364 after the pass 5 `AfskModulator` truncation fix. Hardware flash/boot check remains blocked pending prototype.

---

## Gate G1 — Functional MVP

| Check | Status | Notes |
|---|---|---|
| Beacon TX decodes on known-good receiver | **partial** | Bench evidence is now stronger: reported 2026-03-19 packet TX pass was followed by a fresh 2026-03-21 re-validation where SA818 handshake/config/PTT recovered on hardware, the 10-tone TX stage was heard, and APRS packets from the current prototype were again received externally. Deviation measurement and repeatability are still open before promoting this to a fully closed gate item. |
| SGTL5000 SYS_MCLK + sample-rate stable across reconnect | **partial** | Reported 2026-03-19 bench pass indicates SGTL5000 audio path is live enough for 10-tone TX and RX signal-presence monitoring on hardware. Reconnect/long-run sample-rate stability is still unverified. Dependency: FW-004 Step 2 follow-up. |
| RX decode stream reaches app | **pass** | `audio_task` now wired: SGTL5000 I2C init + I2S RX channel + `AfskDemodulator` instantiated; decoded frames pushed to `g_rx_ax25_queue`; `aprs_task` already drains queue and forwards to BLE notify + KISS RX. On 2026-03-27, quiet-profile hardware testing found and fixed a half-rate I2S unpack bug in the RX path and reduced Stage C SA818 volume to avoid clipping. After the fix, the prototype decoded valid on-air APRS/AX.25 frames on-device multiple times during a single `30 s` capture window. |
| Message send/ack/timeout states correct | **partial** | TxScheduler FSM + MessageTracker fully tested in software (26+37 tests). End-to-end ack flow requires hardware. |
| KISS-over-BLE exchanges frames with reference client | **partial** | KISS software stack complete end-to-end (2026-03-16 pass 2–4): KissFramer (37 host tests), KISS GATT service in BleServer, KISS TX decoded+enqueued into AprsTaskContext raw-AX.25 ring (35 host tests), KISS RX drain loop wired in aprs_task (notifies both native rx_packet and KISS RX clients), notify_kiss_rx sends proper INT-002 chunks, desktop kiss_bridge.py reassembles multi-chunk KISS RX frames (50 Python tests). Real AFSK TX path now wired (pass 4): RadioTxFn and RawTxFn stubs replaced with `afsk_tx_frame()` (Sa818Radio.ptt + AfskModulator + I2S write + PTT release). Hardware-gated: real audio RX pipeline (Ax25RxQueue producer blocked until SGTL5000 hardware present), third-party client validation (APRSdroid/Direwolf/YAAC). Dependency: HW-010 |
| Controlled-condition beacon decode ≥ 95% | **blocked** | On-device APRS RX proof now exists, but controlled-condition decode-rate validation and calibrated repeatability are still unverified. Dependency: FW-006, FW-004, HW-010 |

**G1 assessment:** Hardware progress is now materially stronger: bench work now shows SA818 recovery on live hardware, supervised RF tone TX, refreshed packet-level APRS TX reception on a separate receiver, and on-device APRS RX decode after fixing the half-rate quiet-profile I2S unpack bug and reducing Stage C volume. MVP functional proof is still open because calibrated deviation, controlled-condition repeatability, and end-to-end message-flow validation remain unverified.

---

## Gate G2 — BLE reliability

| Check | Status | Notes |
|---|---|---|
| Desktop harness connect/reconnect and log export | **partial** | BleTransport FSM (3-attempt reconnect), config R/W, log export fully implemented and tested (45 tests). Hardware BLE validation blocked. |
| 1 hour continuous RX BLE session stable | **blocked** | Requires hardware. Dependency: QA-004 (BLE endurance matrix) |
| Reconnect works after link drop and device reboot | **partial** | BleTransport reconnect FSM implemented (RECONNECTING→CONNECTED/ERROR with on_reconnected callback, re-subscription). Validated in test_app.py mocks. Real link drop testing blocked pending hardware. |
| Chunk reassembly handles loss/duplicates/timeouts | **pass** | BleChunker (C++) and chunker.py (Python) both handle duplicate chunks, LRU slot eviction, and timeouts. 18 C++ + 25 Python tests pass. |
| Native BLE and KISS-over-BLE coexist | **partial** | KISS service added alongside existing APRS/Telemetry services in gatt_svcs[]; disconnect resets all chunkers including kiss_tx. Coexistence verified in code; hardware BLE endurance test still blocked. Dependency: HW-010 |

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
| Bell 202 demodulator timing/level calibration on real audio path | Medium | Half-rate I2S unpack bug is now fixed and on-device decode exists. Remaining work is margin and repeatability tuning rather than first-principles decode bring-up. | Firmware / Step 2+3 hardware bring-up |
| SGTL5000 I2S MCLK stability under thermal stress | High | sdkconfig.defaults pins I2S config; bench test with underrun/overrun counters at Step 2 | Firmware / Step 2 |
| SA818 audio deviation calibration (TX deviation) | High | Reported tone-sequence TX bench pass reduces uncertainty in the analog path, but calibrated deviation measurement is still the top open RF risk before APRS TX can be trusted. | RF / Step 1 hardware |
| BleServer encrypted+bonded write rejection (G3 hardware) | High | Firmware implementation complete; P0 priority for first hardware bring-up session | Firmware / Step 4 hardware |
| PTT stuck-on under BLE fault or task crash | High | FW-016 software-complete: PttWatchdog + PttController + watchdog_task wired (21 host tests). Sa818Radio (FW-003) software-complete: 18 host tests; radio_task registers direct-GPIO watchdog callback before init then upgrades to radio.ptt(false) after init. Hardware fault injection test remains blocked. | Firmware / G3 hardware bring-up |
| TxScheduler wired into APRS task (radio TX stub) | Low | TxScheduler IS integrated via AprsTaskContext in aprs_task (ctx.tick()), 26 host tests pass. RadioTxFn and RawTxFn stubs replaced (pass 4) with real `afsk_tx_frame()` (Sa818Radio.ptt + AfskModulator + I2S write). Reported 2026-03-19 bench pass indicates the underlying supervised tone-TX path is alive on hardware; APRS packet transmission still needs deviation and decode validation. | Firmware / Step 7 hardware |
| TX result notify wire format not confirmed against firmware | Medium | Python MessageTracker parses `{"msg_id":"...","status":"..."}` — must verify against actual BleServer notify on hardware | Firmware+App / Step 7 hardware |
| GPS parser (FW-005) software done; UART integration blocked | Medium | NmeaParser implemented (GPRMC/GPGGA, checksum, Unix timestamp, stale-fix); 37 host tests pass. gps_task UART stub must be replaced with real driver on hardware before GPS fields are live. | Firmware / Step 4b hardware bring-up |

---

## Summary

**Software-complete:** Steps 0, 1(FW-016 + FW-003 SA818 driver software), 2 (audio pipeline wired: SGTL5000 + I2S full-duplex + AfskDemodulator → Ax25RxQueue; clocking fixed pass 4), 3, 4, 4b, 5, 6, 7, 8, 10 (all software portions)
**AFSK modem fixed:** 364/364 host tests pass (2026-03-16 pass 5). Demodulator bugs fixed (pass 3). TX buffer sizing tests added (pass 4–5). `AfskModulator::modulate_frame()` truncation signaling corrected (pass 5): inline before-write check, exact-fit success confirmed possible and handled correctly.
**TX path wired (pass 4):** `afsk_tx_frame()` implemented in main.cpp — Sa818Radio.ptt + AfskModulator + I2S write. RadioTxFn and RawTxFn stubs replaced. SGTL5000/I2S clocking mismatch fixed (MCLK 2.048 MHz → 8.192 MHz). MUTE_LO cleared. I2S_IN→DAC routing fixed.
**Hardware-progressed but still open:** Step 1 now has reported SA818 UART/PTT plus supervised RF tone-TX and refreshed APRS packet-TX evidence on the current prototype state, and Step 2 now has confirmed live codec/radio audio-path evidence plus successful on-device APRS RX decode on the corrected quiet profile.
**Hardware-blocked:** Calibrated deviation measurement, controlled-condition RX repeatability, full end-to-end validation of Steps 3–8, G1 repeatability items, G2(endurance), G3(hardware), G4; third-party KISS client validation
**MVP milestone gate:** OPEN — G0(software) passes; G0(hardware), G1, G2(KISS coexistence hardware), G3, G4 blocked pending EVT prototype

**Next unblocking action:** Prototype hardware follow-up on the radio/audio path. Priority order for the next bench session:
1. Measure TX deviation against a calibrated receiver or service monitor
2. Measure decode repeatability on the corrected quiet profile across multiple runs and sources
3. Measure TX deviation and record the now-known-good RX settings in the bench docs
4. If needed, tune RX analog margin and demod thresholds for better decode margin, not first decode
5. Check SGTL5000/I2S reconnect and long-run clock stability
6. Resume G3 critical-path checks: BLE bonded-write rejection and PTT safe-off fault test
