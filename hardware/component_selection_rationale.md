# Confirmed Hardware Baseline

This document replaces the earlier option-screening rationale with the
bench-confirmed hardware baseline that should drive KiCad schematic capture.

## Locked subsystem choices

| Subsystem | Locked baseline | Status | PCB implication |
|-----------|-----------------|--------|-----------------|
| MCU + BLE/Wi-Fi | Adafruit ESP32-S3 Feather with `4MB Flash / 2MB PSRAM` bench baseline | locked | Match the exact bench board family for power-domain behavior and proven GPIO usage. |
| RF modem module | `SA818-V` | locked | Keep UART + PTT + AF wiring unchanged from the bench profile. |
| Audio codec | `SGTL5000` | locked | Preserve `SYS_MCLK` requirement and the proven `LINE_OUT_L` / `LINE_IN_L` radio path. |
| GPS | u-blox `NEO-M9N` class | locked | Keep shared-I2C integration as the default baseline; retain UART fallback pads if space allows. |
| Fuel gauge | `MAX17048` | locked | Follow the Adafruit ESP32-S3 Feather `4MB Flash / 2MB PSRAM` bench-board behavior and pin usage. |
| Charger | `MCP73831T-2ACI/OT` topology | locked | Follow the same Feather bench-board charging behavior. |

## What is no longer part of the active baseline
- `WM8960` is no longer an active codec candidate.
- `MAX17043` is no longer an active fuel-gauge candidate.
- Older Feather battery-gauge topologies that use `LC709203` are not the reference for this PCB.
- The optional SSD1306 display is not part of the first PCB capture baseline.

## Bench-confirmed design rules to preserve

### ESP32-S3
- Preserve the proven bus and control mapping:
  - `GPIO3` -> `I2C_SDA`
  - `GPIO4` -> `I2C_SCL`
  - `GPIO8` -> `I2S_BCLK`
  - `GPIO15` -> `I2S_WS`
  - `GPIO12` -> `I2S_DOUT`
  - `GPIO10` -> `I2S_DIN`
  - `GPIO14` -> `I2S_MCLK`
  - `GPIO13` -> `SA818_RX_CTRL`
  - `GPIO9` -> `SA818_TX_STAT`
  - `GPIO11` -> `SA818_PTT`
  - `GPIO17/GPIO18` -> GPS UART fallback
- Keep UART debug/programming access on Rev A.

### SA818-V
- Preserve active-low PTT behavior.
- Keep local radio bulk capacitance near the module.
- Keep the antenna path short, controlled, and isolated from high-speed digital routing.

### SGTL5000
- Keep `SYS_MCLK` explicit in the schematic and routing plan.
- Preserve the firmware bring-up order: start `I2S/MCLK`, then configure the codec over I2C.
- Keep AC-coupled analog paths with DNP tuning footprints for TX attenuation and RX filtering.

### GPS
- Shared I2C is the default integration baseline because it is bench-proven.
- UART fallback remains useful for recovery, alternate modules, or debug.
- Add optional PPS if routing budget permits.

### Battery subsystem
- Mirror the Adafruit ESP32-S3 Feather with `4MB Flash / 2MB PSRAM` battery behavior so bench wiring and custom PCB behavior stay aligned.
- Keep charger, battery connector, bulk cap, and fuel gauge physically close to the battery entry area.
- Add test points for `VBAT_RAW`, `V_SYS_3V3`, `V_RADIO`, and charger status if possible.

## Schematic requirements generated from the confirmed baseline
1. Explicit `VBUS`, `VBAT_RAW`, `V_SYS_3V3`, `V_AUD_3V3`, and `V_RADIO` nets.
2. Explicit `I2S_MCLK` net from ESP32-S3 to SGTL5000.
3. DNP footprints for AF gain shaping and receive filtering.
4. DNP footprint for an optional PTT transistor stage.
5. USB-C sink `CC1/CC2` resistors and ESD protection on external connectors.
6. Radio bulk capacitance close to the SA818 supply entry.

## PCB release gates
1. SA818 supply droop and PTT polarity remain stable on the bench baseline.
2. TX deviation has measured, repeatable attenuation values.
3. RX decode remains repeatable on the proven SGTL5000 path.
4. No brownout or reset occurs during repeated TX cycles.
5. Every active BOM line has at least one real sourceable candidate and a verified package.
