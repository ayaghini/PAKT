# PCB Component List

This is the first-pass component list for KiCad capture. It is organized around
the confirmed bench baseline and is intended for review before schematic entry.

## Status keys
- `locked` — selected from the bench-proven baseline.
- `reference-derived` — taken from the Adafruit reference design and should be reviewed.
- `tuning` — expected to be finalized from bench measurements or datasheet review.

## Core active components

| Subsystem | Item | Component / value | Package / form | Status | Notes |
|---|---|---|---|---|---|
| MCU | U1 | ESP32-S3 module, matching Adafruit ESP32-S3 Feather `4MB Flash / 2MB PSRAM` | module | locked | Match the exact bench board family. |
| Radio | U2 | `SA818-V` | module | locked | Bench-proven radio baseline. |
| Codec | U3 | `SGTL5000` | QFN-32 | locked | Bench-proven audio codec. |
| GPS | U4 | u-blox `NEO-M9N` class module | module | locked | Shared-I2C baseline, UART fallback optional. |
| Fuel gauge | U5 | `MAX17048` | TDFN-8 | locked | Use the `MAX17048` family, not `MAX17043`. |
| Charger | U6 | `MCP73831T-2ACI/OT` | SOT-23-5 | locked | Adafruit-style single-cell charger baseline. |
| 3V3 regulator | U7 | main `3.3V` regulator | TBD | reference-derived | Final part depends on current budget and power-path strategy. |

## Battery / USB / charge section

| Item | Component / value | Package / form | Status | Notes |
|---|---|---|---|---|
| J1 | USB-C receptacle | USB-C | locked | USB device + charging input. |
| R1 | `5.1 kOhm` `CC1` resistor | 0603 | reference-derived | USB-C sink configuration. |
| R2 | `5.1 kOhm` `CC2` resistor | 0603 | reference-derived | USB-C sink configuration. |
| J2 | JST-PH-2 battery connector | through-hole / SMT hybrid | locked | Single-cell Li-ion battery connector. |
| U6 | `MCP73831T-2ACI/OT` | SOT-23-5 | locked | Charger IC. |
| R3 | `PROG` resistor, start with `5.1 kOhm` | 0603 | reference-derived | Review against desired battery charge current. |
| D1 | Charge LED | 0603 | reference-derived | Driven from charger `STAT`. |
| R4 | Charge LED resistor, start with `5.1 kOhm` | 0603 | reference-derived | Mirrors Adafruit approach. |
| C1 | Charger input bulk cap, start with `10 uF` | 0805 | reference-derived | Place close to charger input. |
| C2 | Battery-node bulk cap, start with `10 uF` | 0805 | reference-derived | Place close to battery node / charger. |
| U5 | `MAX17048` | TDFN-8 | locked | Fuel gauge IC. |
| TP1 | `VBAT_RAW` test point | test pad | locked | Required for bring-up. |

## ESP32-S3 support

| Item | Component / value | Package / form | Status | Notes |
|---|---|---|---|---|
| SW1 | Reset button | tact switch | locked | Keep on Rev A. |
| SW2 | Boot / recovery button | tact switch | locked | Keep on Rev A. |
| R5 | `EN` pull-up, start with `10 kOhm` | 0603 | reference-derived | Mirrors Feather-class support network. |
| C3 | `EN` / reset timing capacitor, start with `1 uF` | 0603 | reference-derived | Review with final module/reset topology. |
| J3 | Debug / programming header | header or tag-connect | locked | Keep early bring-up easy. |
| C4..C7 | ESP32 supply decoupling caps | `100 nF + bulk` | tuning | Final quantity depends on selected module and regulator. |

## I2C shared bus

| Item | Component / value | Package / form | Status | Notes |
|---|---|---|---|---|
| R6 | `I2C_SDA` pull-up, `2.2 kOhm` to `4.7 kOhm` | 0603 | tuning | Only one effective pull-up set on the bus. |
| R7 | `I2C_SCL` pull-up, `2.2 kOhm` to `4.7 kOhm` | 0603 | tuning | Shared by SGTL5000, MAX17048, GPS. |

## SGTL5000 audio section

| Item | Component / value | Package / form | Status | Notes |
|---|---|---|---|---|
| U3 | `SGTL5000` | QFN-32 | locked | Main audio codec. |
| C8..C15 | Codec supply decoupling network | mixed | tuning | Pull exact values from the NXP datasheet during schematic capture. |
| FB1 | Ferrite bead for `V_AUD_3V3` | 0603 | reference-derived | DNP-capable if not needed. |
| C16 | `AF_TX` AC-coupling cap, start with `1 uF` | 0603/0805 | locked | Between codec output and SA818 AF input network. |
| R8 / R9 | `AF_TX` attenuation network | 0603 | tuning | Final values depend on measured TX deviation. |
| C17 | `AF_RX` AC-coupling cap, start with `1 uF` | 0603/0805 | locked | Between SA818 AF output and codec line input. |
| R10 / C18 | Optional `AF_RX` RC filter | 0603 | tuning | DNP-capable bench-tuning network. |

## SA818 radio section

| Item | Component / value | Package / form | Status | Notes |
|---|---|---|---|---|
| U2 | `SA818-V` | module | locked | Main radio module. |
| C19 | Radio local decoupling `100 nF` | 0603 | locked | Place at module supply entry. |
| C20 | Radio local bulk `10 uF` | 0805/1206 | locked | Place at module supply entry. |
| C21 | Radio burst bulk `220 uF` to `470 uF` | electrolytic / polymer | tuning | Final value to be confirmed from TX supply droop measurement. |
| Q1 | Optional PTT transistor stage | SOT-23 | tuning | DNP unless direct drive proves weak/noisy. |
| R11 / R12 | Optional PTT bias resistors | 0603 | tuning | Populate only if transistor stage is used. |
| J4 | Antenna connector | SMA or U.FL | locked | Depends on enclosure/mechanics. |

## GPS section

| Item | Component / value | Package / form | Status | Notes |
|---|---|---|---|---|
| U4 | `NEO-M9N` class module | module | locked | Shared-I2C baseline. |
| C22..C24 | GPS local decoupling caps | mixed | tuning | Pull exact values from the chosen module datasheet. |
| TP2 | Optional PPS test point | test pad | reference-derived | Useful during bring-up. |

## User interface / support

| Item | Component / value | Package / form | Status | Notes |
|---|---|---|---|---|
| D2 | Status LED | 0603 | reference-derived | Firmware-controlled. |
| D3 | RX LED | 0603 | reference-derived | Firmware-controlled. |
| D4 | TX LED | 0603 | reference-derived | Firmware-controlled. |
| R13..R15 | LED resistors | 0603 | tuning | Final value depends on LED choice and brightness target. |
| SW3 | Function button | tact switch | reference-derived | Optional but useful on Rev A. |
| TP3..TP6 | `V_SYS_3V3`, `V_RADIO`, `AF_TX_COUPLED`, `AF_RX_COUPLED` | test pads | locked | Strongly recommended. |

## Open review items before schematic entry
1. Lock the exact ESP32-S3 module orderable.
2. Confirm the main `3.3V` regulator part and current headroom.
3. Decide whether `MAX17048 !ALRT` is routed to the MCU.
4. Confirm the desired charger current and therefore the final `PROG` resistor.
5. Replace the tuning placeholders in the SGTL5000 and GPS sections with datasheet-derived values.
