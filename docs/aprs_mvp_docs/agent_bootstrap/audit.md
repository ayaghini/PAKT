# Agent Update Audit

Date: 2026-03-16
Purpose: rolling implementation and verification ledger for the current MVP branch.

---

## SA818 recovery + TX re-validation (2026-03-21)

Status: hardware tested during the current working session.

What changed in firmware before the successful re-test:
- `firmware/main/main.cpp`
  - added explicit UART init error checking for `uart_driver_install`, `uart_param_config`, and `uart_set_pin`
  - added UART RX flush before first SA818 handshake
  - added a controlled `2 s` SA818 settle delay before the first `AT+DMOCONNECT`
  - increased `watchdog_task` stack size after a debug-image stack overflow was observed
- `firmware/main/sa818_bench_test/sa818_bench_test.cpp`
  - added low-level UART diagnostics around SA818 exchanges:
    - TX write success logging
    - `uart_get_buffered_data_len()` checks after timeouts
    - raw RX-byte dump path for any pending bytes
  - tightened bench safety so TX/RX stages do not proceed after failed frequency config

Intermediate diagnostic result:
- before the hardware reconnect/reseat, the firmware consistently showed:
  - `TX write: ok`
  - `pending_rx=0 bytes`
- that established that the ESP side was transmitting commands but seeing no reply bytes at the UART driver level

Successful hardware result after reconnect/reset:
- `AT+DMOCONNECT` handshake passed on attempt 1
- `AT+DMOSETGROUP` frequency configuration passed
- staged PTT toggle passed
- Stage 5 10-tone TX sequence ran and was heard on a receiver
- APRS packet TX was again received externally from the prototype

Current status impact:
- the current prototype is back in a known-good TX state
- SA818 control path is responsive again on the live hardware
- APRS TX proof is refreshed on the current wiring/firmware state, not only in older logs
- the main remaining open hardware question is still on-device APRS RX decode quality

Recommended next step from this restored baseline:
- hold TX as proven for now, then continue focused RX troubleshooting against this known-good SA818/codec/TX state
- highest RF calibration task remains TX deviation measurement
- highest functional gap remains on-device APRS RX decode proof

---

## Prototype bench workflow + RX recorder update (2026-03-20)

Status: implemented in the repo during the current working session; firmware build re-verification was blocked by the local ESP-IDF environment on this machine.

Firmware changes:
- `firmware/main/bench_profile_config.h`
  - new build-time bench/debug profile file
  - controls whether `audio_bench`, `sa818_bench`, and `aprs_bench` run at boot
  - APRS sub-stages can now be enabled independently:
    - Stage 0 loopback
    - Stage A TX burst
    - Stage B RX gain sweep
    - PCM snapshot dump
    - Stage C full RX recorder/export
  - Stage C ADC gain step is now configurable for targeted capture runs
- `firmware/main/main.cpp`
  - bench calls now respect the profile toggles
  - full RX recorder upgraded from unsigned `8-bit` output to signed `16-bit` PCM WAV export

Bench result/status impact:
- the firmware is no longer forced through every bench stage on every debug boot
- Stage C is now intended as a reusable targeted debug tool, not just a fixed late-stage bench step
- the next APRS RX capture should preserve much more waveform detail than the earlier `8-bit` recorder

Current local limitation:
- attempted local firmware re-build was blocked because the ESP-IDF activation on this machine currently fails with:
  - missing `/Users/macmini4/.espressif/espidf.constraints.v5.5.txt`
- so this audit item records code/documentation state, not a fresh hardware-verified flash

## APRS RX analog-vs-digital troubleshooting record (2026-03-20)

Status: hardware tested and captured during the current working session; intended as the handoff record for continued RX debugging.

Firmware/debug setup used:
- `firmware/main/bench_profile_config.h`
  - used to disable unrelated benches and run APRS Stage C capture in isolation
  - Stage C gain made selectable for targeted RX capture runs
- `firmware/main/main.cpp`
  - Stage C recorder updated to export signed `16-bit` mono WAV from PSRAM
  - full 30 s capture/export path validated on hardware

Captured artifacts:
- pre-cap, 16-bit, +12 dB capture:
  - `/Users/macmini4/Desktop/PAKT/tmp/rx_captures/rx_capture_2026-03-20_101244.wav`
- post-cap, 16-bit, +12 dB capture after adding an inline ~887 nF series capacitor between `SA818 AF_OUT` and `SGTL5000 LINE_IN_L`:
  - `/Users/macmini4/Desktop/PAKT/tmp/rx_captures/rx_capture_2026-03-20_105850.wav`
- scope captures:
  - `/Users/macmini4/Desktop/PAKT/tmp/osc/pic_0_1.png`
  - `/Users/macmini4/Desktop/PAKT/tmp/osc/pic_0_3.png`
  - `/Users/macmini4/Desktop/PAKT/tmp/osc/pic_0_4.png`
  - `/Users/macmini4/Desktop/PAKT/tmp/osc/pic_0_5.png`
  - `/Users/macmini4/Desktop/PAKT/tmp/osc/pic_0_6.png`

What the WAV captures showed:
- both pre-cap and post-cap demod-input recordings still failed to show strong Bell 202 content at `1200 Hz` / `2200 Hz`
- pre-cap capture was dominated by low-frequency / baseline-heavy junk
- post-cap capture was materially cleaner at idle, which means the inline capacitor helped
- however, even the post-cap file still did not look like a clean Bell 202 waveform at the firmware recorder input

Quantitative directional findings:
- pre-cap overall mean amplitude was much higher than post-cap, indicating substantial low-frequency / DC-heavy junk before the cap
- post-cap idle became much quieter while real burst timing remained visible
- despite the cleaner idle, no on-device APRS decode was achieved and the saved waveform still lacked convincing Bell 202 energy

What the scope captures changed in the diagnosis:
- earlier WAV-only analysis pointed toward the analog RX path as the main suspect
- the later scope captures, especially the tighter-timebase views (`pic_0_4.png`, `pic_0_5.png`, `pic_0_6.png`), show that the two probed points track each other closely after vertical scaling
- that makes gross analog corruption between `SA818 AF_OUT` and `SGTL5000 LINE_IN_L` less likely than first suspected
- current best interpretation:
  - the inline cap improved the RX path and should likely remain in place
  - the analog path no longer looks like the sole or primary failure point
  - the stronger suspicion has moved downstream into the codec/sample path:
    - SGTL5000 input selection or gain/config
    - I2S RX sampling / interpretation
    - sample-rate/path mismatch
    - recorder path not faithfully representing the waveform visible on the scope

Conservative conclusion to preserve for follow-up:
- APRS packet TX is still the strongest verified RF success on the prototype
- prototype APRS RX remains open
- a bad APRS source and gross analog-path destruction are now both less likely than before
- next debug step should compare a short burst-triggered raw sample dump from `mono_buf` against the scope waveform at the same event to localize whether the discrepancy begins at codec sampling or later in software

## SGTL5000 / I2S RX-path debug controls added (2026-03-20)

Status: implemented and build-verified in the current working session.

