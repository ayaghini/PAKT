# Rev01 BOM Notes

This folder now includes two BOM helper files:

- `rev01_bom_preferred.csv`: one line per reference with preferred MPN/manufacturer.
- `rev01_bom_grouped.csv`: grouped line items with quantity and reference list.

## Packaging strategy applied

- Resistors: standardized to `0603` (`R_0603_1608Metric`).
- Ferrite beads: standardized to `0603` (`BLM18PG121SN1D` family).
- Capacitors:
  - `100n`, `1u`, `2.2u`: mostly `0603`.
  - `4.7u`, `10u`: mostly `0805` for capacitance derating margin.
  - `47pF` RF matching capacitor (`C26`): `0402`.
  - `100u` bulk (`C10`): kept `1206`.
  - Polymer bulk (`C33`): kept custom footprint.
- LEDs: unified to `0603`.

## Important review items before release

- Confirm exact ESP32 module variant for production order (`U2` flash/PSRAM option).
- Confirm exact GPS module order code (`U1`, e.g. `NEO-M8N-0` vs `NEO-M8M-0`).
- Verify RF-matching/passive choices (`L3`, `C26`, `R23`) against measured RF results.
- Verify TVS part selection (`D5`) against final USB protection requirement.
- Treat `DNP` entries as not populated unless intentionally enabled.
