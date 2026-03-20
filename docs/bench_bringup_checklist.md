# Hardware Bench Bring-Up Checklist

Aligned with: `hardware/prototype_breakout_wiring_plan.md`
Purpose: step-by-step validation sequence for first EVT prototype bring-up.

Status source for what is already software-complete versus still hardware-gated:
- `docs/aprs_mvp_docs/agent_bootstrap/gate_pass_matrix.md`
- `docs/aprs_mvp_docs/agent_bootstrap/audit.md`

Each step lists the expected outcome and the failure mode to watch for.
Record measured values in `docs/bench_measured_values_template.md`.

Bench selection note:
- Blocking bench stages are now configurable in `firmware/main/bench_profile_config.h`.
- For focused debug runs, disable unrelated stages before rebuilding/flashing.
- APRS Stage C can now be run as an isolated recorder/export workflow and has a configurable ADC gain step.

---

## Pre-power checklist (bench setup)

Before applying any power:

- [ ] Wiring matches `hardware/prototype_breakout_wiring_plan.md` pin map — verify each net
- [ ] I2C pull-ups: exactly one set (2.2–4.7 kΩ to 3.3 V); duplicates removed/lifted
- [ ] SA818 supply decoupling populated: 100 nF + 10 µF + 220–470 µF bulk near module
- [ ] SGTL5000 supply decoupling: local 10 µF on harness if leads are long
- [ ] GPS breakout decoupling: 100 nF on VIN/VCC
- [ ] No solder bridges on Feather headers or audio adapter
- [ ] PTT net open (SA818 PTT not asserted) — verify with multimeter before power-on

---

## Step 1 — Power-only smoke test

**Goal:** verify supply rails before connecting active peripherals.

1. [ ] Apply USB 5 V to Feather USB port (no battery, no SA818 connected)
2. [ ] Measure V_3V3 on Feather 3.3 V rail → expect 3.28–3.36 V
3. [ ] Measure current draw → expect < 50 mA (MCU idle only)
4. [ ] No thermal hot spots on Feather LDO or Feather board
5. [ ] Open serial monitor (115200 baud) — expect `PAKT firmware vX.Y.Z starting` log

**Failure mode:** over-current or missing 3.3 V → check for wiring short before proceeding.

---

## Step 2 — BLE advertising visible

**Goal:** confirm BLE stack starts without errors.

1. [ ] Power on (USB)
2. [ ] On a phone or PC BLE scanner, scan for device named `PAKT-TNC`
3. [ ] Device appears in advertising list within 5 s
4. [ ] Serial log shows `BleServer init` and `advertising started` (or equivalent)

**Failure mode:** device not visible → check NimBLE stack log for init errors.

---

## Step 3 — BLE connect + security handshake (G3 critical path)

**Goal:** verify encrypted+bonded write enforcement.

1. [ ] Connect with desktop test app: `python app/desktop_test/main.py`
2. [ ] Attempt a config write **before** pairing → expect write rejected with auth error
3. [ ] Initiate pairing via OS Bluetooth settings → `PAKT-TNC` → Pair
4. [ ] Retry config write with `{"callsign":"W1AW"}` → expect **success** (no error)
5. [ ] Reconnect without re-pairing → config write still succeeds (bond retained)

Record: paired successfully, auth error confirmed on unpaired write.

**Failure mode:** write accepted without pairing → critical security regression; do not proceed.

---

## Step 4 — I2C detect (SGTL5000)

**Goal:** confirm SGTL5000 codec responds at its I2C address.

1. [ ] Connect Teensy Audio Rev D to Feather per wiring plan (`I2C_SDA=GPIO3`, `I2C_SCL=GPIO4`)
2. [ ] Power on
3. [ ] Ensure firmware starts `SYS_MCLK` before codec probing
4. [ ] Run I2C scan firmware or monitor log for SGTL5000 detect message
5. [ ] Expected I2C devices on the current harness: `0x0A` (SGTL5000), `0x36` (MAX17048), `0x42` (u-blox M9N)
6. [ ] Serial log shows `Using SGTL5000 candidate address 0x0A` and successful codec init

---

## Step 5 — SA818 UART command test

**Goal:** confirm Sa818Radio driver, UART communication, and PTT polarity.

Driver: `firmware/components/radio_sa818/Sa818Radio.cpp` — wired into `radio_task` via
`Sa818UartTransport` (UART1, 9600 8N1) and `PttGpioFn` lambda (GPIO11 active-low).

1. [ ] Connect SA818 to Feather per wiring plan:
   - Feather GPIO13 → SA818 RX (AT command input)
   - Feather GPIO9 → SA818 TX (AT response output)
   - Feather GPIO11 → SA818 PTT (active low)