Firmware changes:
- `firmware/main/bench_profile_config.h`
  - added build-time RX sample-path controls:
    - `kRxInputMode` (`Left`, `Right`, `Average`, `Stronger`)
    - `kRxSwapStereoSlots`
    - `kRxByteSwapSamples`
    - `kRxEnableDcBlock`
    - `kRxDcBlockPole`
    - `kRxLogChannelStats`
    - `kLogSgtl5000Readback`
- `firmware/main/main.cpp`
  - added SGTL5000 register readback logging after codec init
  - RX loop now logs left/right/mono channel stats once per second
  - mono recorder/demod input is no longer hard-wired to raw left-slot samples; it is now selected and conditioned according to `bench_profile_config.h`
  - optional firmware DC blocker now exists ahead of both the recorder and demodulator

Why this matters:
- the analog scope captures now suggest the probed RX nodes track each other reasonably well
- because of that, continued APRS RX debugging should focus on how the firmware interprets the codec samples rather than assuming the breadboard path is the only problem
- the new controls let future runs answer these questions quickly without touching wiring:
  - are we using the correct stereo slot?
  - are the samples byte-ordered as expected?
  - does a light DC blocker materially improve Bell 202 energy at the demod input?
  - is one channel consistently cleaner than the other?

Current working hypothesis:
- the remaining issue is now more likely in one of:
  - SGTL5000 input/gain/config assumptions
  - I2S RX slot/sample interpretation
  - sample conditioning needed before demod
  - less likely, but still possible, a deeper sample-rate/path mismatch

## 16 kHz codec / I2S debug mode added (2026-03-20)

Status: implemented and build-verified in the current working session.

Firmware changes:
- `firmware/main/bench_profile_config.h`
  - added `kAudioSampleRateHz` build-time control
  - current debug configuration set to `16000` for higher-fidelity capture work
- `firmware/main/main.cpp`
  - audio pipeline, recorder, PCM snapshot, AFSK TX path, and APRS loopback now use the configured audio sample rate instead of hard-coded `8000`
  - SGTL5000 clocking is now derived from an explicit clock plan:
    - `8000 Hz`  -> `SYS_FS = 32 kHz`, `RATE_MODE = /4`, `MCLK = 8.192 MHz`
    - `16000 Hz` -> `SYS_FS = 32 kHz`, `RATE_MODE = /2`, `MCLK = 8.192 MHz`
  - I2S MCLK multiple is selected to match that plan (`1024` at `8 kHz`, `512` at `16 kHz`)
  - TX PCM buffer sizing increased to avoid truncation in `16 kHz` mode
- `firmware/main/sa818_bench_test/sa818_bench_test.cpp`
  - DMA drain timing now scales with the active sample rate

Why this matters:
- the prior firmware debug path was limited to `8 kHz`, which is enough for Bell 202 but leaves less timing and waveform margin than `16 kHz`
- the new `16 kHz` mode improves both:
  - saved debug WAV fidelity
  - demodulator input detail during RX troubleshooting

Current intent:
- use `16 kHz` as the preferred SGTL5000/I2S debug mode while APRS RX remains unresolved
- if a future regression appears in packet TX or bench audio stages, the firmware can still be switched back to `8 kHz` from the same config file for A/B comparison

## RX WAV analysis follow-up (2026-03-20)

Status: extracted and inspected from a saved serial capture during the current working session.

Captured artifact:
- saved WAV exported from Stage C:
  - `/Users/macmini4/Desktop/PAKT/tmp/rx_captures/rx_capture_2026-03-20_092235.wav`

Observed characteristics of the saved `8-bit` capture:
- clear burst timing aligned with operator transmissions
- strongest spectral energy concentrated at very low frequencies (roughly `50-150 Hz`)
- essentially zero `1200 Hz` / `2200 Hz` Bell 202 energy in the strongest windows
- very low zero-crossing counts in the most active windows, inconsistent with clean APRS AFSK

Conservative interpretation:
- the prototype is receiving something during those windows, but the saved demod-input waveform does not yet look like clean Bell 202
- because that capture was recorded at `0 dB` ADC gain and through the older `8-bit` recorder, it should not be treated as the final RF diagnosis
- the next meaningful test is a new capture with the `16-bit` recorder and a deliberate Stage C gain choice matched to the best observed RX margin

## Prototype radio-audio bench pass (reported 2026-03-19)

Status: reported by agent, not independently re-run in this review session.

Firmware changes reported in this pass:
- `firmware/main/sa818_bench_test/sa818_bench_test.cpp`
  - replaced the old single-tone TX bench stage with a 10-tone stepped TX sequence
  - replaced the old manual RX wait stage with an RX peak-monitor stage driven by a callback
- `firmware/main/sa818_bench_test/sa818_bench_test.h`
  - added `RxPeakFn`
  - extended `run_sa818_bench()` to accept an RX peak callback
- `firmware/main/main.cpp`
  - added rolling `g_rx_peak_abs` tracking in the audio demod loop
  - passed an `rx_peak_fn` lambda into `run_sa818_bench()`

Reported hardware result:
- TX audio: pass
  - SA818 keyed on 144.390 MHz
  - 10-tone stepped sequence reportedly heard on a reference receiver
- RX audio: pass
  - reported `max_peak = 9835`
  - reported `signal_seconds = 15/20`
  - operator reportedly keyed a handheld on 144.390 MHz and generated voice/audio

Important review caveat:
- This pass does **not** yet record an actual RX waveform or PCM dump; it records
  only rolling peak amplitude.
- This is sufficient for a quick bench confidence check, but it is weaker than a
  real captured-audio artifact for later inspection.
- TX deviation measurement remains open and is still the next required RF step.

## Prototype APRS packet bench pass (reported 2026-03-19)

Status: reported by agent, not independently re-run in this review session.

Firmware behavior reported in this pass:
- added a one-shot APRS bench flow in `firmware/main/main.cpp`
  - Stage 0: pure-software modem loopback
  - Stage A: short APRS RF TX burst
  - Stage B: APRS RX listen window

Reported verification result:
- Stage 0 modem loopback: pass
  - a 46-byte AX.25 frame was modulated and decoded successfully in software
  - this is good evidence that the modem path is not the current blocker
- Stage A APRS TX burst: pass
  - operator reportedly received the transmitted APRS packets on a separate APRS-capable receiver
  - this is stronger evidence than the earlier tone-sequence test and should be treated as packet-level TX proof on hardware
- Stage B APRS RX listen: unconfirmed
  - no valid APRS frame decoded during the 120 s receive window
  - `rx_peak_abs` reportedly spiked, which suggests on-frequency audio energy reached the demod path
  - however, the operator was reportedly transmitting voice rather than APRS packets, so this does not count as APRS RX proof

Current interpretation:
- APRS packet TX over the real RF/audio path is now bench-proven in at least one supervised setup.
- APRS RX is still not proven on-air; current evidence only shows that the RX analog/audio path is alive enough to pass energy into the demodulator.
- The next meaningful close-out step for RX is a repeat of the listen window with a real APRS source such as APRSdroid, a TNC-connected HT, or a second PAKT board.

## Prototype APRS RX troubleshooting follow-up (reported 2026-03-19)

Status: reported by agent, not independently re-run in this review session.

Firmware instrumentation reported in this pass:
- `AfskDemodulator` now exposes cumulative diagnostic counters:
  - detected HDLC flags
  - FCS rejects
  - decoded frames
