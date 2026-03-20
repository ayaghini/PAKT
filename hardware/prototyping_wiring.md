# Prototyping Wiring Guide (Schematic-Ready Draft)

This document defines a wiring baseline that can be transferred into schematic capture and first-pass PCB layout.

For the breakout-board prototype stack (ESP32-S3 Feather + Teensy Audio Rev D + SA818 + u-blox M9N), see:
- `hardware/prototype_breakout_wiring_plan.md`

## Scope and assumptions
- Target architecture: ESP32-S3 + SA818 + SGTL5000 + u-blox M9N + MAX17048.
- Logic domain is 3.3V unless a module explicitly requires otherwise.
- SA818 supply must be validated against the exact module datasheet before PCB release.
- Final design must pass bench validation items in Section 10 before fabrication release.
- Prototype strategy: use Adafruit Feather ESP32-S3 onboard charger + battery monitor behavior as the wiring baseline for custom PCB migration.

## 1. Power Tree

### Functional flow
1. USB-C 5V input -> Li-ion charger input.
2. Charger BAT output -> single-cell Li-ion battery.
3. Battery rail -> power switch/load switch.
4. Switched battery rail feeds:
   - `V_RADIO` (SA818 path)
   - `V_SYS_3V3` regulator path (digital and codec domains)

### Required rails
- `VBAT_RAW`: battery node from charger/battery connector.
- `V_SYS_3V3`: primary regulated 3.3V digital rail.
- `V_AUD_3V3`: filtered codec analog rail (L or ferrite bead from `V_SYS_3V3`).
- `V_RADIO`: SA818 supply rail, isolated from MCU rail return noise.

### Power integrity requirements
- Place local bulk capacitance near SA818 supply entry.
- Place 100nF decoupling at every IC power pin, shortest loop to local ground.
- Add at least one bulk capacitor near ESP32-S3 module supply entry.
- Keep SA818 TX current return out of codec analog ground path.
- Include brownout test point on `V_SYS_3V3` and `V_RADIO`.

## 2. Digital Interfaces

### I2C shared bus (Control)
| ESP32-S3 Pin | Net        | Device Pins                              |
|--------------|------------|-------------------------------------------|
| GPIO3        | I2C_SDA    | SGTL5000 SDA, MAX17048 SDA, SSD1306 SDA  |
| GPIO4        | I2C_SCL    | SGTL5000 SCL, MAX17048 SCL, SSD1306 SCL  |

I2C requirements:
- Used for configuration and control of I2C peripherals.
- One pull-up set only per bus (typically 2.2k to 4.7k to 3.3V).
- Confirm no duplicate strong pull-ups on all breakouts in prototype.
- Keep bus short and route away from antenna feed and SA818 RF section.
- On the Adafruit Feather ESP32-S3 prototype harness, confirm the board's I2C pull-up power path is enabled if using onboard pull-ups.
- Bench-verified devices on this shared bus are `SGTL5000 @ 0x0A`, `MAX17048 @ 0x36`, and `u-blox M9N @ 0x42`.
- Important bring-up note: the SGTL5000 does not answer cleanly until `SYS_MCLK` is running. Start I2S/MCLK first, then perform codec I2C init.
- Bench-proven audio output path: with `SYS_MCLK` active, the PJRC headphone jack works when SGTL5000 routing remains `I2S -> DAP -> DAC -> HP` and the codec is configured after MCLK starts.

### I2S bus (ESP32 master, Audio Data)
| ESP32-S3 Pin | Net       | SGTL5000 Pin |
|--------------|-----------|--------------|
| GPIO8        | I2S_BCLK  | SCLK         |
| GPIO15       | I2S_WS    | LRCLK        |
| GPIO12       | I2S_DOUT  | DIN          |
| GPIO10       | I2S_DIN   | DOUT         |
| GPIO14       | I2S_MCLK  | SYS_MCLK     |