2. [ ] Power on SA818 supply rail; measure V_RADIO → expect 3.6–4.2 V (SA818 datasheet §4)
3. [ ] Power on Feather; serial log must show within 2 s:
   - `radio: PTT GPIO11 configured (HIGH=off); watchdog safe-off registered`
   - `sa818_bench: --- STAGE 1: UART HANDSHAKE (AT+DMOCONNECT) ---`
   - `radio: SA818 init OK`
   - (If init fails: `radio: SA818 init failed – radio unavailable; PTT remains off`)
4. [ ] Confirm AT handshake visible on UART1 with logic analyser or USB-serial tap:
   - Firmware sends: `AT+DMOCONNECT\r\n`
   - SA818 responds: `+DMOCONNECT:0\r\n`
5. [ ] Serial log shows frequency set: `AT+DMOSETGROUP=1,144.3900,144.3900,0000,1,0000\r\n`
   - SA818 responds: `+DMOSETGROUP:0\r\n`
6. [ ] PTT polarity test — trigger a TX request via BLE; measure SA818 PTT pin:
   - PTT asserted (TX on)  → GPIO11 LOW  → SA818 PTT pin should be LOW
   - PTT de-asserted (idle) → GPIO11 HIGH → SA818 PTT pin should be HIGH
7. [ ] **Do not sustain TX** — immediately cancel or wait for timeout after polarity confirmed

Record: SA818 UART response string, PTT voltage on assert/de-assert, V_RADIO under TX load.

**Failure modes:**
- `SA818 init failed` in log → check UART wiring polarity (TX/RX swap is common)
- PTT logic inverted → verify GPIO11 is wired to SA818 PTT; check active-low assumption
- `+DMOCONNECT:1` response → SA818 in bad state; power-cycle SA818 supply rail

---

## Step 6 — I2S / MCLK stability

**Goal:** confirm MCLK is present and I2S clocks are stable.

1. [ ] With SGTL5000 and Feather connected, power on
2. [ ] Probe I2S_MCLK (GPIO14) with oscilloscope → expect 8.192 MHz
3. [ ] Probe I2S_BCLK (GPIO8) → expect 256 kHz
4. [ ] Probe I2S_WS (GPIO15) → expect 8 kHz square wave
5. [ ] Monitor for I2S underrun/overrun counters in serial log (should be 0 at idle)

Clock plan:
- ESP32-S3 I2S master uses `I2S_MCLK_MULTIPLE_1024` at 8 kHz → `8.192 MHz` MCLK
- SGTL5000 expects `SYS_FS = 32 kHz`, `RATE_MODE = ÷4` → effective `8 kHz` ADC/DAC
- Expected ratio: `MCLK / BCLK = 32`

Record: MCLK frequency, BCLK frequency, WS frequency, and whether the measured values are stable across reconnect/reset.

---

## Step 7 — Audio loop calibration (codec ↔ SA818)

**Goal:** set TX audio level and RX audio level for correct APRS deviation.

1. [ ] Connect SGTL5000 line out → SA818 AF_IN via AF_TX_COUPLED network (1 µF + attenuator footprint)
2. [ ] Connect SA818 AF_OUT → SGTL5000 line in via AF_RX_COUPLED network (1 µF + optional RC LPF)
3. [ ] Run the firmware bench stages and confirm:
   - headphone tones are audible on the PJRC jack
   - `audio_bench` shows non-zero `peak` / `rms` values when `LINE_IN` is driven
   - `sa818_bench` runs the 10-tone TX sequence only with a load or antenna attached
   - `sa818_bench` RX stage logs live `rx_peak_abs` values while a nearby radio transmits voice/audio
4. [ ] Inject a 1200 Hz / 2200 Hz Bell 202 tone pair from PC audio into codec line in
5. [ ] Monitor SA818 AF_IN level with AC voltmeter → adjust attenuator for ~100 mVpp (SA818 AF_IN nominal)
6. [ ] Key SA818 TX; monitor TX deviation with calibrated reference receiver or RF power meter
7. [ ] Target: ±3 kHz deviation for FM APRS (adjust R divider in AF_TX_COUPLED)

Record: AF_TX_COUPLED attenuation setting, measured deviation, SA818 AF_OUT level into codec, and RX `max_peak` / `signal_seconds` from the bench log.

---

## Step 8 — GPS NMEA lock

**Goal:** confirm GPS UART and fix acquisition.