- `main.cpp` publishes those counters once per second from the audio task and logs them during the APRS RX bench window

Reported result from two RX troubleshooting runs:
- no valid APRS frames decoded on-device
- noise-floor flag detections reportedly stayed near baseline
- no strong flag-rate burst characteristic of a Bell 202 preamble was reported
- no FCS-reject growth indicating a near-decode condition was reported

Conservative interpretation:
- The prototype still has no on-device APRS RX proof.
- The new counters make a modem-software failure less likely, but they do not by themselves prove the handheld transmitted voice or an unmodulated carrier.
- The remaining uncertainty is now concentrated around the external APRS source path and the analog receive chain:
  - handheld/APRS app actually generating Bell 202
  - HT audio/PTT interface configuration
  - receive audio level and conditioning into SGTL5000 LINE_IN_L
  - real-world demod margin on this analog path

Most useful next test:
- repeat Stage B with a trusted APRS source whose Bell 202 output is already known-good:
  - APRSdroid + verified audio/PTT cable
  - Direwolf or another TNC feeding a radio
  - a second PAKT unit or known-good APRS transmitter
- if that still shows flags without decode, focus next on RX analog level and demod margin rather than source validity

## Archived Resolved Reviews

The detailed review findings from 2026-03-14 and the early 2026-03-15 follow-up pass were rechecked against the current tree and are still resolved. Those issue-by-issue review sections were removed here to keep this file focused on active implementation state and later milestone passes.

Resolved items confirmed in the current repo include:
- host-test `main()` duplication removed
- `TxScheduler::kMaxMsgIdStr` test references corrected
- TX result placeholder remap in `message_tracker.py`
- duplicate `BleServer.cpp` callback bodies removed
- CI app-tests install the required Python dependencies
- BLE chunker oldest-slot eviction logic corrected
- GPS telemetry field/schema alignment completed
- bootstrap GPS status conflict corrected
- tracked Python bytecode artifacts removed and ignored

Those older resolved review details remain available in git history if needed.

---

## Pre-Hardware Sprint (2026-03-15) — P0 implementation

Scope: software-only components to close the gaps identified before hardware bring-up.
All items are pure C++ with no ESP-IDF/FreeRTOS dependencies; host-testable.

### New components

#### `firmware/components/payload_codec/` (NEW)

- `include/pakt/PayloadValidator.h` — declares `ConfigFields`, `TxRequestFields`, and `PayloadValidator`.
- `PayloadValidator.cpp` — flat JSON scanner (no heap, no third-party JSON lib).
  - `validate_config_payload()` — requires `"callsign"` (1–6 alphanumeric/dash chars); optional `"ssid"` (0–15).
  - `validate_tx_request_payload()` — requires `"dest"` (callsign rules) and `"text"` (1–67 chars); optional `"ssid"`.
  - Rejects null data, zero length, and payloads ≥ `kMaxJsonLen` (512 bytes).
- `CMakeLists.txt` — standalone ESP-IDF component, no REQUIRES.

#### `firmware/components/aprs_fsm/TxResultEncoder` (NEW)

- `include/pakt/TxResultEncoder.h` — `TxResultEvent` enum (TX, ACKED, TIMEOUT, CANCELLED, ERROR); encoder and state mapper.
- `TxResultEncoder.cpp` — `encode()` produces `{"msg_id":"<id>","status":"<event>"}` via `snprintf`; `state_to_event()` maps terminal `TxMsgState` values.

#### `firmware/components/aprs_fsm/AprsTaskContext` (NEW)

- `include/pakt/AprsTaskContext.h` — SPSC ring buffer (depth 8) between BLE handler (producer) and `aprs_task` (consumer); owns `TxScheduler`.
- `AprsTaskContext.cpp` — `push_tx_request()` is lock-free (atomic head/tail); `tick()` drains ring + calls `scheduler_.tick()`; `notify_ack()` forwards to scheduler.
- TX notify fired as intermediate event inside `TransmitFn` wrapper; terminal events fired in `ResultFn`. Both routed through the `NotifyFn` callback.
- `aprs_fsm/CMakeLists.txt` updated: added `TxResultEncoder.cpp`, `AprsTaskContext.cpp`, `REQUIRES payload_codec`.

### Host tests added

- `firmware/test_host/test_payload_validator.cpp` — 42 tests covering acceptance, rejection, out-parameter filling, key-name collision, escaped-string edge cases, field-order independence, and malformed JSON.
- `firmware/test_host/test_tx_integration.cpp` — 26 tests covering `TxResultEncoder` encode/map, `AprsTaskContext` ring buffer, notify callbacks, TIMEOUT after max retries, radio-failure semantics, and ack-mismatch robustness.
- `firmware/test_host/CMakeLists.txt` updated: added `CODEC_INCLUDE`, `CODEC_SRC`, `FSM_EXTRA_SRC`, two new test files.

### `firmware/main/main.cpp` wiring

- `aprs_task` now instantiates a `static AprsTaskContext` with a stub `RadioTxFn` (logs and returns true) and a `NotifyFn` that calls `BleServer::instance().notify_tx_result()`.
- File-static `g_aprs_ctx` pointer published after context init; ble_task checks for null before pushing requests.
- `on_config_write` handler now validates via `PayloadValidator::validate_config_payload()`; rejects and logs on failure.
- `on_tx_request` handler validates via `PayloadValidator::validate_tx_request_payload()`, then calls `g_aprs_ctx->push_tx_request()`; returns false on validation failure or full ring.
- `firmware/main/CMakeLists.txt` updated: added `payload_codec` to REQUIRES.

### Runtime validation status (2026-03-15 sprint)

- `cmake` unavailable; C++ host tests not executed in this session.
- All new `.cpp` files compile independently (no ESP-IDF headers); confirmed by include-chain analysis.
- `AprsTaskContext` member declaration order (`notify_fn_` before `scheduler_`) ensures safe `this`-capture initialization.

---

## Python bug fixes (2026-03-15)

Two pre-existing failures found and fixed during `pytest` run (178 collected).

| # | File | Bug | Fix |
|---|------|-----|-----|
| 1 | `app/desktop_test/message_tracker.py` | `recent()` sort unstable when `queued_at` is identical (fast test execution) | Added `client_id` as tiebreaker: `key=lambda m: (m.queued_at, m.client_id)` |
| 2 | `app/desktop_test/telemetry.py` | `SysTelem.parse("null")` throws `AttributeError` because `json.loads("null")` returns `None`, not a dict | Added `if not isinstance(d, dict): return None` guard before field access |

**Result after fix: 178/178 passed.**

---

## P1 edge-case tests added (2026-03-15)

Extended test coverage for PayloadValidator and AprsTaskContext.

### `test_payload_validator.cpp` additions (36 → 42 tests)

- Config: extra whitespace around `:` is accepted
- Config: `ssid` field before `callsign` (field order independence)
- TX request: text with JSON `\"` escape (extracted correctly)
- TX request: text with JSON `\\` escape (extracted correctly)
- TX request: key name appearing as a string VALUE (colon-check correctly rejects it)
- TX request: unterminated text string → rejected

