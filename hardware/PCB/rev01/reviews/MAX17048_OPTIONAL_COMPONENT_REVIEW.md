# MAX17048 Review 01: Optional/Support Components for Rev01

Scope: `MAX17048G+T10` only, focused on supply stability, sensing integrity, and alert/pullup behavior for schematic readiness.

Primary references:
- ADI MAX17048/MAX17049 datasheet (Rev. 7):
  - https://www.analog.com/media/en/technical-documentation/data-sheets/max17048-max17049.pdf
- ADI MAX17048 product page:
  - https://www.analog.com/en/products/max17048.html

---

## 1. Key datasheet constraints for external components

- `VDD` must be bypassed with `0.1 uF` to GND (explicit pin description requirement).
- `MAX17048` `VDD` and `CELL` both connect to the positive battery terminal in 1S usage.
- `CTG` must connect to ground.
- `QSTRT` should be tied to GND if not used.
- `ALRT` is open-drain active-low; if used, system pull-up is required.
- Datasheet states the system must provide pullups for `ALRT` (if used), `SDA`, and `SCL`.
- Exposed pad (TDFN) should be connected to GND.

---

## 2. Power/sense stability components

Populate now:
1. `C_FG_VDD_100nF` (X7R) very close to `VDD`.
2. Kelvin-style short routing from battery rail to `CELL`/`VDD` node (avoid noisy/high-current detours).
3. Solid local ground reference and exposed-pad grounding vias.

DNP options to place now:
1. `C_FG_BULK_1uF` footprint near `VDD`/`CELL` only if bench noise or droop appears.
2. Optional small series resistor footprint in alert line for edge damping if needed by EMC testing.

---

## 3. I2C and alert support network

Populate now:
1. `R_I2C_SDA_PU` and `R_I2C_SCL_PU` on the shared bus (typical 4.7k to 3.3V unless bus capacitance dictates otherwise).
2. `R_FG_ALRT_PU` (typical 10k to 3.3V) if `ALRT` is routed to MCU.
3. `QSTRT` tie-to-GND (direct or via 0R link).

DNP options:
1. Test-point on `ALRT` if not routed to MCU interrupt on rev01.
2. Optional 0R link to repurpose `QSTRT` to a reset pulse source in later testing.

Note:
- Pull-up resistor values above are implementation guidance inferred from standard I2C/open-drain practice; the datasheet mandates pullups but does not force exact resistor values.

---

## 4. Minimal practical BOM additions (MAX17048 section)

Populate now:
1. `C_FG_VDD_100nF`
2. `R_I2C_SDA_PU`
3. `R_I2C_SCL_PU`
4. `R_FG_ALRT_PU` (if `ALRT` used)
5. `QSTRT` GND tie component (`0R` or direct net tie)

Place as DNP/optional:
1. `C_FG_BULK_1uF`
2. Optional alert-series resistor footprint
3. Optional `ALRT` test-point if MCU interrupt is omitted

---

## 5. Recommended rev01 population

- Populate the mandatory `0.1uF` bypass and stable battery-sense routing now.
- Keep `QSTRT` tied low for rev01.
- Route `ALRT` to MCU with pull-up if you want low-SOC interrupt behavior from first bring-up; otherwise leave it on test pad with pull-up footprint.

Bench checks to decide optional population:
- I2C waveform quality at chosen pull-up values.
- Gauge reading stability during SA818 TX current bursts.
- Need for asynchronous low-battery interrupt handling (`ALRT`) in firmware behavior.