I2S requirements:
- Used for digital audio data transfer between ESP32 and SGTL5000.
- Keep traces short and length-matched where practical.
- Route over continuous ground reference.
- Avoid running parallel to RF or high-current TX supply routes.
- SGTL5000 clocking requires valid `SYS_MCLK`; verify selected ESP32 I2S/MCLK GPIO in firmware and schematic.
- Bench verification on the current harness: `GPIO14` MCLK at `8.192 MHz`, with codec bring-up succeeding only after MCLK is enabled.
- Bench verification on the current harness: audible headphone output is confirmed with 16-bit stereo Philips framing on the I2S bus.
- Current audio-first bench remap uses `GPIO15` for `I2S_WS`; this conflicts with the earlier draft SA818 UART TX assignment and must be revisited before full radio integration.

### UART links
| Interface | ESP32-S3 Pin | Net           | Remote Pin |
|-----------|---------------|---------------|------------|
| GPS TX    | GPIO17        | GPS_RX_CTRL   | M9N RXD    |
| GPS RX    | GPIO18        | GPS_TX_NMEA   | M9N TXD    |
| SA818 TX  | GPIO13        | SA818_RX_CTRL | SA818 RXD  |
| SA818 RX  | GPIO9         | SA818_TX_STAT | SA818 TXD  |

Optional but recommended:
- Route GPS PPS to spare ESP32 interrupt-capable GPIO.

## 3. Control and UI Signals

| ESP32-S3 Pin | Net           | Destination            | Notes |
|--------------|---------------|------------------------|-------|
| GPIO11       | SA818_PTT     | SA818 PTT              | Verify active polarity from datasheet; add pull resistor and optional transistor stage footprint. |
| GPIO38       | LED_STATUS_G  | Green LED anode        | Series resistor required. |
| GPIO39       | LED_RX_B      | Blue LED anode         | Series resistor required. |
| GPIO40       | LED_TX_R      | Red LED anode          | Series resistor required. |
| GPIO41       | BTN_FUNC_N    | Function button to GND | Use internal or external pull-up; add debounce in firmware. |
| GPIO42       | HAPTIC_DRV    | NPN/MOSFET gate/base   | Use transistor driver + flyback diode if motor type requires it. |

## 4. Analog Audio Interface (SGTL5000 <-> SA818)

| Source          | Destination | Net            | Required conditioning |
|-----------------|-------------|----------------|-----------------------|
| SGTL5000 DAC out| SA818 AF_IN | AF_TX_COUPLED  | AC coupling capacitor + optional trim attenuation network footprint. |
| SA818 AF_OUT    | SGTL5000 ADC| AF_RX_COUPLED  | AC coupling capacitor + optional RC low-pass footprint. |

Analog design rules:
- Keep AF paths short, away from antenna and switching regulators.
- Use a dedicated analog ground island tied at a single low-impedance point.
- Provide footprints for optional gain trim (resistor divider/pot footprint during EVT).
- Add test pads for AF_TX_COUPLED and AF_RX_COUPLED.
- Bench result: the PJRC 3.5 mm jack is confirmed as headphone output. Input testing should use the separate `LINE_IN` header, not the headphone jack.

Bench-verified operator setup:
- To verify codec output before SA818 integration, plug standard `TRS` headphones into the PJRC 3.5 mm jack.
- To verify codec input before SA818 integration, inject a line-level signal into the PJRC `LINE_IN` header (`L` + `GND`, optionally `R` + `GND` for stereo source).
- Do not use the PJRC headphone jack as a microphone input path for this project stage.
- Current SA818 bench-test path is:
  - `SGTL5000 LINE_OUT_L -> AC coupling / attenuation -> SA818 AF_IN`
  - `SA818 AF_OUT -> AC coupling -> SGTL5000 LINE_IN_L`
- Bench status so far:
  - headphone output is confirmed audible
  - `LINE_IN` monitor shows strong input signal when driven
  - SA818 staged bench code now exercises UART, config, PTT, RX-audio window, and short TX-audio tone