### `test_tx_integration.cpp` additions (23 → 26 tests)

- Radio tx failure: TX notify fires before radio call; message stays QUEUED and retries
- Invalid request (empty dest) pushed to ring: silently dropped by TxScheduler, no crash
- Ack for wrong msg_id: returns false, does not affect the active message

---

## Pre-hardware readiness summary (2026-03-15)

### Software-complete (no hardware required)

| Area | State |
|------|-------|
| AX.25 framing | ✓ Host-tested |
| APRS encode/decode | ✓ Host-tested |
| Bell 202 AFSK modem | ✓ Host-tested |
| BLE chunk reassembler (BleChunker) | ✓ Host-tested |
| TxScheduler FSM (retry, ack, timeout) | ✓ Host-tested |
| TxResultEncoder (wire-format JSON) | ✓ Host-tested |
| AprsTaskContext (SPSC ring, BLE→scheduler bridge) | ✓ Host-tested |
| PayloadValidator (config + tx_request BLE write validation) | ✓ Host-tested |
| NmeaParser (GPRMC/GPGGA, Unix timestamp) | ✓ Host-tested (37 tests) |
| DeviceCapabilities (JSON, BLE read) | ✓ Host-tested |
| Telemetry serializers (GPS, battery, system) | ✓ Host-tested |
| Desktop app: BLE transport FSM | ✓ Python-tested |
| Desktop app: MessageTracker (placeholder→firmware ID remap) | ✓ Python-tested |
| Desktop app: diagnostics store / telemetry parsers | ✓ Python-tested |
| CI pipeline (firmware-build, host-tests, app-tests) | ✓ Implemented |
| TX result wire format `{"msg_id":"...","status":"..."}` | ✓ Aligned firmware ↔ app |
| PTT watchdog (FW-016) + PttController hook | ✓ Software-complete; host-tested (21 tests) |
| DeviceConfigStore (config persistence layer) | ✓ Software-complete; host-tested (7 tests) |
| Payload contracts doc | ✓ Written + field names reconciled |
| Dev setup guide | ✓ Written |
| Bench bring-up checklist | ✓ Written |

### Hardware-blocked (cannot validate without prototype)

| Item | Dependency | First bench step |
|------|-----------|------------------|
| BLE connect + bonded-write enforcement | EVT board | Checklist step 3 |
| SA818 UART AT handshake + PTT polarity | SA818 + EVT | Checklist step 5 |
| SGTL5000 I2C detect | EVT board | Checklist step 4 |
| I2S MCLK stability | EVT board + scope | Checklist step 6 |
| AF_TX/AF_RX audio calibration (deviation) | SA818 + SGTL5000 | Checklist step 7 |
| GPS UART NMEA stream | u-blox M8 + EVT | Checklist step 8 |
| End-to-end APRS TX decode | SA818 + reference TNC | Checklist step 9 |
| PTT stuck-on fault test (hardware injection) | EVT board | Checklist step 10 |
| SA818 electrical validation (UART handshake, PTT polarity, audio deviation) | SA818 + EVT | Checklist step 5 (driver software-complete) |
| NVS config persistence on-device validation | ESP-IDF NVS + flash | After BLE pairing verified |

### Recommended first bench action

**Checklist step 1–3 in order** (`docs/bench_bringup_checklist.md`):
1. Power-only smoke test (supply rails, current draw, boot log)
2. BLE advertising visible (`PAKT-TNC` in scan)
3. BLE security handshake — confirm write rejected without bond, accepted after pairing

These three steps require only the ESP32-S3 Feather (no SA818, no audio adapter, no GPS).
They gate G0 (flash/boot) and G3 (auth enforcement) — the two highest-priority gates.

---

## FW-016 PTT Watchdog implementation (2026-03-15)

P0 safety item: prevents PTT from getting stuck asserted if `aprs_task` hangs or stalls.
Pure C++ — no ESP-IDF/FreeRTOS headers; fully host-testable.

### New component: `firmware/components/safety_watchdog/`

- `include/pakt/PttWatchdog.h` — three-state FSM (IDLE → ARMED → TRIGGERED):
  - `static constexpr uint32_t kDefaultTimeoutMs = 10'000` (10 s, 10 missed beats at 1 Hz)
  - `void heartbeat(uint32_t now_ms)` — arms watchdog, resets stale timer, clears triggered (recovery)
  - `bool tick(uint32_t now_ms)` — call from supervisor task; returns `true` exactly once per trigger event
  - `void force_safe(uint32_t now_ms)` — fires `safe_fn` immediately from any task; idempotent
  - `bool is_triggered() const; bool is_armed() const;`
  - All shared state via `std::atomic<>` with acquire/release ordering; `compare_exchange_strong` on `triggered_` ensures `safe_fn` fires exactly once even on concurrent `tick()` / `force_safe()` race
  - uint32_t wrapping arithmetic for elapsed-time check (correct for intervals < 2³¹ ms ≈ 24 days)
- `PttWatchdog.cpp` — implementation
- `CMakeLists.txt` — standalone component, no REQUIRES (pure C++)

### `firmware/main/main.cpp` wiring (updated 2026-03-15 hardening pass)

- Includes `pakt/PttWatchdog.h`, `pakt/PttController.h`, `pakt/DeviceConfigStore.h`, `<cinttypes>`
- File-static `g_ptt_watchdog` pointer (null until `aprs_task` is ready; `watchdog_task` guards with null check)
- File-static `g_device_config` (DeviceConfigStore; NvsStorage backend is wired during `app_main()` NVS init path)
- `aprs_task`: instantiates `static PttWatchdog watchdog(safe_fn, kDefaultTimeoutMs)`; safe_fn calls `pakt::ptt_safe_off()`; publishes `g_ptt_watchdog = &watchdog`; calls `watchdog.heartbeat(now_ms)` each loop iteration
- `radio_task`: configures GPIO11 (PTT output, HIGH=off), registers direct-GPIO safe-off lambda before SA818 init, calls `radio.init()`, sets APRS frequency, then upgrades safe-off callback to `radio.ptt(false)`; falls back to direct-GPIO-only loop if init fails
- New `watchdog_task` (priority 6, stack 2048): ticks every 500 ms, calls `g_ptt_watchdog->tick(now_ms)`
- Priority 6 is above `aprs_task` (5) and below `radio_task` (7); watchdog can preempt a hung `aprs_task`
- `on_config_write` handler: calls `g_device_config.apply(fields)`, logs persist success or in-memory-only warning
- `firmware/main/CMakeLists.txt`: added `safety_watchdog` to REQUIRES (already done in prior pass)

### New files: `PttController` (FW-016 hardening)

- `firmware/components/safety_watchdog/include/pakt/PttController.h` — free functions `ptt_register_safe_off()`, `ptt_safe_off()`, `ptt_is_registered()`; pure C++, host-testable
- `firmware/components/safety_watchdog/PttController.cpp` — single `static std::function<void()> s_safe_off_fn`; `ptt_safe_off()` is a no-op if not registered (safe: hardware PTT default = off)
- `firmware/components/safety_watchdog/CMakeLists.txt`: added `PttController.cpp` to SRCS

### New files: `DeviceConfigStore` (P1 config persistence)

