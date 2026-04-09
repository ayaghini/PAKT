# MAX17048 CAD Assets

## KiCad symbol
- Local symbol file: `symbols/MAX17048.kicad_sym`
- Symbol name: `MAX17048`
- Pinout source: Analog Devices datasheet, cross-checked against the Adafruit MAX17048 Feather-family usage.

## Footprint
- Preferred footprint: `footprints/DFN-8-1EP_3x2mm_P0.5mm_EP1.7x1.4mm.kicad_mod`
- Package class: `TDFN/UTDFN-8`, `3.0 mm x 2.0 mm`, exposed pad.

## 3D model
- Current STEP model: `step/DFN-8-1EP_3x2mm_P0.5mm_EP1.75x1.45mm.step`
- Status: close mechanical reference; confirm exposed-pad dimensions before release.

## KiCad capture guidance
- Connect `CELL` and `VDD` to `VBAT_RAW` as in the Feather-style battery-sense topology.
- Tie `QSTRT` low unless there is a firmware reason to expose it.
- Keep `SCL` and `SDA` on the shared battery-side I2C bus.
- Decide early whether `!ALRT` will be routed to the MCU or left unused with a test pad.

## Verification checklist
- Confirm the final orderable package suffix matches the land pattern.
- Confirm the alert pin treatment before schematic freeze.
- Keep the fuel-gauge routing physically close to the battery rail sense point.
