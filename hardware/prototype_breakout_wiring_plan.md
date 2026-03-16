# Prototype Breakout Wiring Plan (ESP32-S3 Feather + Teensy Audio Rev D + SA818 + u-blox M8)

This document is the prototype harness baseline for your selected stack:
- MCU: Adafruit ESP32-S3 Feather 4MB Flash / 2MB PSRAM
- Audio codec board: Teensy Audio Adapter Rev D (SGTL5000)
- RF module: SA818 breakout/module
- GPS: u-blox M8 breakout board (M8N-class)

Goal:
- Use one wiring plan for prototype bring-up and later PCB migration.
- Keep firmware pin mapping stable between proto and PCB.
- Keep bench validation aligned with `docs/bench_bringup_checklist.md` and record values in `docs/bench_measured_values_template.md`.

## 1. Sources consulted

Project-local references:
- `hardware/prototyping_wiring.md`
- `hardware/component_selection_rationale.md`
- `doc/aprs_mvp_docs/hardware/interfaces.md`
- `doc/hardware/components/esp32-s3/sources.md`
- `doc/hardware/components/sa818/sources.md`
- `doc/hardware/components/neo-m8n/sources.md`
- `doc/hardware/components/sgtl5000/sources.md`

Notes:
- This repo does not contain local PDFs for all breakout board schematics. It stores authoritative source links and requires schematic verification against your exact board revision before freezing the harness.
- The electrical plan below follows the existing project net conventions and SGTL5000/SA818/GPS architecture already used in firmware docs.

## 2. Architecture wiring diagram

```mermaid
flowchart LR
    USB5V[USB 5V] --> FEATHER[ESP32-S3 Feather]
    BAT[Li-ion Battery] --> FEATHER

    FEATHER -->|3V3 + GND| AUDIO[Teensy Audio Adapter Rev D\nSGTL5000]
    FEATHER -->|UART + PTT + GND| SA[SA818]
    FEATHER -->|UART + GND| GPS[u-blox M8 Breakout]

    FEATHER -->|I2C SDA/SCL| AUDIO

    FEATHER -->|I2S BCLK/LRCLK/DOUT/DIN/MCLK| AUDIO

    AUDIO -->|AF_TX_COUPLED| SA
    SA -->|AF_RX_COUPLED| AUDIO

    SA --> ANT[SMA Antenna]
```

## 3. Pin map (prototype harness)

Use this mapping as the canonical firmware/net map. Wire Feather header pins that expose these GPIO numbers.

### 3.1 I2C control bus
| Feather GPIO | Net | Connects to |
|---|---|---|
| GPIO8 | I2C_SDA | SGTL5000 SDA (Teensy Audio), optional other I2C peripherals |
| GPIO9 | I2C_SCL | SGTL5000 SCL (Teensy Audio), optional other I2C peripherals |

Requirements:
- One pull-up set only on the bus (typ 2.2k to 4.7k to 3.3V).
- If breakouts already include pull-ups, remove/disable extras where possible.

### 3.2 I2S digital audio bus (ESP32 master)
| Feather GPIO | Net | Teensy Audio Rev D / SGTL5000 signal |
|---|---|---|
| GPIO5 | I2S_BCLK | BCLK / SCLK |
| GPIO6 | I2S_WS | LRCLK / WS |
| GPIO7 | I2S_DOUT | DIN (codec data input from MCU) |
| GPIO10 | I2S_DIN | DOUT (codec data output to MCU) |
| GPIO4 | I2S_MCLK | MCLK / SYS_MCLK |

Requirements:
- Keep these wires short and grouped with adjacent ground returns.
- Add optional 22 to 47 ohm series resistors at MCU side if edges ring on long jumper harnesses.

### 3.3 UARTs and control
| Function | Feather GPIO | Net | Remote signal |
|---|---|---|---|
| GPS TX | GPIO17 | GPS_RX_CTRL | u-blox RXD |
| GPS RX | GPIO18 | GPS_TX_NMEA | u-blox TXD |
| Radio TX | GPIO15 | SA818_RX_CTRL | SA818 RXD |
| Radio RX | GPIO16 | SA818_TX_STAT | SA818 TXD |
| PTT | GPIO11 | SA818_PTT | SA818 PTT |

Optional:
- GPS PPS -> one spare interrupt-capable GPIO (for time sync diagnostics).

## 4. Analog audio interconnect (codec <-> SA818)

| Source | Destination | Net | Prototype conditioning |
|---|---|---|---|
| SGTL5000 line out / DAC out | SA818 AF_IN | AF_TX_COUPLED | AC coupling cap + attenuation pad option |
| SA818 AF_OUT | SGTL5000 line in / ADC in | AF_RX_COUPLED | AC coupling cap + optional RC low-pass |