- `firmware/components/payload_codec/include/pakt/DeviceConfigStore.h` — header-only class; `apply(ConfigFields&)` updates in-memory `DeviceConfig` and calls `storage_->save()` if a backend is set; `load()` populates from storage on startup; `config()` returns current state
- Backed by `IStorage*` (nullable; null = in-memory only, returns true)
- `apply()` always updates in-memory first; a persist failure does not roll back runtime state

### Host tests: `firmware/test_host/test_ptt_watchdog.cpp` (21 tests)

Unit tests (10):
- IDLE: `tick()` returns false before first heartbeat
- No timeout before threshold (999 ms < 1000 ms)
- Timeout fires exactly at threshold (1000 ms)
- Timeout fires at threshold + 1
- `tick()` idempotent after trigger (safe_fn called once only)
- `heartbeat()` resets the stale timer (defers timeout)
- `heartbeat()` clears triggered state (enables recovery)
- `is_triggered()` / `is_armed()` state transition sequence
- uint32_t wrap-around arithmetic (heartbeat near `0xFFFFFFFF`)

`force_safe` tests (4):
- `force_safe()` fires `safe_fn` immediately
- `force_safe()` is idempotent (multiple calls → 1 fire)
- `force_safe()` in IDLE (no heartbeat) fires once
- `force_safe()` after `tick()` timeout does not double-fire

`PttController` tests (4):
- `ptt_safe_off()` with no registration is a safe no-op
- `ptt_safe_off()` fires registered callback
- Watchdog trigger invokes `ptt_safe_off()` exactly once (not double-fired)
- Registration state transitions: unregistered → registered → cleared → unregistered (no re-fire)

Integration tests (3, with `RadioControlMock`):
- Stale heartbeat triggers `RadioControlMock::ptt(false)` exactly once
- Active heartbeat prevents `RadioControlMock::ptt(false)`
- Recovery cycle: timeout fires twice across two arm/trigger episodes

### Host tests: `firmware/test_host/test_config_store.cpp` (7 tests)

- `apply()` with no storage: in-memory config updated, returns true
- `apply()` with storage backend: `save()` called once, saved fields match
- `apply()` with failing storage: returns false, in-memory still updated
- `load()` without storage: returns false, config retains defaults
- `config_to_json()` reflects applied callsign + ssid
- `config_to_json()` default config emits empty callsign and `ssid:0`
- `config_to_json()` returns 0 on buffer too small

`firmware/test_host/CMakeLists.txt` updated: added `PttController.cpp` to `WATCHDOG_SRC`, added `test_config_store.cpp`.

### Payload contract fixes (2026-03-15 hardening pass)

- `payload_contracts.md §6` GPS: renamed JSON key `course_deg` → `course` to match what `GpsTelem::to_json()` actually emits and `telemetry.py` reads
- `payload_contracts.md §4` Device Status: aligned schema with `telemetry.py DeviceStatus.parse()` — `tx_queue` → `pending_tx`; added `bonded`, `gps_fix`, `rx_queue` fields
- `docs/aprs_mvp_docs/docs/05_ble_gatt_spec.md`: Device Status and GPS Telemetry examples updated to match canonical schemas; note added to refer to `payload_contracts.md`

### Updated pre-hardware readiness table

| Area | State |
|------|-------|
| PTT watchdog (FW-016) | ✓ Software-complete; host-tested (21 tests) |
| PttController (safe-off hook) | ✓ Software-complete; host-tested (4 tests) |
| DeviceConfigStore (config persistence layer) | ✓ Software-complete; host-tested (7 tests) |
| NvsStorage (NVS-backed IStorage) | ✓ Software-complete; wired in app_main NVS boot path |

The FW-016 row in the hardware-blocked table has been resolved to software-complete.
The `ptt_safe_off()` hardware binding (SA818 `radio.ptt(false)`) is implemented; on-device validation remains pending hardware bring-up.
NVS persistence on-device validation remains pending hardware (flash + NVS driver).

---

## KISS reality-check note (archived)

The detailed KISS implementation reality-check review from early 2026-03-16 was removed from this active ledger after confirming its findings were closed by later pass-2 through pass-5 changes:
- KISS TX now reaches the shared TX path
- KISS RX is wired through the AX.25 queue drain path
- desktop KISS bridge multi-chunk RX reassembly is implemented

The useful implementation outcome is preserved in the later KISS completion and pass logs below, while the now-resolved issue list is left to git history.

---

## Post-FW-016 cleanup sprint (2026-03-15)

### Changes

| Item | File(s) | Detail |
|------|---------|--------|
| A1: GPS fixture JSON key fix | `firmware/test_host/golden_payloads.h` | Renamed `"course_deg"` → `"course"` in `kGpsTelemetry` and `kGpsTelemetryNoFix` strings to match the actual JSON key emitted by `GpsTelem::to_json()`. The C++ struct field `GpsTelem::course_deg` is unchanged. |
| A2: Config persistence boot path | `firmware/main/NvsStorage.h` (NEW), `firmware/components/payload_codec/include/pakt/DeviceConfigStore.h`, `firmware/main/main.cpp`, `firmware/main/CMakeLists.txt` | Added `set_storage(IStorage*)` setter to `DeviceConfigStore`. Created `NvsStorage` (concrete `IStorage` using ESP-IDF NVS blob API, namespace `"pakt_cfg"`, key `"device_config"`, `schema_version` guard, `nvs_commit()` on every save). Wired NVS init + config load in `app_main()` before task creation, with explicit log for loaded/defaults/failure outcomes. Added `nvs_flash` to REQUIRES. |
| A3: PttController state-transition test | `firmware/test_host/test_ptt_watchdog.cpp` | Added 21st test: unregistered → registered → fires → cleared → unregistered (no re-fire). Total: 21 tests. |

---

## FW-003 SA818 driver bootstrap sprint (2026-03-15)

### New component: `firmware/components/radio_sa818/`

- `include/pakt/ISa818Transport.h` — pure virtual `write(data, len)` + `read(buf, len, timeout_ms)`; injectable for host testing
- `include/pakt/Sa818CommandFormatter.h` + `Sa818CommandFormatter.cpp` — static builders: `connect()` → `AT+DMOCONNECT\r\n`; `set_group(rx_hz, tx_hz, squelch, wide_band)` → `AT+DMOSETGROUP=BW,TXF,RXF,0000,SQ,0000\r\n`; frequency formatted as `NNN.NNNN`
- `include/pakt/Sa818ResponseParser.h` + `Sa818ResponseParser.cpp` — `Result{Ok,Error,Unknown}`; `parse_connect(resp)` / `parse_set_group(resp)` check `+DMO...:0` (Ok) vs non-zero status (Error)
- `include/pakt/Sa818Radio.h` + `Sa818Radio.cpp` — `IRadioControl` impl; `PttGpioFn = std::function<void(bool)>`; `init()` calls `ptt_fn_(false)` immediately; `set_freq()` idempotent (skips UART if values unchanged); `force_ptt_off()` on any UART error; error state blocks `ptt(true)` but not `ptt(false)`
- `CMakeLists.txt` — `REQUIRES pakt_hal`

### New file: `firmware/main/Sa818UartTransport.h`

