# PCB Wiring Baseline

This document is the wiring baseline for the first KiCad schematic. It now
reflects the confirmed bench configuration rather than earlier exploratory
hardware plans.

For the physical breakout harness used on the bench, see:
- `hardware/prototype_breakout_wiring_plan.md`

## Scope and assumptions
- Target architecture: ESP32-S3 + SA818-V + SGTL5000 + u-blox M9N + MAX17048.
- Logic domain is `3.3V` unless a module explicitly requires otherwise.
- Battery charging and battery gauging should follow the Adafruit ESP32-S3 Feather with `4MB Flash / 2MB PSRAM` bench-board behavior.
- SA818 supply/current and AF levels still require final measured values before layout release.

## 1. Power tree

### Functional flow
1. USB-C `VBUS` -> `MCP73831T-2ACI/OT` charger input.
2. Charger `VBAT` -> single-cell Li-ion battery connector.
3. `VBAT_RAW` -> `MAX17048 CELL/VDD`.
4. `VBAT_RAW` -> system power path / regulator path.
5. Switched or regulated rails feed:
   - `V_RADIO` for the SA818 path
   - `V_SYS_3V3` for ESP32-S3 digital logic
   - `V_AUD_3V3` as the filtered codec analog rail

### Required rails
- `VBUS`
- `VBAT_RAW`
- `V_SYS_3V3`
- `V_AUD_3V3`
- `V_RADIO`
- `GND`

### Power integrity rules
- Place local bulk capacitance near the SA818 supply entry.
- Place `100 nF` decoupling at every IC power pin with the shortest possible loop.
- Add bulk capacitance near the ESP32-S3 supply entry.
- Keep SA818 TX current return out of the codec analog ground path.
- Include test points for `VBAT_RAW`, `V_SYS_3V3`, and `V_RADIO`.
- Keep charger, battery connector, and fuel gauge in the same battery-power zone.

## 2. Shared I2C control bus

| ESP32-S3 pin | Net | Devices |
|---|---|---|
| GPIO3 | `I2C_SDA` | SGTL5000, MAX17048, GPS |
| GPIO4 | `I2C_SCL` | SGTL5000, MAX17048, GPS |

Rules:
- Only one effective pull-up set on the bus.
- Keep the bus short and away from the SA818 RF area.
- Bench-verified devices on this bus are `SGTL5000 @ 0x0A`, `MAX17048 @ 0x36`, and `u-blox M9N @ 0x42`.
- The SGTL5000 does not answer cleanly until `SYS_MCLK` is running, so start `I2S/MCLK` first, then run codec I2C init.

## 3. I2S audio bus

| ESP32-S3 pin | Net | SGTL5000 pin |
|---|---|---|
| GPIO8 | `I2S_BCLK` | `SCLK` |
| GPIO15 | `I2S_WS` | `LRCLK` |
| GPIO12 | `I2S_DOUT` | `DIN` |
| GPIO10 | `I2S_DIN` | `DOUT` |
| GPIO14 | `I2S_MCLK` | `SYS_MCLK` |

Rules:
- `I2S_MCLK` is mandatory for the SGTL5000.
- Keep these traces short and referenced to a continuous ground plane.
- Reserve optional `22 Ohm` to `47 Ohm` series resistors at the MCU side if edge control is needed.
- The earlier pin-conflict draft is obsolete. The current bench profile uses `GPIO15` for `I2S_WS` and `GPIO13/GPIO9/GPIO11` for radio control without overlap.

## 4. UART and control links

| Function | ESP32-S3 pin | Net | Remote pin |
|---|---|---|---|
| GPS TX | GPIO17 | `GPS_RX_CTRL` | GPS RXD |
| GPS RX | GPIO18 | `GPS_TX_NMEA` | GPS TXD |
| Radio TX | GPIO13 | `SA818_RX_CTRL` | SA818 RXD |
| Radio RX | GPIO9 | `SA818_TX_STAT` | SA818 TXD |
| PTT | GPIO11 | `SA818_PTT` | SA818 PTT |

Optional:
- Route GPS PPS to a spare interrupt-capable GPIO if board space allows.

## 5. Analog audio interface

| Source | Destination | Net | Starting network |
|---|---|---|---|
| `SGTL5000 LINE_OUT_L` | SA818 `AF_IN` | `AF_TX_COUPLED` | AC coupling capacitor + DNP attenuation network |
| SA818 `AF_OUT` | `SGTL5000 LINE_IN_L` | `AF_RX_COUPLED` | AC coupling capacitor + optional DNP RC filter |

Rules:
- Keep the analog path short and away from RF and switching currents.
- Add test pads for `AF_TX_COUPLED` and `AF_RX_COUPLED`.
- Preserve the bench-proven left-channel path for the first PCB pass.

## 6. Battery and charging

- Follow the Adafruit ESP32-S3 Feather with `4MB Flash / 2MB PSRAM` battery behavior for:
  - `MCP73831T-2ACI/OT`
  - `MAX17048`
  - USB-C sink `CC` resistors
  - charger `STAT` LED path
- Keep `MAX17048 CELL` and `VDD` on `VBAT_RAW`.
- Tie `MAX17048 QSTRT` low unless a firmware-controlled quick-start function is intentionally added.
- Decide whether `MAX17048 !ALRT` goes to the ESP32 or to a test pad only.
- Provide charger status output to LED and optionally to an MCU-readable node if that adds value.

## 7. Required named nets

- Power: `VBUS`, `VBAT_RAW`, `V_SYS_3V3`, `V_AUD_3V3`, `V_RADIO`, `GND`
- Audio: `AF_TX_COUPLED`, `AF_RX_COUPLED`
- Radio control: `SA818_PTT`, `SA818_RX_CTRL`, `SA818_TX_STAT`
- GPS: `GPS_TX_NMEA`, `GPS_RX_CTRL`, optional `GPS_PPS`
- I2C: `I2C_SDA`, `I2C_SCL`
- I2S: `I2S_BCLK`, `I2S_WS`, `I2S_DOUT`, `I2S_DIN`, `I2S_MCLK`

## 8. Pre-PCB validation checklist
1. Confirm SA818 supply voltage/current limits on the exact purchased module.
2. Confirm SA818 PTT polarity and UART electrical levels.
3. Measure the required `AF_TX` level for proper deviation.
4. Measure `AF_RX` level/noise floor into the codec path.
5. Verify no ESP32 resets during repeated TX bursts.
6. Verify GPS lock and shared-I2C stability while TX/RX is active.
7. Verify I2C bus rise time with SGTL5000, MAX17048, and GPS attached.
8. Verify BLE operation while the RX audio pipeline is active.
9. Verify SGTL5000 `SYS_MCLK` frequency/ratio configuration is stable at the selected sample rate.
10. Keep the firmware bring-up order as: power bus -> enable QT/STEMMA pull-up path -> start I2S/MCLK -> init SGTL5000 over I2C.
11. Preserve these working SGTL5000 setup assumptions in firmware unless bench data disproves them:
    `CHIP_I2S_CTRL=0x0030`, `CHIP_SSS_CTRL=0x0010`, `CHIP_ANA_CTRL=0x0006`, `CHIP_SHORT_CTRL=0x4446`, final `CHIP_ANA_POWER=0x40FF`
12. Confirm headphone output with the bench audio test before wiring SA818.
13. Validate SA818 UART on `GPIO13/GPIO9` with `AT+DMOCONNECT` before RF tests.
14. Run only short, supervised TX-audio tests until deviation is measured and tuned.
