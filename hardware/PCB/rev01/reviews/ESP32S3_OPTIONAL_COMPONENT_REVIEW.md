# ESP32-S3 Review 01: Optional/Support Components for Rev01

Scope: `ESP32-S3-WROOM-1` module integration only, focused on boot/reset robustness, power stability, and RF-safe layout support parts.

Primary references:
- ESP32-S3-WROOM-1/WROOM-1U datasheet:
  - https://www.espressif.com/sites/default/files/documentation/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf
- ESP32-S3 hardware design guidelines:
  - https://documentation.espressif.com/esp-hardware-design-guidelines/en/latest/esp32s3/index.html

---

## 1. Key constraints from official guidance

- Boot strapping depends on GPIO strapping pins (`GPIO0`, `GPIO45`, `GPIO46`; plus `GPIO3` for JTAG source selection).
- `GPIO0` and `GPIO46` control boot mode at reset; signal timing must satisfy setup/hold around reset release.
- Module placement must protect antenna performance; baseboard interference near antenna region should be minimized.
- Hardware guidelines recommend 3.3 V supply with sufficient current capability and local input capacitance/ESD protection at power entry.

---

## 2. Power stability and entry protection

Populate now:
1. `TVS_3V3_IN` (or upstream rail TVS) at module power entry if rail is exposed to cable/hot-plug transients.
2. `C_3V3_ENTRY_10uF` bulk capacitor at 3.3 V entry to the ESP32 power branch.
3. `C_3V3_LOCAL_100nF` decoupling near module power pins (at least one per local power cluster).
4. Keep power feed short/wide and ensure low-impedance return path.

DNP options to place now:
1. Extra local `1uF` footprint near module supply pins if bench shows transient droop.
2. Ferrite bead footprint for isolating noisy digital branch if coexistence noise appears.

---

## 3. Reset/boot support network

Populate now:
1. `R_EN_PULLUP` (10k typical) on EN/CHIP_PU.
2. `C_EN_DELAY` (100 nF to 1 uF footprint) to GND for reset stabilization/boot reliability tuning.
3. `R_IO0_PULLUP` (10k typical) so normal boot defaults to SPI boot.
4. `BOOT` pushbutton footprint to pull `GPIO0` low for download mode.
5. `RESET` pushbutton footprint on EN.

DNP options to place now:
1. Auto-program transistor network footprint (USB-UART DTR/RTS to EN/IO0) if automated flashing is desired in rev01.
2. Small series resistor footprints on EN/IO0 control lines for edge/noise tuning.

Note:
- Exact pull-up/down values can be finalized after combined MCU + USB-UART interface decision; values above are standard integration practice for ESP modules.

---

## 4. Strapping pin safety and reserved pins

Populate/layout now:
1. Keep strapping-pin external loads weak/high-impedance at reset unless intentionally controlling boot behavior.
2. Avoid large capacitive loads directly on strapping pins.
3. Keep test points for at least `EN`, `GPIO0`, `U0TXD`, `U0RXD`.

Design rule notes:
- Respect module-specific unavailable pins tied to in-package memory interfaces (variant-dependent) per module datasheet.
- Do not place copper/ground/features in antenna keepout region.

---

## 5. Minimal practical BOM additions (ESP32-S3 section)

Populate now:
1. `R_EN_PULLUP`
2. `C_EN_DELAY`
3. `R_IO0_PULLUP`
4. `SW_BOOT` and `SW_RESET`
5. `C_3V3_ENTRY_10uF` + local decoupling (`100nF` class)

Place as DNP/optional:
1. Auto-program transistor network
2. Extra local `1uF` near module supply
3. Ferrite bead on module power branch

---

## 6. Recommended rev01 population

- Populate robust manual boot/reset network and power-entry stabilization now.
- Keep auto-download circuit as DNP footprint unless you want immediate USB-UART one-click flashing on prototype boards.
- Reserve antenna keepout strictly in layout and keep noisy/high-current nets away from module antenna edge.

Bench checks to decide optional population:
- Cold-boot and reset reliability across supply ramp profiles.
- Flash/download reliability with chosen programming workflow.
- RF performance sensitivity to nearby board features.