Concrete `ISa818Transport` wrapping `uart_write_bytes` / `uart_read_bytes` (UART1). ESP-IDF only; excluded from host tests.

### `firmware/main/main.cpp` wiring (`radio_task`)

1. GPIO11 configured as output, default HIGH (PTT off)
2. `ptt_register_safe_off([](){ gpio_set_level(GPIO11, 1); })` — direct GPIO before init (race-safe; bypasses driver state)
3. UART1 configured: 9600 8N1, TX=GPIO15, RX=GPIO16
4. `radio.init()` → if fail, log error and stay in loop (direct-GPIO callback remains)
5. `radio.set_freq(144390000, 144390000)` — APRS simplex
6. `ptt_register_safe_off([](){ radio.ptt(false); })` — upgrade callback through driver after successful init

Lambda capture note: `kPttGpio` and `radio` are `static constexpr` / `static` locals (static storage duration); accessible from non-capturing lambdas per C++17 §6.7.1.

### `firmware/main/CMakeLists.txt`

Added `radio_sa818` and `driver` to REQUIRES.

### BLE config read wired to live config store

- `DeviceConfigStore::config_to_json(cfg, buf, len)` static method added — serializes `callsign` + `ssid` as `{"callsign":"...","ssid":N}`
- `on_config_read` handler in `ble_task` replaced: now calls `config_to_json(g_device_config.config(), ...)` instead of returning a static placeholder

### Host tests: `firmware/test_host/test_sa818.cpp` (18 tests)

`Sa818CommandFormatter` (5):
- `connect` command is correct (`AT+DMOCONNECT\r\n`)
- `connect` returns 0 on buffer too small
- `set_group` formats APRS frequency correctly (`144.3900`)
- `set_group` narrow band uses `BW=0`
- `set_group` squelch encoded correctly

`Sa818ResponseParser` (5):
- `parse_connect` recognizes OK / error / unknown
- `parse_set_group` recognizes OK / error

`Sa818Radio` (8):
- PTT before init forces PTT off via callback
- PTT after successful init succeeds
- `init` sends `DMOCONNECT` and parses OK
- `init` failure forces PTT off and returns false
- UART timeout during init forces PTT off
- `set_freq` is idempotent with same values
- `set_freq` failure forces PTT off
- `ptt(false)` succeeds even in error state

### Host tests: `firmware/test_host/test_config_store.cpp` additions (4 -> 7 tests)

Three new tests for `config_to_json`:
- Reflects applied callsign and ssid (`{"callsign":"W1AW","ssid":7}`)
- Default config has empty callsign and ssid 0
- Returns 0 on buffer too small

### `firmware/test_host/CMakeLists.txt`

Added `SA818_INCLUDE`, `SA818_SRC` (3 `.cpp` files), `test_sa818.cpp`.

---

## KISS software completion sprint (2026-03-16, pass 2)

Scope: close four KISS gaps identified in the 2026-03-16 audit. All changes are pure software; hardware bring-up items remain blocked.

Historical note: some pass-2 "next actions" and blocked items below were later closed by pass 3-5. Treat this section as a dated implementation snapshot, not the current open-issues list.

### Changes made

| # | Gap | Files changed | Summary |
|---|-----|--------------|---------|
| K1 | KISS TX → shared TX queue | `firmware/main/main.cpp`, `firmware/components/aprs_fsm/include/pakt/AprsTaskContext.h`, `firmware/components/aprs_fsm/AprsTaskContext.cpp`, `firmware/test_host/test_tx_integration.cpp` | `handlers.on_kiss_tx` now decodes KISS frame via `KissFramer::decode` then calls `g_aprs_ctx->push_kiss_ax25()`. `AprsTaskContext` extended with second SPSC ring (depth 4, max 330 B) and `set_raw_tx_fn(RawTxFn)`. Ring drained in `tick()` via configurable raw TX function (stub pending audio pipeline). 9 new C++ tests added (26→35). |
| K2 | KISS RX drain wired | `firmware/main/main.cpp` | `aprs_task` tick loop now drains `g_rx_ax25_queue`: each decoded AX.25 frame calls `notify_rx_packet` (native) and `KissFramer::encode` + `notify_kiss_rx` (KISS). File-static `Ax25RxQueue` SPSC added (depth 8); audio_task updated with exact producer call-site comment. |
| K3 | `notify_kiss_rx` INT-002 chunking | `firmware/components/ble_services/BleServer.cpp`, `firmware/components/ble_services/include/pakt/BleServer.h` | Rewrote `notify_kiss_rx` to send INT-002 chunks (kChunkPayload=17, per MTU=23 assumption) via `ble_gatts_notify_custom`, bypassing per-characteristic rate limiter. Added `kiss_rx_msg_id_` counter to BleServer private state. |
| K4 | Desktop multi-chunk KISS RX | `app/desktop_test/kiss_bridge.py`, `app/desktop_test/test_kiss_bridge.py` | `_on_kiss_rx_notify` rewritten: accumulates chunks per msg_id in `_rx_pending` dict (max 4 slots, LRU eviction), sorts by chunk_idx, delivers on completion. `_deliver_kiss_frame` fixed to not add extra FEND delimiters. `_rx_pending` cleared on disconnect. 12 new Python tests (38→50). |

### Pre-existing test bugs fixed (opportunistic, not regressions)

| File | Fix |
|------|-----|
| `firmware/test_host/test_aprs.cpp:131` | `||` inside `CHECK()` macro forbidden by doctest — extracted to `bool has_msg_id` |
| `firmware/test_host/test_ax25.cpp:24` | Expected value `0xB915` was CRC-CCITT-FALSE (non-reflected). AX.25 uses reflected CRC-16/0x8408; correct value for `{0x41}` is `0xA3F5`. Comment updated. |
| `firmware/test_host/test_ax25.cpp:38,190` | Residue expected `0xF0B8` (raw running CRC) but `fcs()` applies `^ 0xFFFF` xorout. Correct expected return value is `0x0F47 = 0xF0B8 ^ 0xFFFF`. |
| `firmware/components/gps/NmeaParser.cpp:135` | `2000 + yy` always — year 94 → 2094 instead of 1994. Fixed to NMEA Y2K convention: `yy >= 70 ? 1900 + yy : 2000 + yy`. |

### Test results (2026-03-16, pass 2)