1. [ ] Connect u-blox M8 to Feather per wiring plan (TX=GPIO17/GPS_RX_CTRL, RX=GPIO18/GPS_TX_NMEA)
2. [ ] Place GPS module with sky view (window or outdoors)
3. [ ] Serial log shows NMEA sentences being received (firmware GPS task log)
4. [ ] Wait for fix (typically 30–60 s cold start, <5 s hot start)
5. [ ] BLE GPS telemetry notify shows non-zero `lat`, `lon`, `sats ≥ 4`, `fix = 1`

Record: time to first fix, sat count at fix, lat/lon vs known reference.

---

## Step 9 — Full APRS TX beacon

**Goal:** end-to-end APRS TX decode on a reference receiver.

1. [ ] All prior steps completed and passing
2. [ ] Callsign configured via BLE config write
3. [ ] Send a TX request via BLE: `{"dest":"APRS","text":">PAKT bench test"}`
4. [ ] Observe TX result notify on desktop app: `{"msg_id":"1","status":"tx"}`
5. [ ] Reference TNC or SDR + software decoder confirms APRS frame decode
6. [ ] Frame shows correct source callsign, destination, and text
7. [ ] Repeat 5× while monitoring for resets, brownout events, or PTT stuck-on

Record: decode success rate, receiving device/app used, decoded packet text, and any brownout or WDT reset events.

**Important:** hearing Bell 202 tones is not enough for Step 9 to pass; this step requires packet-level APRS decode on the receiving side.

---

## Step 9b — APRS RX listen window

**Goal:** prove the prototype can decode on-air APRS packets, not just voice energy or generic RF audio.

1. [ ] Tune the prototype to the APRS channel for your region (`144.390 MHz` in North America)
2. [ ] Provide a real APRS source during the receive window:
   - APRSdroid + HT audio/PTT interface
   - a TNC-connected radio
   - a second PAKT board transmitting APRS frames
3. [ ] Transmit APRS packets every 20-30 seconds during the firmware RX bench window
4. [ ] Confirm the prototype logs a decoded AX.25/APRS frame
5. [ ] If no frame decodes, record whether `rx_peak_abs` changed anyway
6. [ ] If deeper analysis is needed, run the 30-second RX recorder stage and save the emitted base64 WAV dump from serial for offline analysis
7. [ ] Prefer the `16-bit` Stage C recorder path for new captures; use the selected ADC gain step in `bench_profile_config.h` to match the best observed RX margin

Record: APRS source used, packet interval, approximate distance, decoded frame examples if any, `rx_peak_abs` behavior if decode fails, and whether a 30-second WAV capture was exported.

---

## Step 10 — PTT stuck-on fault test (G3 critical path)

**Goal:** confirm PttWatchdog (FW-016) de-asserts PTT under fault conditions.

FW-016 parameters (see `PttWatchdog::kDefaultTimeoutMs`):
- Timeout: 10 000 ms (10 s)
- Supervisor tick: 500 ms
- `aprs_task` heartbeat: every 1 000 ms (loop period)

Sub-test A — Stale heartbeat (BLE disconnect):

1. [ ] Send a TX request; observe `status: tx` notify and PTT asserted on SA818
2. [ ] Kill the desktop app or disconnect BLE (do not send any further heartbeat)
3. [ ] Within 10–11 s, firmware serial log must show: `PTT WATCHDOG TRIGGERED at t=... ms – forcing PTT off`
4. [ ] PTT must de-assert (SA818 PTT pin goes high) within that window
5. [ ] After reconnect, send a fresh TX request → watchdog re-arms, PTT can be asserted again (recovery)

Sub-test B — aprs_task hang simulation (not possible in software stub; revisit when SA818 driver wired):

1. [ ] When SA818 driver is wired, induce a deliberate `vTaskDelay` hang in aprs_task
2. [ ] Watchdog fires within 10 s; PTT de-asserts; verify via SA818 PTT pin

Sub-test C — Reboot safe-off:

1. [ ] During active TX, remove power briefly then restore
2. [ ] On reboot, PTT must be low before firmware log prints first line
3. [ ] Firmware log must show `PTT default state: OFF` before any task starts

**Failure mode:** PTT stays high after BLE disconnect or reboot → **do not transmit further**; investigate FW-016 watchdog wiring before any field use.

---

## Pass/fail summary

| Gate | Step | Pass criterion |
|------|------|----------------|
| G0 | 1–2 | Flash, boot, BLE advertising |
| G3 | 3, 10 | Auth enforcement, PTT safe-off |
| G1 | 4–9 | Full APRS TX decode on reference receiver |
| G2 | — | 1-hour BLE endurance run (separate test session) |
| G4 | 7, 9 | No resets under repeated TX; audio levels in spec |
