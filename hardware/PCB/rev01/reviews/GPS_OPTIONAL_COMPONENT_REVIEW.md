# GPS Review 01: NEO-M9N / NEO-M8N-Layout Optional Support Components

Scope: GPS section only (as requested), focused on supply stability, filtering, and startup behavior.

Primary references:
- NEO-M9N Integration Manual (UBX-19014286, R10, 2025-03-27)
  - https://content.u-blox.com/sites/default/files/NEO-M9N_Integrationmanual_UBX-19014286.pdf
- NEO-M9N-00B Datasheet (UBX-19014285, R08, 2025-10-09)
  - https://www.u-blox.com/sites/default/files/NEO-M9N-00B_DataSheet_UBX-19014285.pdf

---

## 1. Decisions You Should Make Now (GPS-only)

### A) Backup domain strategy (`V_BCKP`)
- Why: without `V_BCKP`, module cold-starts each boot.
- Datasheet/integration guidance: connect backup battery to `V_BCKP` for warm/hot starts; if no backup source, tie `V_BCKP` to `VCC`.

Recommendation:
- Add all three options in schematic now:
  1. `R_DNP` link: `VCC -> V_BCKP` (default populated for first build).
  2. Backup-source footprint option (coin cell or supercap) to `V_BCKP`.
  3. Isolation diode footprint (DNP by default) between backup source and `V_BCKP`.

This gives you startup-performance flexibility without PCB respin.

### B) USB usage on GPS (`V_USB` pin)
- If GPS USB is not used: `V_USB` must be tied to GND.
- If USB is used: keep 1 uF local capacitor near `V_USB`; u-blox recommends separate LDO path for USB supply behavior.

Recommendation for PAKT rev01:
- You are not planning GPS USB now -> tie `V_USB` to GND in schematic, no connector required.

### C) RF front-end protection level
- u-blox flags RF input ESD protection as mandatory in real-world integration.

Recommendation:
- Add ESD diode footprint at antenna connector to GND (populate by default).
- Keep optional footprints for extra protection/filtering (DNP unless testing requires):
  - series RF element / 0 ohm jumper position
  - SAW footprint or matching placeholder

---

## 2. Power/Supply Stability Components

## Must-have (populate)
- `C_VCC_1uF` close to GPS `VCC` pin.
- `C_VCC_100nF` close to GPS `VCC` pin.
- Solid low-impedance `VCC` feed (avoid series resistance in supply path; short/wide feed).
- Full ground pin stitching to ground plane.

## Strongly recommended (populate)
- Keep GPS supply branch isolated from noisy digital/radio return loops (placement/routing discipline).
- Add at least one GPS rail test point (`TP_GPS_VCC`) and one ground probe point nearby.

## Optional (DNP footprints)
- Ferrite bead in GPS `VCC` branch (`FB_GPS`) if conducted noise appears during coexistence tests.
- Extra bulk cap footprint (`4.7 uF` to `10 uF`) near module in case of rail droop in full-system operation.

---

## 3. RF/Antenna Optional Components

## Must-have (populate)
- ESD protection path at antenna input.
- Controlled-impedance RF trace planning (50 ohm target), no stubs.

## Optional by use-case
- Bias-tee inductor footprint if active antenna feed is needed from `VCC_RF`.
- Current-limit resistor footprint in `VCC_RF` feed path for short-circuit robustness in active-antenna scenarios.
- Optional external LNA/SAW placeholders only if your enclosure/RF environment proves hostile.

For first PAKT board: place footprints, leave DNP unless bench validates need.

---

## 4. Interface/Protection Optional Components

## Recommended optional footprints
- `SAFEBOOT_N` test pad (or tiny header access) for recovery/firmware service.
- `TIMEPULSE` test pad (excellent for bring-up timing/lock diagnostics).
- Optional open-drain isolation footprint on host I/O paths if you plan deep backup scenarios with host rails off (prevents back-powering through IOs).

---

## 5. Minimal Practical BOM Additions (GPS section)

Populate now:
1. `C_GPS_VCC_1uF` (X7R).
2. `C_GPS_VCC_100nF` (X7R).
3. `ESD_RF_GPS` diode at antenna connector.
4. `BAT_GPS_BCKP` = `Panasonic BR-1225A/FAN` (3 V, 48 mAh, tab-mount primary coin cell).
5. `D_GPS_BCKP_OR` = Schottky isolation diode (`BAT54`-class) in series from battery to `V_BCKP`.
6. `TP_TIMEPULSE` and `TP_GPS_VCC`.

Place as DNP options now:
1. `R_VBCKP_LINK` (0 ohm) from `VCC` to `V_BCKP` (lab/fallback option only; keep DNP when battery is populated).
2. `FB_GPS_VCC` ferrite bead.
3. `C_GPS_BULK` 4.7-10 uF.
4. `L_BIAS_TEE` for active antenna power path.
5. Optional RF front-end placeholder parts (series/SAW position).

---

## 6. My Recommendation for Rev01 (fastest safe path)

- Populate: basic decoupling + ESD + backup battery path on `V_BCKP` + testpoints.
- Keep DNP: `VCC -> V_BCKP` fallback link and active-antenna bias path.
- Validate on bench:
  - TTFF and reacquisition performance first.
  - If startup behavior is not acceptable, populate backup source next spin/next build.

---

## 7. Selected Backup Battery (locked for review)

Chosen part:
- `Panasonic ML-1220/F1AN` (rechargeable lithium coin cell, 3 V class).

Why this part:
- Active industrial part with standard 12.5 mm coin-cell form factor.
- Commonly sourced through major distributors; practical for prototype and low-volume builds.
- Capacity (48 mAh nominal) is usable for GPS backup retention without adding a charging circuit.

Implementation notes:
- Keep the isolation diode (`BAT54`-class) between backup battery positive and `V_BCKP`.
- Add low-current trickle path for rechargeable option:
  - `+3V3 -> R18 (100k) -> D7 (BAT54) -> GPS_BCKP_BAT`.
- Keep the `VCC -> V_BCKP` 0 ohm fallback link footprint (`R6`) for bring-up flexibility.
- If you switch back to a primary (non-rechargeable) coin cell, do not populate the trickle-charge path.

Source links:
- u-blox integration guidance (`V_BCKP`, `V_USB`, minimal design):
  - https://content.u-blox.com/sites/default/files/NEO-M9N_Integrationmanual_UBX-19014286.pdf
- Rechargeable backup battery references:
  - https://industry.panasonic.com/global/en/products/primary-batteries/rechargeable-coin/ml

If you want, next message I can convert this into exact schematic symbols/refs/nets for your current `rev01.kicad_sch` naming so you can drop them in directly.
