# SGTL5000 Review 01: Optional/Support Components for Rev01

Scope: `SGTL5000XNLA3` (20-QFN) only, focused on power stability, filtering, and practical audio-path support components for PAKT.

Primary references:
- NXP SGTL5000 datasheet (Rev. 7, 2022-01):
  - https://www.nxp.com/docs/en/data-sheet/SGTL5000.pdf
- NXP SGTL5000 product page:
  - https://www.nxp.com/products/audio/audio-converters/ultra-low-power-audio-codec%3ASGTL5000

---

## 1. Key datasheet constraints for external components

- External `VDDD` is required for new designs (datasheet typical applications note).
- `VDDIO` and `VDDA` each need local decoupling; if both come from same rail, one decoupler can be shared and should be nearest `VDDA`.
- `VAG` requires an external filter capacitor.
- `CPFLT` uses `0.1 uF` to GND only when both `VDDIO` and `VDDA` are `<= 3.0 V`; if either is `> 3.0 V`, `CPFLT` capacitor must not be placed.
- Exposed pad must be connected to ground.
- `HP_VGND` is a virtual ground and must not be tied to system ground.

---

## 2. Power and reference stability network

Populate now:
1. `C_SGTL_VDDD_100nF` close to `VDDD`.
2. `C_SGTL_VDDA_100nF` close to `VDDA`.
3. `C_SGTL_VDDIO_100nF` close to `VDDIO`.
4. `C_SGTL_VAG_1uF` close to `VAG`.
5. Solid exposed-pad-to-GND via stitching.
6. Keep `SYS_MCLK` net clean/short with a nearby ground reference.

DNP options to place now:
1. `C_SGTL_VDDD_BULK_1uF` footprint if rail impedance/noise is seen during bench tests.
2. Ferrite bead footprint on `VDDA` branch (`FB_SGTL_VDDA`) if digital switching noise leaks into analog path.

Conditional assembly rule:
- `C_SGTL_CPFLT_100nF`:
  - Populate only if both `VDDIO` and `VDDA` are `<= 3.0 V`.
  - DNP if either rail is `> 3.0 V`.

---

## 3. Analog audio coupling/filter options for PAKT

Populate now:
1. AC-coupling cap on `AF_RX_RADIO_OUT -> SGTL_LINEIN` path (`1 uF` class, as in typical app style).
2. AC-coupling cap on `SGTL -> AF_TX_CODEC_OUT` path (`1 uF` class).

DNP options to place now:
1. Optional RC low-pass footprint on TX audio path for deviation shaping into SA818 `MIC_IN`.
2. Optional RC cleanup footprint on RX path from SA818 `AF_OUT`.
3. Optional mic-bias network footprints if future board revision adds electret mic use.

---

## 4. Headphone / line-output path options

For current PAKT rev01 (radio modem focus), headphone output is not a primary requirement.

Recommendation:
- Keep headphone-jack related components as DNP footprints only (or omit on first spin if board area is tight).
- If headphone monitor path is desired later, place cap-coupled output option footprints.

---

## 5. Minimal practical BOM additions (SGTL section)

Populate now:
1. `C_SGTL_VDDD_100nF`
2. `C_SGTL_VDDA_100nF`
3. `C_SGTL_VDDIO_100nF`
4. `C_SGTL_VAG_1uF`
5. `C_AF_RX_IN_1uF` (SA818 AF_OUT to SGTL input)
6. `C_AF_TX_OUT_1uF` (SGTL output to SA818 MIC_IN path)

Place as DNP/conditional:
1. `C_SGTL_CPFLT_100nF` (conditional by rail voltage rule above)
2. `FB_SGTL_VDDA`
3. `C_SGTL_VDDD_BULK_1uF`
4. Optional TX/RX analog RC shaping footprints
5. Optional headphone monitoring path parts

---

## 6. Recommended rev01 population

- Populate the core decoupling/reference capacitors and AC-coupling parts now.
- Keep CPFLT capacitor conditional based on final rail values (`3.3 V` rails usually mean DNP).
- Keep analog-shaping and headphone-monitor features as DNP/placeholders for bench tuning.

Bench checks to decide DNP population:
- Audible digital-noise bleed into analog path.
- RX baseband noise floor in quiet captures.
- TX deviation cleanup needs on real RF measurements.