## 5. RF and External Connectors

- SA818 ANT pin -> 50-ohm controlled path -> SMA connector center pin.
- Maintain solid ground around antenna connector and SA818 RF section.
- Add ESD protection on USB and user-exposed lines.
- Keep high-speed digital lines out of RF keep-out region.

## 6. Battery and Charging

- Prefer MCP73831/2 Li-ion charger to match Adafruit Feather ESP32-S3 behavior.
- Use MAX17048 as fuel gauge baseline for prototype parity.
- If using USB-C receptacle, include CC resistors for sink configuration.
- Add battery connector reverse-polarity protection strategy.
- Provide charger status output to LED or MCU GPIO if available.

## 7. Suggested Supporting Circuit Footprints

Add these as DNP-capable where uncertain in EVT:
- PTT level-shift/transistor stage footprint.
- AF_TX attenuation resistor ladder footprint.
- AF_RX RC filter footprint.
- Ferrite bead between `V_SYS_3V3` and `V_AUD_3V3`.
- Extra bulk capacitor pads on `V_RADIO`.
- Optional series resistors (22 to 47 ohm) on I2S lines for edge control.

## 8. Pin Reservation and Bring-Up Notes

- Keep boot-critical ESP32 pins out of external pull conflicts.
- Avoid assigning strapping-sensitive pins to external circuits that can force boot states.
- Keep one spare GPIO for recovery/diagnostics.
- Add SW/serial debug header access for early firmware bring-up.

## 9. Schematic Handoff Netlist Summary

Core named nets to preserve in schematic:
- Power: `VBAT_RAW`, `V_RADIO`, `V_SYS_3V3`, `V_AUD_3V3`, `GND`.
- Audio: `AF_TX_COUPLED`, `AF_RX_COUPLED`.
- Radio control: `SA818_PTT`, `SA818_RX_CTRL`, `SA818_TX_STAT`.
- GPS: `GPS_TX_NMEA`, `GPS_RX_CTRL`, optional `GPS_PPS`.
- I2C: `I2C_SDA`, `I2C_SCL`.
- I2S: `I2S_BCLK`, `I2S_WS`, `I2S_DOUT`, `I2S_DIN`, `I2S_MCLK`.
- UI: `LED_STATUS_G`, `LED_RX_B`, `LED_TX_R`, `BTN_FUNC_N`, `HAPTIC_DRV`.

## 10. Pre-PCB Validation Checklist

Before committing to PCB rev A:
1. Confirm SA818 supply voltage/current limits on exact purchased module.
2. Confirm SA818 PTT polarity and UART electrical levels.
3. Measure required AF_TX level for proper deviation (target packet decode quality).
4. Measure AF_RX level/noise floor into codec path.
5. Verify no ESP32 resets during repeated TX bursts.
6. Verify GPS lock and UART stability while TX/RX active.
7. Verify I2C bus rise time with all attached peripherals.
8. Verify BLE operation while RX audio pipeline is active.
9. Verify SGTL5000 `SYS_MCLK` frequency/ratio configuration is stable at selected sample rate.
10. Keep the firmware bring-up order as: power bus -> enable QT/STEMMA pull-up path -> start I2S/MCLK -> init SGTL5000 over I2C.
11. Preserve these working SGTL5000 setup assumptions in firmware unless bench data disproves them:
    `CHIP_I2S_CTRL=0x0030`, `CHIP_SSS_CTRL=0x0010`, `CHIP_ANA_CTRL=0x0006`, `CHIP_SHORT_CTRL=0x4446`, final `CHIP_ANA_POWER=0x40FF`.
12. Confirm headphone output with the bench audio test before wiring SA818.
13. Use `LINE_IN` as the correct SA818-side receive-audio target on the SGTL5000 board.
14. Validate SA818 UART on `GPIO13/GPIO9` with `AT+DMOCONNECT` before attempting RF tests.
15. Run only short, supervised TX-audio tests until deviation is measured and tuned.
