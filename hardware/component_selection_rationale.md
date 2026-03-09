# Component Selection Rationale (PCB-Oriented)

This document records the current component decisions and readiness for schematic/PCB capture.

## 1. Decision status summary

| Subsystem | Selected | Status | Reason |
|-----------|----------|--------|--------|
| MCU + BLE/Wi-Fi | ESP32-S3 module with PSRAM | Locked | Strong ecosystem, enough compute for AFSK + BLE, available dev modules. |
| RF modem module | SA818 (VHF variant) | Locked for MVP | Fastest path for APRS RF MVP with UART + AF/PTT interface. |
| Audio codec | SGTL5000 | Locked for prototype and Rev A baseline | Better lifecycle confidence, strong dev-board ecosystem, sufficient feature set for mono APRS modem path. |
| GPS | U-blox NEO-M8N class | Locked | Strong field performance and common UART workflow. |
| Fuel gauge | MAX17048 | Locked | Matches Adafruit Feather ESP32-S3 baseline for prototype-to-PCB continuity. |
| Charger | MCP73831/2 | Locked for PCB | Matches Adafruit Feather ESP32-S3 charging topology and simplifies migration. |
| Display | 0.96" SSD1306 I2C | Optional for MVP board | Useful for bring-up and diagnostics; can be depopulated in compact builds. |

## 2. Validation notes by subsystem

### 2.1 ESP32-S3 module
- Use a certified module variant where possible to reduce RF and manufacturing risk.
- Keep UART debug/programming access in PCB rev A.
- Reserve one spare GPIO for recovery and test instrumentation.

### 2.2 SA818 module
- Key interface dependencies to verify before release:
  - supply voltage range on exact module revision
  - TX burst current and rail droop
  - PTT active level and default state
  - AF input/output level expectations
- Keep antenna feed short and controlled; place module away from high-speed digital lines.

### 2.3 SGTL5000 codec
- Confirm I2C address strap and control bus pull-ups in schematic notes.
- SGTL5000 requires stable `SYS_MCLK`; reserve explicit ESP32 MCLK-capable output pin.
- Place optional analog gain/attenuation footprints between codec and SA818.
- Split analog and digital supply filtering on PCB where feasible.

### 2.4 GPS module (NEO-M8N class)
- Include optional PPS connection footprint to MCU.
- Keep GPS RF area physically separated from SA818 and noisy switching nodes.

### 2.5 Power subsystem
- Keep charger and battery protection close to connector/battery entry.
- Isolate radio rail return from sensitive analog and MCU ground paths.
- Add test points for `VBAT_RAW`, `V_SYS_3V3`, `V_RADIO`.

## 3. Components to keep as alternatives (do not block schematic)

- Codec alternates: TLV320AIC3204, TLV320AIC3101/AIC3104.
- Fuel gauge alternates: LC709203F, MAX17043.
- Charger alternate: BQ24075 family if power-path behavior is required.

## 4. Schematic requirements generated from component choices

1. DNP footprints for AF gain shaping and filtering.
2. DNP footprint for PTT transistor stage if direct drive proves marginal.
3. Ferrite-bead option between digital 3.3V and codec analog rail.
4. USB-C sink CC resistors and ESD protection on external connectors.
5. Radio supply bulk capacitance near SA818 power entry.
6. Explicit `I2S_MCLK` net from ESP32-S3 to SGTL5000.

## 5. Release gates for PCB Rev A

A component set is considered validated for PCB when all of the following are true:
1. Bench prototype confirms SA818 supply and PTT electrical behavior.
2. Bench calibration identifies stable TX deviation settings.
3. RX path demonstrates acceptable decode rate without clipping/noise floor collapse.
4. No repeatable brownout/reset during TX cycles.
5. BOM contains at least one purchasable candidate per line item with footprint compatibility.
6. SGTL5000 clock tree is validated for selected sample rates.
