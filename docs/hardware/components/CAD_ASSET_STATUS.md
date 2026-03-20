# CAD Asset Status

This file records which CAD assets are exact matches and which are provisional.

## Status legend
- `exact`: symbol/footprint/model matches the intended component or module.
- `provisional`: usable starting point, but package/symbol/model needs final verification before PCB release.

## Component matrix
| Component | Symbol | Footprint | STEP | Status | Notes |
|---|---|---|---|---|---|
| `esp32-s3` | `RF_Module.kicad_sym` | `ESP32-S3-WROOM-1.kicad_mod` | `ESP32-S3-WROOM-1.step` | exact | From KiCad 9 default libraries. |
| `sa818` | `SA818S.kicad_sym` | `XCVR_SA818S.kicad_mod` | `SA818S.step` | exact | Imported from existing local `uConsole_HAM_HAT` component library. |
| `sgtl5000` | `Audio.kicad_sym` | `QFN-32-1EP_5x5mm_P0.5mm_EP3.6x3.6mm.kicad_mod` | `QFN-32-1EP_5x5mm_P0.5mm_EP3.7x3.7mm.step` | provisional | Closest local STEP available is EP `3.7x3.7`. |
| `neo-m8n` | `RF_GPS.kicad_sym` | `ublox_NEO.kicad_mod` | `NEO-M9N-00B.step` | provisional | Local KiCad install lacked `ublox_NEO.step`; using close u-blox module STEP. |
| `mcp73831` | `Battery_Management.kicad_sym` | `DFN-8-1EP_3x2mm_P0.5mm_EP1.7x1.4mm.kicad_mod` | `DFN-8-1EP_3x2mm_P0.5mm_EP1.75x1.45mm.step` | provisional | Closest local STEP variant used. |
| `max17048` | `Battery_Management.kicad_sym` | `DFN-8-1EP_3x2mm_P0.5mm_EP1.7x1.4mm.kicad_mod` | `DFN-8-1EP_3x2mm_P0.5mm_EP1.75x1.45mm.step` | provisional | KiCad default libs do not include a dedicated MAX17048 symbol in this environment. |
| `max17043` | `Battery_Management.kicad_sym` | `DFN-8-1EP_3x2mm_P0.5mm_EP1.7x1.4mm.kicad_mod` | `DFN-8-1EP_3x2mm_P0.5mm_EP1.75x1.45mm.step` | provisional | Legacy alternate only; dedicated symbol not found in local default libs. |
| `wm8960` | `Audio.kicad_sym` | `QFN-32-1EP_5x5mm_P0.5mm_EP3.45x3.45mm.kicad_mod` | `QFN-32-1EP_5x5mm_P0.5mm_EP3.45x3.45mm.step` | provisional | Legacy codec reference; dedicated WM8960 symbol not found in local default libs. |

## Pre-release checks required
1. Confirm pin mapping for `max17048` symbol selection before schematic freeze.
2. Confirm exposed pad and body dimensions for `mcp73831` and `sgtl5000` STEP choices.
3. Replace `neo-m8n` STEP with exact NEO-M8N model if strict mechanical fit is required.
4. If `wm8960` is reintroduced, import a dedicated WM8960 symbol before use.
