# Bench Measured Values Log

Date: ___________
Tester: ___________
Hardware revision: ___________
Firmware version (git hash): ___________

Fill in actual measured values during `docs/bench_bringup_checklist.md` bring-up.
Replace each placeholder with the measured value and mark the step pass/fail.

---

## Step 1 — Power-only smoke test

| Measurement | Expected | Measured | Pass? |
|-------------|----------|----------|-------|
| V_3V3 (Feather rail) | 3.28–3.36 V | | |
| Idle current (USB) | < 50 mA | | |
| Thermal: Feather LDO | no hot spot | | |

Notes:

---

## Step 2 — BLE advertising

| Measurement | Expected | Measured | Pass? |
|-------------|----------|----------|-------|
| BLE device visible in scan | Yes, `PAKT-TNC` | | |
| Time to appear | < 5 s | | |

Notes:

---

## Step 3 — BLE security

| Measurement | Expected | Measured | Pass? |
|-------------|----------|----------|-------|
| Write rejected before pairing | auth error | | |
| Write accepted after pairing | success | | |
| Bond retained after reconnect | yes | | |

Notes:

---

## Step 4 — I2C / SGTL5000

| Measurement | Expected | Measured | Pass? |
|-------------|----------|----------|-------|
| SGTL5000 I2C address detected | 0x0A | | |
| I2C bus voltage | 3.3 V high | | |

Notes:

---

## Step 5 — SA818 UART

| Measurement | Expected | Measured | Pass? |
|-------------|----------|----------|-------|
| V_RADIO supply | 3.6–4.2 V | | |
| SA818 AT handshake response | `+DMOCONNECT: 0` | | |
| PTT pin voltage (asserted) | low (< 0.4 V) | | |
| PTT pin voltage (de-asserted) | high (> 2.4 V) | | |

Notes:

---

## Step 6 — I2S / MCLK

| Measurement | Expected | Measured | Pass? |
|-------------|----------|----------|-------|
| I2S_MCLK frequency (GPIO4) | 8.192 MHz | | |
| I2S_BCLK frequency (GPIO5) | 256 kHz | | |
| I2S_WS frequency (GPIO6) | 8.000 kHz | | |
| I2S underrun count (idle) | 0 | | |

Notes:

---

## Step 7 — Audio calibration

| Measurement | Expected | Measured | Pass? |
|-------------|----------|----------|-------|
| SA818 AF_IN level (1200 Hz) | ~100 mVpp | | |
| TX deviation (APRS) | ±3 kHz | | |
| Attenuator setting (R divider) | TBD | | |
| SA818 AF_OUT level | TBD | | |
| RC LPF cutoff (AF_RX path) | TBD | | |

Notes:

---

## Step 8 — GPS NMEA

| Measurement | Expected | Measured | Pass? |
|-------------|----------|----------|-------|
| Time to first fix (cold) | < 90 s | | |
| Satellite count at fix | ≥ 4 | | |
| Lat/lon vs reference | < 10 m error | | |

Notes:

---

## Step 9 — APRS TX beacon

| Measurement | Expected | Measured | Pass? |
|-------------|----------|----------|-------|
| Frame decoded by reference TNC | yes | | |
| Source callsign correct | matches config | | |
| Decode success rate (5 TX) | 5/5 | | |
| Resets during 5 TX sequence | 0 | | |
| Brownout events | 0 | | |

Notes:

---

## Step 10 — PTT fault test

| Measurement | Expected | Measured | Pass? |
|-------------|----------|----------|-------|
| PTT de-asserts after BLE disconnect | < 5 s | | |
| PTT state on boot (before first log) | low (off) | | |
| PTT state after hard power cycle | low (off) | | |

Notes:

---

## Open items after bring-up

List any measurements that were out of spec or require follow-up:

1.
2.
3.
