# CAD Asset Status

This file tracks the CAD assets that matter for the current PCB baseline.

## Status legend
- `exact`: ready to use as-is for the active hardware baseline.
- `provisional`: usable with explicit pre-schematic verification.
- `reference-only`: archived or historical, not part of the active PCB baseline.

## Active component matrix
| Component | Symbol | Footprint | STEP | Status | Notes |
|---|---|---|---|---|---|
| `esp32-s3` | `RF_Module.kicad_sym` | `ESP32-S3-WROOM-1.kicad_mod` | `ESP32-S3-WROOM-1.step` | provisional | Use as a module reference only after matching the exact `4 MB / 2 MB PSRAM` module variant for the PCB. |
| `sa818` | `SA818S.kicad_sym` | `XCVR_SA818S.kicad_mod` | `SA818S.step` | exact | Bench baseline radio module. |
| `sgtl5000` | `Audio.kicad_sym` | `QFN-32-1EP_5x5mm_P0.5mm_EP3.6x3.6mm.kicad_mod` | `QFN-32-1EP_5x5mm_P0.5mm_EP3.7x3.7mm.step` | provisional | Symbol is fine; confirm exposed-pad size and 3D fit before PCB release. |
| `neo-m8n` | `RF_GPS.kicad_sym` | `ublox_NEO.kicad_mod` | `NEO-M9N-00B.step` | provisional | Folder name is legacy; current bench baseline is `NEO-M9N`. |
| `max17048` | `MAX17048.kicad_sym` | `DFN-8-1EP_3x2mm_P0.5mm_EP1.7x1.4mm.kicad_mod` | `DFN-8-1EP_3x2mm_P0.5mm_EP1.75x1.45mm.step` | provisional | Dedicated local symbol added; confirm package drawing against the exact orderable suffix before release. |
| `mcp73831` | `MCP73831.kicad_sym` | `Package_TO_SOT_SMD:SOT-23-5` | standard KiCad 3D model | provisional | Active charger package is `SOT-23-5`; do not use the old placeholder DFN mapping. |

## Archived references
| Component | Status | Notes |
|---|---|---|
| `max17043` | reference-only | Archived alternate, not part of the current PCB baseline. |
| `wm8960` | reference-only | Archived codec alternate, not part of the current PCB baseline. |

## Pre-schematic checks still required
1. Lock the exact ESP32-S3 module orderable tied to the final antenna/module strategy.
2. Confirm the SGTL5000 exposed pad and analog supply pin decoupling network from the NXP datasheet.
3. Confirm the exact `NEO-M9N` mechanical model if enclosure clearance is tight.
4. Verify the MAX17048 and MCP73831 package land patterns against the selected manufacturers' package drawings before fab release.
