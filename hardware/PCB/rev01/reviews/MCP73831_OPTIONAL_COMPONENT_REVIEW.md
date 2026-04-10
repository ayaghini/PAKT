# MCP73831 Review 01: Optional/Support Components for Rev01

Scope: `MCP73831T-2ACI/OT` charger block only, focused on required external parts, power stability, and thermal robustness.

Primary references:
- Microchip MCP73831/2 datasheet (DS20001984H):
  - https://ww1.microchip.com/downloads/en/DeviceDoc/MCP73831-Family-Data-Sheet-DS20001984H.pdf
- Microchip MCP73831 product page:
  - https://www.microchip.com/en-us/product/mcp73831

---

## 1. Key datasheet constraints for external components

- `VDD` input should be bypassed to `VSS` with a minimum `4.7 uF`.
- `VBAT` output should be bypassed to `VSS` with a minimum `4.7 uF` for CV-loop stability, especially with no battery attached.
- Charge current is set by resistor from `PROG` to `VSS` with:
  - `IREG (mA) = 1000 / RPROG (kOhm)`.
- If `PROG` is left floating (or driven high), charging is disabled.
- Hot-pluggable inputs (USB/cable) require input overvoltage transient protection; datasheet recommends transorb from input to ground.
- Exposed thermal pad (if DFN package used) must connect to `VSS`; thermal vias improve dissipation.

---

## 2. Power stability and filtering network

Populate now:
1. `C_CHG_IN_4u7` (`>=4.7 uF`, X7R preferred) from `+5V_USB` to GND, near `VDD`.
2. `C_CHG_BAT_4u7` (`>=4.7 uF`, X7R preferred) from `VBAT` to GND, near `VBAT` pin.
3. `R_PROG` set for target charge current (start with `5.1 kOhm` for ~196 mA).
4. Short/wide copper on high-current charger paths (`VDD`, `VBAT`, GND return).

DNP options to place now:
1. Extra bulk on input (`10 uF`) if USB rail droop appears under cable/hub conditions.
2. Extra bulk on battery node (`10 uF`) if pack wiring inductance/no-load ringing appears.

---

## 3. Protection and status interface

Populate now:
1. `TVS_USB_IN` on charger input (transorb/TVS from `+5V_USB` to GND) due to hot-plug transients.
2. `R_LED_CHG` + status LED on `STAT` (if visual charging indicator desired in rev01).

DNP options:
1. `R_STAT_PU` footprint to route `STAT` to MCU input if firmware-side charge-state reading is desired later.
2. Input series impedance footprint (ferrite or small resistor) only if EMI/plug-in transients require tuning.

---

## 4. Thermal and layout recommendations

Populate/layout now:
1. Keep MCP73831 physically close to battery connector and `VBAT` copper to reduce drop.
2. Provide copper area around ground and power pins for heat spreading.
3. If migrating to DFN variant in future revision, include thermal-via strategy under EP.

Bench-triggered optional action:
- If charger enters thermal regulation too early under expected ambient, reduce programmed current (increase `R_PROG`) or improve copper heat spreading.

---

## 5. Minimal practical BOM additions (MCP73831 section)

Populate now:
1. `C_CHG_IN_4u7`
2. `C_CHG_BAT_4u7`
3. `R_PROG` (`5.1 kOhm` initial)
4. `TVS_USB_IN`
5. `R_LED_CHG` + `LED_CHG` (if status LED kept)

Place as DNP/optional:
1. `C_CHG_IN_BULK_10uF`
2. `C_CHG_BAT_BULK_10uF`
3. `R_STAT_PU` + MCU status routing option
4. Input tuning footprint (ferrite/resistor)

---

## 6. Recommended rev01 population

- Populate both required `>=4.7 uF` capacitors and keep `R_PROG = 5.1 kOhm` as first-build baseline.
- Populate input TVS for hot-plug protection.
- Keep optional bulk and STAT-to-MCU path as DNP footprints unless bench behavior requires them.

Bench checks to decide DNP population:
- Input rail transient behavior at plug/unplug events.
- Charge thermal throttling behavior at expected ambient.
- Need for MCU-aware charge-state telemetry beyond LED indication.