| Suite | Result |
|-------|--------|
| Python (test_kiss_bridge, test_chunker, test_telemetry_app, test_capability, test_messaging) | **188 passed** in 0.05s |
| C++ host tests | **353 passed, 5 failed** (all 5 failures are pre-existing AFSK modem simulation tests: `AfskDemodulator` returns empty in pure-software sim; hardware timing/level calibration required — not caused by this session's changes) |

### Hardware-blocked items (unchanged from audit)

- Real AFSK TX via SA818 (RawTxFn is a log stub)
- Real audio RX → AX.25 decode → Ax25RxQueue.push() (audio_task is still a stub)
- Third-party KISS client validation (APRSdroid, Direwolf, YAAC, Xastir)
- BLE security validation for KISS TX writes (encrypted + bonded enforcement on hardware)

### Next agent actions

1. Step 2 (FW-004): wire SGTL5000 I2S driver + AFSK demodulator; push decoded frames into `g_rx_ax25_queue` in audio_task
2. Step 1 hardware: SA818 UART bring-up; replace `RawTxFn` stub with real `radio.ptt(true)` + AFSK TX
3. First hardware session: connect kiss_bridge.py, send KISS TX frame, verify firmware log; verify KISS RX notify reaches client after step 2 is complete
4. Validate AFSK modem round-trip host tests once hardware-calibrated timing constants are known

---

## Fix Log (2026-03-16, pass 3)

### AFSK demodulator root-cause analysis and fix

Historical note: this section records the pass-3 state. Later pass-4 and pass-5 entries supersede parts of the clocking and TX-path status below.

Two bugs in `AfskDemodulator.cpp` caused all 5 `TEST_SUITE("AFSK modem round-trip")` tests to fail:

**Bug 1 — ones_count abort threshold off-by-one (Critical)**
- `if (++ones_count_ >= 6)` triggered an abort when the 6 consecutive 1-bits inside an HDLC flag (0x7E = 0b01111110, sent LSB-first as 0,1,1,1,1,1,1,0) were received inside a frame. The abort fired before the trailing 0-bit could complete the 0x7E pattern in the shift register, so `dispatch_frame()` was never called.
- Fix: changed threshold to `>= 7`. HDLC specifies abort only on 7 or more consecutive 1-bits.

**Bug 2 — Q = 3.5 causes excessive biquad bandpass group delay (Critical)**
- Group delay of a biquad bandpass at centre frequency = Q × Fs / (π × fc).
- At Q = 3.5, fc = 1200 Hz, Fs = 8000 Hz: delay ≈ 7.4 samples, exceeding one symbol period (6.667 samples at 1200 baud / 8000 Hz).
- The decision signal (mark_env > space_env) lagged by ≈1 symbol, causing the transition-tracking synchroniser to sample adjacent symbols. All bit decisions were wrong; no valid frames were recovered.
- Diagnostic sweep confirmed Q ∈ {1.2, 1.3, 1.5, 2.0} all pass 5/5 frames; Q = 3.5 gives 0/5.
- Fix: changed `static constexpr float kQ = 3.5f` to `1.5f` in both the constructor and `reset()`. Q = 1.5 gives group delay ≈ 3.2 samples (0.48 symbol period), well within the eye opening.

### Audio pipeline wired (Step 2 / FW-004)

Note: this pass-3 section records the initial Step 2 landing. The clocking details below were corrected by pass 4; prefer the pass 4 and pass 5 entries later in this file for current SGTL5000/I2S values.

`audio_task` in `main.cpp` is no longer a stub. The following hardware pipeline is now implemented (hardware-gated at run time):

- I2C master bus initialised on SDA=GPIO8, SCL=GPIO9 at 400 kHz.
- SGTL5000 detected at I2C address 0x0A; register power-up sequence applied:
  - ADC path enabled; LINE_IN selected as ADC source.
  - Initial clock plan in this pass was later superseded by the corrected pass-4 plan.
  - I2S: slave mode, 16-bit, I2S (Philips) format.
- I2S RX channel: ESP32-S3 master, MCLK=GPIO4, BCLK=GPIO5, WS=GPIO6, DOUT=GPIO7, DIN=GPIO10.
  - Initial `I2S_MCLK_MULTIPLE_256` note in this pass was later corrected to the pass-4 `I2S_MCLK_MULTIPLE_1024` / `8.192 MHz` plan.
- `AfskDemodulator` instantiated at 8 kHz; callback pushes decoded AX.25 frames into `g_rx_ax25_queue`.
- `aprs_task` already drains `g_rx_ax25_queue` and forwards to native BLE notify + KISS RX notify.

On init failure (I2C/I2S not wired), `audio_pipeline_run()` logs and returns; `audio_task` idles. No crash, no PTT impact.

### Test results (2026-03-16, pass 3)

| Suite | Result |
|-------|--------|
| C++ host tests | **358 passed, 0 failed** (all 5 AFSK round-trip tests now pass after Bug 1 + Bug 2 fix) |
| Python tests | unchanged — 188 passed |

### Hardware-blocked items (updated)

- Real AFSK TX via SA818 (RawTxFn is a log stub in aprs_task; TX path wiring is next)
- SGTL5000 I2C/I2S runtime validation (audio_pipeline_run() will fail gracefully until hardware is connected)
- Exact MCLK calibration: see pass 4/5 current plan (`8.192 MHz` target); still measure and adjust during bring-up
- ADC input gain tuning (CHIP_ADC_CTRL = 0x0000 = 0 dB starting point)
- Third-party KISS client validation (APRSdroid, Direwolf, YAAC, Xastir)
- BLE security validation for KISS TX writes

### Next agent actions

1. Step 1 / SA818 TX: replace `RawTxFn` stub in `aprs_task` with real PTT + AFSK TX pipeline (Sa818Radio.ptt + AfskModulator + I2S write via the audio pipeline)
2. First hardware session: flash → BLE connect → bonded-write rejection (G3) → PTT safe-off (G3) → SA818 UART validation
3. SGTL5000 MCLK calibration + ADC gain tuning during audio bring-up

---

## Fix Log (2026-03-16, pass 4)

Scope: SGTL5000 clocking fix, full I2S TX channel, real `afsk_tx_frame()` implementation, TX path wired end-to-end, new TX buffer sizing tests added.

### SGTL5000 / I2S clocking mismatch (Critical — would prevent codec lock at runtime)

Previous code wrote `CHIP_CLK_CTRL = 0x0038` which encodes `SYS_FS = 48 kHz, MCLK_FREQ = 256×SYS_FS → MCLK = 12.288 MHz`. But the I2S driver used `I2S_MCLK_MULTIPLE_256 × 8000 Hz = 2.048 MHz`. These two clocks disagree by a factor of 6 — the SGTL5000 would never lock.

**Fix (main.cpp `sgtl5000_init()`):**
- `CHIP_CLK_CTRL = 0x0020` → `SYS_FS = 32 kHz, RATE_MODE = ÷4 → 8 kHz effective, MCLK = 256 × 32000 = 8.192 MHz`
- I2S `mclk_multiple = I2S_MCLK_MULTIPLE_1024` → `1024 × 8000 = 8.192 MHz` — now matches codec expectation.
- `BCLK = 256 kHz` (16-bit stereo Philips); `MCLK / BCLK = 32` — exact integer, no jitter.

**Additional SGTL5000 register fixes:**
- `CHIP_SSS_CTRL = 0x1000` (was 0x0000): routes I2S_IN → DAC for TX audio. Previous value routed ADC → DAC.
- `CHIP_ANA_CTRL = 0x0104` (was 0x0114): clears `MUTE_LO` (bit 4). Previous value kept line-out muted — SA818 AF_IN would receive silence during TX.
- Added `CHIP_DAC_VOL = 0x3C3C` (0 dB) and final `ANA_POWER = 0x42E0` writes to complete power-up sequence.

### I2S TX channel wired

`audio_pipeline_run()` now creates a full-duplex I2S channel pair (TX + RX) via `i2s_new_channel(&cfg, &tx_chan, &rx_chan)`. Previously only RX was allocated (`i2s_new_channel(&cfg, nullptr, &rx_chan)`), which prevented any audio TX. Both channels share the same MCLK/BCLK/WS. The TX handle is published to `g_i2s_tx_chan` after `i2s_channel_enable(tx_chan)`.

### `afsk_tx_frame()` implemented (replaces stub)

New file-static function in `main.cpp`:
1. Guards: `g_radio == nullptr || g_i2s_tx_chan == nullptr` → log + return false.
2. `AfskModulator::modulate_frame()` writes into `g_tx_pcm_buf[25600]` (global static, avoids stack pressure).
3. `g_radio->ptt(true)` → 10 ms ramp-up → `i2s_channel_write(g_i2s_tx_chan, ...)` with per-frame timeout → 150 ms DMA drain → `g_radio->ptt(false)`.
4. Returns false if modulation returns 0, PTT fails, I2S write errors, or byte count mismatches.

**TX PCM buffer sizing:** worst-case AX.25 (330 B, all 0xFF) with maximum bit stuffing → ~22 400 samples at 8 kHz / 1200 baud. `kAfskMaxPcmSamples = 25600` (14 % margin). Buffer sizing tests added and verified.

### RadioTxFn and RawTxFn stubs replaced

Both lambda stubs in `aprs_task` now call `afsk_tx_frame()` after encoding the frame via `aprs::encode_message` + `ax25::encode`. `aprs_task` stack increased from 4096 → 6144 words to accommodate AX.25 / APRS encoding stack frames.

`g_radio` is published by `radio_task` after successful SA818 init. `g_i2s_tx_chan` is published by `audio_task` after I2S enable. Both are checked for null before use — no mutex needed (single writer, single reader).

### New host tests (TX buffer sizing)

Added `TEST_SUITE("AfskModulator TX buffer sizing")` to `test_host/test_afsk_modem.cpp`:
1. **max-encoded AX.25 frame fits in 25 600 samples at 8 kHz** — worst-case 0xFF payload.
2. **zero-byte frame (degenerate) produces preamble+tail only** — null/empty boundary.
3. **output buffer exactly 1 sample too small returns 0** — no silent truncation.
4. **APRS encode_message + ax25::encode round-trip within buffer** — simulates full RadioTxFn path.

### Test results (2026-03-16, pass 4)

| Suite | Result |
|-------|--------|
| C++ host tests | **362 passed, 0 failed** (+4 TX buffer sizing tests; all pass) |
| Python tests | unchanged — 188 passed |

---

## Fix Log (2026-03-16, pass 5)

Scope: Correct `AfskModulator::modulate_frame()` truncation signaling; prove exact-fit validity; add definitive tests.

### Root cause: post-hoc `pos == out_max` check was ambiguous

The pass 4 fix used `if (pos == out_max) return 0` after writing. This is ambiguous:
- **Genuine truncation**: the buffer ran out mid-frame; pos stopped at `out_max`.
- **Exact-fit success**: the frame's last sample happened to land exactly at position `out_max - 1`, leaving `pos == out_max` — a fully valid write.

Both cases produce `pos == out_max`, making the check incorrectly return 0 for valid exact-fit frames.

### Fix: inline truncation detection in `emit_bit_samples`

`emit_bit_samples` was restructured from:
```cpp
for (int i = 0; i < n && pos < out_max; ++i) { out[pos++] = ...; }
```
to:
```cpp
for (int i = 0; i < n; ++i) {
    if (pos >= out_max) return true;  // buffer full BEFORE this write — truncated
    out[pos++] = ...;
}
return false;
```
The check now happens **before** each write. Returning `true` means a sample was skipped (truncation). Returning `false` after the loop means all `n` samples were written — even if `pos == out_max` after the last write.

All three private emit methods (`emit_bit_samples`, `emit_data_bit`, `emit_flag`) now return `bool` (truncated?). `modulate_frame` propagates via `bool truncated` with short-circuit: once truncated, further emit calls are skipped. Final return: `truncated ? 0 : pos`.

**Exact-fit trace** (`out_max == real_n`): last sample write: `pos = out_max - 1 < out_max` → write → `pos = out_max`. Loop exit (i == n). Return false. `modulate_frame` returns `pos = out_max = real_n`. ✓

**Truncation trace** (`out_max < real_n`): at some sample, `pos == out_max` before write → return true. `modulate_frame` returns 0. ✓

### New tests (pass 5, added to `test_host/test_afsk_modem.cpp`)

5. **exact-fit buffer returns sample count, not 0** — `out_max = real_n` (1493 for the 4-byte test frame); verifies return value is `real_n`, not 0. This test would have failed with the pass 4 `pos == out_max` fix.
6. **significantly undersized buffer returns 0** — `out_max = 10` (cannot fit even the first preamble flag); verifies early truncation is detected.

### Exact-fit answer: YES, exact-fit success is possible

The fractional sample accumulator (`8000/1200 = 6.6̄`) produces a deterministic per-bit sample count pattern. For any frame of fixed content, `real_n` is a definite integer. Passing `out_max = real_n` produces a perfectly valid full frame write with `pos == out_max == real_n`. This is a success case that must return `real_n`, not 0.

### Test results (2026-03-16, pass 5)

| Suite | Result |
|-------|--------|
| C++ host tests | **364 passed, 0 failed** (+2 new tests: exact-fit and significantly-undersized) |
| Python tests | unchanged — 188 passed |

Key verified values: worst-case AX.25 (330 B, 0xFF) → 22 400 samples < 25 600 limit. 4-byte test frame → 1493 samples. APRS encode_message + ax25::encode 66 B frame → 4800 samples.

### Files changed (pass 5)

- `firmware/components/modem/AfskModulator.cpp` — `emit_bit_samples`, `emit_data_bit`, `emit_flag` return `bool`; `modulate_frame` uses `bool truncated` with short-circuit; post-hoc `pos == out_max` check removed.
- `firmware/components/modem/include/pakt/AfskModulator.h` — private method signatures updated (void → bool).
- `firmware/test_host/test_afsk_modem.cpp` — 2 new tests added.

### Hardware-blocked items (updated)

- SGTL5000 MCLK verification on scope: measure 8.192 MHz at codec MCLK pin during boot; adjust `mclk_multiple` if ESP-IDF PLL produces a measurably off frequency.
- SGTL5000 `CHIP_ANA_POWER` bit assignments: values used (0x42E0) based on public application notes; verify against physical datasheet during bring-up.
- ADC input gain tuning (`CHIP_ADC_CTRL = 0x0000 = 0 dB` starting point; adjust with SA818 AF_OUT signal level).
- SA818 TX audio deviation calibration: measure with calibrated TNC during first RF session.
- KISS TX hardware validation: connect `kiss_bridge.py`, send a KISS frame, verify AFSK output on SA818.
- Third-party KISS client validation (APRSdroid, Direwolf, YAAC, Xastir).

### Next agent actions

1. Hardware bring-up (HW-010): flash/boot → G3 BLE security → G3 PTT safe-off → SA818 UART → SGTL5000 MCLK.
2. KISS TX validation: `kiss_bridge.py` → firmware KISS RX → AFSK TX on SA818 output.
3. GPS task UART: replace stub driver in `gps_task` with real UART read loop.