Recommended starting network (tune during bench calibration):
- AF_TX path: 1 uF AC coupling capacitor in series, plus resistor divider footprint (for TX deviation tuning).
- AF_RX path: 1 uF AC coupling capacitor in series; optional RC footprint (for example 10k/10nF) for high-frequency noise trim.

## 5. Power and grounding guidance (important for SA818 reliability)

### 5.1 Rail strategy
- Keep Feather onboard battery/charger path as your prototype baseline.
- For this prototype, battery management/charging is provided by the Feather board (no external charger/fuel-gauge wiring required).
- Feed SA818 from a dedicated radio rail branch with local bulk capacitance.
- Keep codec on clean 3.3V domain; optionally add ferrite bead from digital 3.3V to analog codec rail.

### 5.2 Minimum decoupling on prototype harness
- SA818 supply entry: 100 nF + 10 uF + extra bulk (start with 220 to 470 uF low-ESR near module).
- SGTL5000 board supply: verify local decoupling is populated on Teensy Audio board; add local 10 uF on harness if leads are long.
- GPS breakout: 100 nF close to module VIN/VCC if breakout does not already provide enough local decoupling.

### 5.3 Grounding
- Use a star-like return concept in proto wiring: separate SA818 high-current return path from codec analog return as much as practical.
- Run at least one dedicated ground wire alongside each signal bundle (I2S/UART/audio).

## 6. Protection and support components to include now (so PCB reuse is easy)

Add these in prototype harness or as inline adapter boards where practical:
- PTT stage option: transistor driver footprint/path (NPN or NMOS) with pull resistor, in case direct GPIO drive is noisy or polarity-sensitive.
- USB and external connector ESD protection (especially if antenna/debug jacks are user-exposed).
- Reverse polarity and inrush strategy for battery path (if using external battery connectors in proto).
- Optional ferrite bead and extra bulk pad locations on radio rail.
- Test points or accessible clip points for: V_SYS_3V3, V_RADIO, AF_TX_COUPLED, AF_RX_COUPLED, SA818_PTT.

## 7. Breakout schematic checks to perform before freezing

Because breakout revisions vary, confirm the following on each exact board revision you own:

1. ESP32-S3 Feather
- Confirm each required GPIO is actually broken out to a header pad.
- Confirm no boot-strapping conflict on selected pins.
- Confirm battery/charger behavior for expected TX burst load profile.

2. Teensy Audio Adapter Rev D
- Confirm SGTL5000 pins exposed and naming for MCLK/BCLK/LRCLK/DIN/DOUT/SDA/SCL.
- Confirm board logic voltage and pull-up population.
- Confirm line-in/line-out reference level and coupling expectations.

3. SA818 breakout/module
- Confirm supply voltage range, TX burst current, and logic input thresholds.
- Confirm PTT polarity and default state.
- Confirm AF input/output nominal levels.

4. u-blox M8 breakout
- Confirm UART voltage level compatibility with 3.3V logic.
- Confirm onboard regulator/input voltage expectations (3.3V vs 5V VIN).
- Confirm availability of PPS pin if you plan to use it.

## 8. Firmware-to-hardware continuity notes

Keep these net names and GPIO assignments aligned in firmware config for direct proto->PCB reuse:
- I2C: `I2C_SDA`, `I2C_SCL`
- I2S: `I2S_BCLK`, `I2S_WS`, `I2S_DOUT`, `I2S_DIN`, `I2S_MCLK`
- Radio: `SA818_PTT`, `SA818_RX_CTRL`, `SA818_TX_STAT`
- GPS: `GPS_TX_NMEA`, `GPS_RX_CTRL`, optional `GPS_PPS`
- Audio: `AF_TX_COUPLED`, `AF_RX_COUPLED`

## 9. Bring-up sequence for this prototype stack

1. Power-only smoke test (Feather + rails, no SA818 TX).
2. I2C detect SGTL5000 path.
3. I2S/MCLK validation (clock present and stable).
4. SA818 UART command test + PTT polarity test (no sustained TX yet).
5. Audio loop calibration (codec -> SA818 AF_IN and SA818 AF_OUT -> codec).
6. GPS NMEA lock and UART stability.
7. Full APRS path with repeated TX while monitoring resets/brownout.

## 10. Open items before PCB capture

- Capture exact breakout board schematic revisions (PDFs) into `doc/hardware/components/...` for traceable signoff.
- Replace any provisional component-level assumptions with measured bench values (AF attenuation/filter values, radio bulk capacitance, PTT drive topology).
- Freeze final pin map only after confirming chosen Feather GPIOs are conflict-free for boot and runtime.
