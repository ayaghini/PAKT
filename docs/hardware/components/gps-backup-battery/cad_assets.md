# CAD Assets

This folder tracks schematic/PCB assets for the GPS backup battery support path.

## Symbol intent
- Battery symbol (`Device:Battery_Cell` or project-local equivalent).
- Schottky diode symbol (`Device:D_Schottky`).
- Optional 0 ohm link symbol (`Device:R` with value `0R`, default DNP).

## Footprint intent
- Battery:
  - Prefer the exact footprint matching selected tab-cell package dimensions for `BR-1225A/FAN`.
- Schottky diode:
  - `SOD-323` or `SOD-523` class footprint (based on chosen diode part).
- Optional link resistor:
  - `0402`/`0603` according to assembly preference.

## Verification checklist
1. Battery footprint polarity and keepout are correct.
2. `V_BCKP` diode orientation is battery -> `V_BCKP`.
3. `VCC -> V_BCKP` 0 ohm link is marked DNP by default when battery is used.
