# MVP Gap Analysis — Firmware, Protocols, and KISS TNC

Date: 2026-03-20
Purpose: make the remaining implementation gaps explicit so an agent can begin work immediately.

## 1. Scope

This gap analysis assumes KISS-over-BLE is part of MVP.

The project is therefore not MVP-complete until:

- native PAKT BLE flows are live on hardware
- the existing KISS-over-BLE software path is validated on hardware
- at least one third-party KISS client or bridge is validated

## 2. Firmware gaps

### Critical P0 gaps (updated 2026-03-16)

1. **KISS service — software implementation complete; hardware-gated items remain**
- ✅ `firmware/components/kiss/KissFramer` implemented (encode/decode/escape/unescape)
- ✅ KISS UUIDs in BleUuids.h
- ✅ KISS GATT service in BleServer (RX notify + TX write characteristics)
- ✅ KISS TX write/reassembly path wired (BleChunker → on_kiss_tx handler → KissFramer::decode)
- ✅ KISS RX notify call site wired through `g_rx_ax25_queue` drain in `aprs_task`
- ✅ KISS TX real software path wired through shared `afsk_tx_frame()` path
- ⏳ Hardware validation still required for real BLE/RF/audio operation

2. **Capability record — software complete**
- ✅ `kiss_ble` feature added to `Feature` enum and `kMvpFeatures`
- ✅ `mvp_defaults()` now emits `kiss_ble` in JSON
- ✅ `capability.py` and `test_capability.py` aligned

3. **Shared TX path is software-complete, hardware-validated status still open**
- native APRS TX and KISS/raw AX.25 TX both use the same `afsk_tx_frame()` path in `firmware/main/main.cpp`
- completion still depends on SA818 + SGTL5000 validation on hardware

4. **RX frame path is software-wired but still hardware-gated**
- `audio_pipeline_run()` pushes decoded AX.25 into `g_rx_ax25_queue`
- `aprs_task` forwards decoded frames to both native `rx_packet` and KISS RX notifications
- real decode quality and stability still depend on hardware bring-up

### Important P1 gaps

5. **Device Command behavior remains limited**
- the characteristic exists but command semantics are still narrow
- additional bench-control and diagnostics commands may still be useful during bring-up

6. **GPS UART integration is not live**
- Parser exists
- task still contains UART placeholder code

7. **Power telemetry path is not live**
- schema exists
- `power_task` is still stubbed

8. **Audio modem path still needs hardware closure**
- I2S/codec driver is wired
- short APRS packet TX is now bench-proven on a separate receiver
- receive-side analog path is active and now has bench instrumentation for peak/flag/FCS/decode counters
- a PSRAM-backed RX WAV export path now exists for offline analysis, bench stages are now selectable through `firmware/main/bench_profile_config.h`, and the firmware now supports `16 kHz` codec/I2S debug mode plus configurable RX sample interpretation
- scope captures from the analog RX nodes and the saved WAV captures do not yet agree; current debug focus has shifted toward SGTL5000 input/gain assumptions, I2S RX interpretation, and sample-conditioning fidelity
- SGTL5000 capture-path closure, trusted Bell 202 source validation, and final on-device APRS RX proof remain gating work

## 3. Protocol gaps

### Resolved by this documentation update

1. KISS is now MVP, not post-MVP.
2. KISS service UUID layout is frozen as:
- service `0xA050`
- RX `0xA051`
- TX `0xA052`
3. KISS TX uses encrypted + bonded writes.
4. KISS and native PAKT BLE are non-modal and may coexist.
5. MVP logical KISS frame limit is `330` bytes after reassembly.

### Remaining implementation gaps

1. `05_ble_gatt_spec.md`, `16_kiss_over_ble_spec.md`, and code must stay aligned as hardware validation closes out remaining gaps.
2. `payload_contracts.md` covers JSON only; KISS binary behavior must remain documented in `16_kiss_over_ble_spec.md`.
3. Error accounting for malformed/oversize KISS frames must be implemented and surfaced consistently.

## 4. App and tooling gaps (updated 2026-03-19)

1. **Desktop KISS harness/bridge — software complete**
- ✅ `app/desktop_test/kiss_bridge.py`: KissPacketizer (serial framing), KissBridge (BLE bridge), chunking helpers
- ✅ `app/desktop_test/test_kiss_bridge.py`: bridge and chunking coverage updated, including multi-chunk RX reassembly
- ⏳ Bridge end-to-end test requires hardware BLE connection

2. **Third-party compatibility evidence missing**
- No validated YAAC/Xastir/APRSdroid/Direwolf evidence yet — requires hardware

3. **Interop matrix is still mostly expectation-level**
- Needs real tested entries once firmware path exists (hardware-gated)

## 5. Recommended implementation order (updated 2026-03-19)

Software-complete as of this revision:
1. ✅ KISS UUIDs added to `BleUuids.h`.
2. ✅ `Feature::KISS_BLE` added to `DeviceCapabilities.h/cpp`; `kMvpFeatures` includes it.
3. ✅ `firmware/components/kiss/KissFramer` — encode/decode/escape/unescape with host tests (37 tests, 89 assertions).
4. ✅ KISS GATT service added to `BleServer` (kiss_chars[], gatt_svcs[], kiss_access_cb, g_kiss_tx_chunker, notify_kiss_rx).
5. ✅ KISS TX write path wired in `main.cpp` on_kiss_tx handler and forwarded into the shared TX path.
6. ✅ `app/desktop_test/kiss_bridge.py` — KissPacketizer framing + KissBridge BLE bridge + chunking helpers.
7. ✅ `app/desktop_test/test_kiss_bridge.py` — 38 tests covering encode/decode/feed/chunking round-trips.
8. ✅ `capability.py` updated: `Feature.KISS_BLE = "kiss_ble"`, `MVP_REQUIRED` includes `kiss_ble`.
9. ✅ `test_capability.py` updated: 7 new KISS_BLE tests; pre-existing asyncio.run bug fixed.
10. ✅ CI `.github/workflows/ci.yml` updated to run `test_kiss_bridge.py`.

Hardware-gated remaining items:
- end-to-end KISS validation with a reference client (APRSdroid, YAAC, Xastir, Direwolf)
- BLE security validation on physical hardware
- SA818 + SGTL5000 electrical/audio validation under real RF conditions
- trusted Bell 202 source confirmation into the prototype RX path
- closure of the current SGTL5000/I2S/sample-capture mismatch seen between scope and saved WAVs
- on-device APRS RX decode proof
- final confirmation that the new `16-bit` Stage C recorder captures clean Bell 202 at the demod input

## 6. Agent-ready next tasks

- Validate native + KISS coexistence under reconnect, chunk loss, and malformed input cases on hardware.
- Measure SA818 TX deviation and record the actual AF attenuation settings used on the bench harness.
- Continue SGTL5000/I2S/capture-path debugging using the new `16 kHz` recorder mode and sample-interpretation controls.
- Re-run APRS RX with a trusted Bell 202 source and close the receive-margin question.
- Run hardware bring-up follow-up for BLE security, SGTL5000 MCLK stability, and PTT safety.
- Close remaining hardware-gated RF/audio/GPS/power items.
- Update interoperability matrix (`docs/15_interoperability_matrix.md`) with real validated clients once hardware is available.
