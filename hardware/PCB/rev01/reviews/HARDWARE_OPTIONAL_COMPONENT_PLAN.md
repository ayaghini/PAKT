# Rev01 Optional Components Plan

This plan tracks datasheet-driven optional/support components one block at a time, with explicit decisions for what we populate in `rev01` vs leave as DNP.

## Current status

| Block | Review file | Status | Decision summary |
|---|---|---|---|
| GPS (`NEO-M8N`/`M9N`-layout compatible) | `GPS_OPTIONAL_COMPONENT_REVIEW.md` | done | Add backup battery path + decoupling + RF ESD now; keep ferrite/bias-tee/extra RF filtering as DNP options. |
| Radio (`SA818S`) | `SA818_OPTIONAL_COMPONENT_REVIEW.md` | done | Add robust local power decoupling/bulk and RF ESD; keep extra supply filtering/audio conditioning as DNP options until bench data says needed. |
| Audio codec (`SGTL5000XNLA3`) | `SGTL5000_OPTIONAL_COMPONENT_REVIEW.md` | done | Core decoupling + VAG cap + audio AC coupling now; keep CPFLT as conditional by rail voltage and optional analog shaping as DNP. |
| Fuel gauge (`MAX17048G+T10`) | `MAX17048_OPTIONAL_COMPONENT_REVIEW.md` | done | Add mandatory 0.1uF bypass + clean CELL/VDD sense routing; tie QSTRT low; ALRT/I2C pullups included with optional alert flexibility. |
| Charger (`MCP73831T-2ACI/OT`) | `MCP73831_OPTIONAL_COMPONENT_REVIEW.md` | done | Add required >=4.7uF input/output caps, set RPROG baseline, include TVS and thermal/layout safeguards; keep extra bulk/STAT-MCU path as options. |
| MCU (`ESP32-S3-WROOM-1`) | `ESP32S3_OPTIONAL_COMPONENT_REVIEW.md` | done | Add robust EN/BOOT network and entry decoupling; keep auto-program and extra filtering as optional footprints; enforce antenna keepout/strap safety. |

## Locked decision: GPS backup battery

- Selected battery: `Panasonic BR-1225A/FAN` (3 V, 48 mAh, tab-mount primary cell).
- Required support parts:
  1. `D_GPS_BCKP_OR` Schottky diode (`BAT54` class), battery -> `V_BCKP`.
  2. `R_VBCKP_LINK` 0 ohm fallback link (`VCC -> V_BCKP`), DNP by default if battery is populated.
  3. `C_GPS_VCC_1uF` + `C_GPS_VCC_100nF` local decoupling at GPS `VCC`.

## Notes for schematic capture

- Use DNP footprints for optional filters from each review so rev01 can be tuned on bench without PCB respin.
- Record final population choices in this plan before routing freeze.

## Power architecture updates (current)

- USB input connector added: `USB_C_Receptacle_USB2.0_16P` (`J4`) with `5.1k` Rd on `CC1/CC2`.
- Main rail for high-current radio path is now `VSYS_MAIN`.
- SA818 supply preference updated to `VSYS_MAIN` (not from `+3V3` regulator output).
- Added `TPS63031DSK` (`U7`) as 3.3V regulator stage with required inductor and input/output capacitors.
- GPS backup battery updated to rechargeable option (`ML-1220/F1AN`) with trickle path (`R18` + `D7`) plus isolation diode to `V_BCKP`.
