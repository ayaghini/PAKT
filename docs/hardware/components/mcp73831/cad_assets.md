# MCP73831 CAD Assets

## KiCad symbol
- Local symbol file: `symbols/MCP73831.kicad_sym`
- Symbol name: `MCP73831T-2ACI/OT`
- Pinout: `STAT`, `VSS`, `VBAT`, `VDD`, `PROG`

## Footprint
- Preferred footprint library: KiCad standard
- Preferred footprint: `Package_TO_SOT_SMD:SOT-23-5`
- Important note: the old placeholder DFN footprint in this folder is not the active package choice for this project.

## 3D model
- Preferred model source: KiCad standard SOT-23-5 model
- No custom local STEP model is required unless mechanical enclosure work demands it.

## KiCad capture guidance
- `VDD` goes to `VBUS`.
- `VBAT` goes to the raw single-cell battery node.
- `PROG` uses the charge-current set resistor.
- `STAT` can drive a charge LED directly in the Adafruit-style reference topology.

## Verification checklist
- Match the selected `MCP73831T-2ACI/OT` package drawing before layout release.
- Reconfirm the final `PROG` resistor against the intended battery charge current.
- Keep the charger input, battery connector, and battery bulk capacitor physically tight.
