# CAD Assets

This folder tracks schematic symbols, PCB footprints, and 3D STEP models for this component.

## Symbol (KiCad)
- Place .kicad_sym files in symbols/.
- Preferred naming: <component>.kicad_sym.

## Footprint (KiCad)
- Place footprint library folders or .kicad_mod files in ootprints/.
- Preferred naming: <component>.pretty/.

## 3D Model (STEP)
- Place .step/.stp files in step/.
- Align model origin/orientation to the KiCad footprint before PCB release.

## Verification checklist
- Symbol pin numbers match datasheet package pinout.
- Footprint pads, courtyard, and orientation match package drawing.
- 3D model aligns with footprint and board top/bottom side as intended.