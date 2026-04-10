# SA818 Review 01: Optional/Support Components for Rev01

Scope: SA818S radio block only, with emphasis on power stability, filtering, and protection.

Primary references:
- SA818S product page (pin behavior, specs, typical circuit image):
  - https://www.nicerf.com/walkie-talkie-module/walkie-talkie-module-sa818s-ce.html
- SA818S module datasheet PDF (V1.7, 2025-08):
  - https://www.nicerf.com/download_file?path=/upload/20250814/d374728c4b5426c63631449da8745cc6.pdf&file_name=SA818S%201W%20Embedded%20walkie%20talkie%20module%20V1.7

---

## 1. Key constraints that affect external parts

- Supply range is `3.3 V` to `5.5 V`.
- TX current is high (`650 mA` typ, `750 mA` max at high power).
- Vendor FAQ explicitly flags large supply ripple as a cause of poor range.
- `ANT` expects a `50 ohm` path.
- `H/L` pin: leave open for high power, pull low for low power; do not drive high.
- `RXD` note for sleep entry: pull low before sleep to avoid leakage/reset issues.

---

## 2. Power stability components

Populate now:
1. `C_SA818_100nF` (X7R) very close to `VBAT` pin.
2. `C_SA818_10uF` (X5R/X7R) close to `VBAT`.
3. `C_SA818_BULK_100uF` low-ESR bulk cap on SA818 local branch.
4. Wide/short `VBAT` copper feed with direct return to ground plane.
5. `TP_SA818_VBAT` + nearby GND test point for ripple checks during TX bursts.

DNP options to place now:
1. `FB_SA818_VBAT` ferrite bead on SA818 supply branch if bench ripple/noise coupling appears.
2. Additional bulk footprint (`220 uF`) if battery impedance causes TX droop in enclosure build.

---

## 3. RF protection and matching

Populate now:
1. `ESD_RF_SA818` low-cap RF ESD diode at antenna connector.
2. Keep strict `50 ohm` RF route from SA818 `ANT` to antenna connector.

DNP options to place now:
1. `R_RF_SER` (0 ohm default footprint position) as a tuning/isolation placeholder.
2. Optional `PI` matching placeholder footprints near antenna connector for final RF tuning if needed.

---

## 4. Control and interface robustness

Populate now:
1. `R_PTT_PULLUP` weak pull-up on `SA818_PTT_N` net (keeps radio in RX if MCU pin is floating at reset).
2. `R_PD_PULLUP` weak pull-up on `PD` (keep module in normal operation by default).
3. `R_HL_PD` weak pull-down on `H/L` only if you want default low-power mode; otherwise leave open/default high-power behavior.

DNP options:
1. Series resistors (`22` to `100 ohm`) on `TXD/RXD/PTT` for edge/noise damping if needed after bench EMC checks.

---

## 5. Audio-side optional conditioning

Populate now:
1. AC-coupling capacitor on `AF_OUT` path into codec input chain.
2. AC-coupling capacitor on codec-to-`MIC_IN` path.

DNP options:
1. Optional RC low-pass footprint on `MIC_IN` for transmit-audio bandwidth shaping.
2. Optional RC de-emphasis/cleanup footprint on `AF_OUT` path if receiver noise shaping is needed.

---

## 6. Recommended rev01 population

Populate for first build:
1. `100nF + 10uF + 100uF` local SA818 power caps.
2. RF ESD diode.
3. PTT and PD safe default resistors.
4. Audio AC-coupling parts.

Keep as DNP:
1. Ferrite bead and extra bulk beyond `100uF`.
2. RF tuning placeholders.
3. Optional audio RC shaping parts.

Bench checks to decide DNP population:
- TX burst rail droop/ripple at `VBAT`.
- Spurious reset or UART instability during TX.
- Audio bandwidth/deviation cleanup needs.
