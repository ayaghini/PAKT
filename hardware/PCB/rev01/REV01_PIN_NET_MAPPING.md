# PAKT Rev01 Pin/Net Mapping (As-Built Schematic Snapshot)

This mapping is synced to `hardware/PCB/rev01/rev01.kicad_sch` as currently wired.
Use this file as the source of truth for routing and firmware pin assignment checks.

## 1) MCU Control/Data Mapping

| Function | ESP32-S3-WROOM-1 pin | Net Name | Remote pin(s) | Notes |
|---|---|---|---|---|
| I2C SDA | IO3 | `I2C_SDA` | U3 `CTRL_DATA`, U4 `SDA`, U1 `SDA/~SPI_CS` | Shared I2C bus |
| I2C SCL | IO4 | `I2C_SCL` | U3 `CTRL_CLK`, U4 `SCL`, U1 `SCL/SPI_CLK` | Shared I2C bus |
| I2S BCLK | IO8 | `I2S_BCLK` | U3 `I2S_SCLK` | Audio clock |
| I2S LRCLK/WS | IO15 | `I2S_LRCLK` | U3 `I2S_LRCLK` | Word select |
| I2S DOUT | IO12 | `I2S_DOUT` | U3 `I2S_DOUT` | ESP -> codec |
| I2S DIN | IO10 | `I2S_DIN` | U3 `I2S_DIN` | Codec -> ESP |
| I2S MCLK | IO14 | `I2S_MCLK` | U3 `SYS_MCLK` | Must run before codec init |
| GPS RX path | IO18 | `GPS_RXD` | U1 `TXD/SPI_MISO` | ESP RX <- GPS TX |
| GPS TX path | IO17 | `GPS_TXD` | U1 `RXD/SPI_MOSI` | ESP TX -> GPS RX |
| SA818 RX path | IO9 | `SA818_RXD` | U5 `TXD` | ESP RX <- SA818 TX |
| SA818 TX path | IO13 | `SA818_TXD` | U5 `RXD` | ESP TX -> SA818 RX |
| SA818 PTT | IO11 | `SA818_PTT_N` | U5 `PTT` | Active low |
| USB FS D+ (MCU side) | USB_D+ pin | `USB_D_P` | U8 pin 6 | After ESD device |
| USB FS D- (MCU side) | USB_D- pin | `USB_D_N` | U8 pin 4 | After ESD device |

## 2) USB Data/ESD Chain

| Segment | Net | Nodes |
|---|---|---|
| Connector side D+ | `DD+` | J4 A6/B6, U8 pin 1 |
| Connector side D- | `DD-` | J4 A7/B7, U8 pin 3 |
| MCU side D+ | `USB_D_P` | U8 pin 6, U2 USB_D+ |
| MCU side D- | `USB_D_N` | U8 pin 4, U2 USB_D- |
| ESD supply | `+5V` | U8 pin 5, C22 |

## 3) Power Nets (Current Schematic)

| Net | Source path | Main consumers |
|---|---|---|
| `VBUS` | USB-C VBUS pins | U6 input cap path, Q4 gate control, D6 anode |
| `+5V` | Local USB/ESD rail | U8 (`USBLC6-2P6`), C22 |
| `VBAT` | 18650 battery node (BT2) | U4 `CELL/VDD`, FB16 input, Q4 drain |
| `+5V_SA818` | SA818 pre-supply rail | FB16 output, D15 anode, C33, R7/R8 pull-up source |
| `Net-(D15-K)` | SA818 supply rail after D15 | U5 `VBAT`, C8/C9/C10 |
| `Net-(D6-K)` | Main regulator-input rail | U7 `VIN`, C23, R21, Q4 source |
| `+3.3V` | U7 output | ESP32, SGTL5000 rails, GPS VCC, I2C pull-ups, support caps |
| `VBCKP` | GPS backup rail | BT1+, U1 `V_BCKP`, D2 cathode |
| `GPS_BCKP_CHG` | Backup charge feed | R17 output, D2 anode |

### MAX17048 (U4) pin map (updated symbol)

| U4 pin | Pin name | Intended net/function |
|---|---|---|
| 1 | `CTG` | Leave NC unless using external thermistor path |
| 2 | `CELL` | `VBAT` battery sense |
| 3 | `VDD` | `VBAT` supply |
| 4 | `GND` | `GND` |
| 5 | `*ALRT` | Alert output (`MAX17048_ALRT_N` if labeled) |
| 6 | `QSTRT` | Quick-start control (`MAX17048_QSTRT`) |
| 7 | `SCL` | `I2C_SCL` |
| 8 | `SDA` | `I2C_SDA` |
| 9 | `EPAD` | Tie to `GND` |

## 4) RF/Analog Paths

### SA818 TX/RF path

| Net | From | To |
|---|---|---|
| `SA818_MIC_IN_PATH` | U3 line outputs via C17 | U5 `MIC_IN` |
| `AF_RADIO_OUT` | U5 `AF_OUT` | U3 line inputs via C16 |
| `SA818_ANT` | U5 `ANT` | U9 `IN` |
| `Net-(U9-OUT)` | U9 `OUT` | J2 center pin |

### SGTL5000 bench-reference filtering additions (As currently placed, no AP7313)

Use this as the recommended add-on set for SGTL5000 supply stability and cleaner analog behavior.

| Ref | Value | Connect pin 1 to | Connect pin 2 to | Purpose |
|---|---|---|---|---|
| `C3` | `2.2u_SGTL_PRE` | `+3.3V` | `GND` | Pre-bead bulk decoupling |
| `C4` | `2.2uf` | `+3.3V` | `GND` | Additional pre-bead bulk decoupling (as currently valued) |
| `FB2` | `Murata BLM18PG121SN1D` | `+3.3V` | SGTL filtered rail feeding `VDDA` | Analog-domain HF isolation (same family as SA818 FB16) |
| `C6` | `2.2u_SGTL_POST` | SGTL filtered rail (FB2 output) | `GND` | Post-bead bulk decoupling |
| `C5` | `2.2uf` | SGTL filtered rail (FB2 output) | `GND` | Additional post-bead bulk decoupling (as currently valued) |
| `C15` | `100n_SGTL_VAG` | `U3 VAG` | `GND` | Local VAG HF bypass (place close to U3 pin 5) |

Recommended SGTL rail usage with the above:
- `U3 VDDA` -> SGTL filtered rail from `FB2` output
- `U3 VDDD` and `U3 VDDIO` -> `+3.3V` (or filtered rail if you intentionally choose single-rail codec power)
- Keep `C11/C12/C13/C14` local to U3 pins as currently done.

### GPS rail filtering additions (same ferrite family)

| Ref | Value | Connect pin 1 to | Connect pin 2 to | Purpose |
|---|---|---|---|---|
| `FB1` | `Murata BLM18PG121SN1D` | `+3.3V` | GPS filtered rail feeding `U1 VCC` | Isolate GPS rail noise |
| `C1` | `10u_GPS_RAIL` | GPS filtered rail (FB1 output) | `GND` | GPS local bulk decoupling |
| `C2` | `100n_GPS_RAIL` | GPS filtered rail (FB1 output) | `GND` | GPS local HF bypass |

Audio coupling note from bench reference:
- Teensy Audio Adapter commonly uses `2.2uF` AC-coupling on line paths.
- If you want to match that behavior, change `C16` and `C17` from `1u` to `2.2u`.

Scope note:
- microSD-related parts in the bench reference are intentionally excluded.

### GPS RF path

| Net | From | To | Notes |
|---|---|---|---|
| `GPS_RF_IN` | J5 center pin | U1 `RF_IN` through L3 | GPS antenna input path |
| `Net-(C26-Pad1)` | L3 output | C26, R23 | Matching/bias node |
| `VCCREF` | U1 `VCC_RF` | R23 | RF bias feed |

### GPS timepulse indicator

| Net | From | To |
|---|---|---|
| `GPS_TIMEPULSE` | U1 `TIMEPULSE` | R24 |
| `Net-(D4-A)` | R24 | D4 anode (LED cathode to GND) |

## 5) Key Support Components Present

- GPS: `BT1`, `D2`, `R17`, `J5`, `L3`, `R23`, `C26`, `R24`, `D4`
- SA818 RF/power: `U9`, `J2`, `FB16`, `D15`, `C33`, `C8/C9/C10`
- SGTL filter additions: `FB2`, `C3/C4/C5/C6`, `C15`
- GPS rail filter additions: `FB1`, `C1/C2`
- USB protection: `U8` (`USBLC6-2P6`)
- Charger/fuel gauge core: `U6`, `U4`, `C18/C19/C20`, `R5`, `R10`, `R11`, `R12`
- ESP boot/reset: `SW1`, `SW2`, `R13`, `R14`, `C21`

## 6) Net Label Checklist (Current)

`I2C_SDA`, `I2C_SCL`, `I2S_BCLK`, `I2S_LRCLK`, `I2S_DOUT`, `I2S_DIN`, `I2S_MCLK`, `GPS_RXD`, `GPS_TXD`, `GPS_TIMEPULSE`, `SA818_RXD`, `SA818_TXD`, `SA818_PTT_N`, `SA818_MIC_IN_PATH`, `AF_RADIO_OUT`, `SA818_ANT`, `GPS_RF_IN`, `VCCREF`, `DD+`, `DD-`, `USB_D_P`, `USB_D_N`, `VBUS`, `+5V`, `VBAT`, `+5V_SA818`, `Net-(D15-K)`, `Net-(D6-K)`, `+3.3V`, `VBCKP`, `GPS_BCKP_CHG`, `GND`.

## 7) Notes

- Charger nets are currently auto-named in schematic as `Net-(U6-PROG)` and `Net-(U6-STAT)`.
- ESP EN/BOOT nets are currently auto-named in schematic as `Net-(U2-EN)` and `Net-(U2-IO0)`.
- Keep `R17`/`D2` backup-charge path DNP unless backup chemistry supports recharge.

## 8) Net Standardization Map

Use this map to replace remaining KiCad auto-nets (`Net-(...)`) with explicit names.

| Current Auto Net | Standard Name |
|---|---|
| `Net-(C26-Pad1)` | `GPS_RF_MATCH` |
| `Net-(D1-K)` | `CHG_LED_N` |
| `Net-(D4-A)` | `GPS_TIMEPULSE_LED` |
| `Net-(D5-A)` | `USB_TVS_A` |
| `Net-(D5-K)` | `USB_TVS_K` |
| `Net-(D6-K)` | `VSYS_MAIN` |
| `Net-(D15-K)` | `SA818_VBAT` |
| `Net-(J4-CC1)` | `USB_CC1` |
| `Net-(J4-CC2)` | `USB_CC2` |
| `Net-(U2-EN)` | `ESP_EN` |
| `Net-(U2-IO0)` | `ESP_IO0` |
| `Net-(U4-QSTRT)` | `MAX17048_QSTRT` |
| `Net-(U4-*ALRT)` | `MAX17048_ALRT_N` |
| `Net-(U5-H{slash}L)` | `SA818_HL` |
| `Net-(U5-PD)` | `SA818_PD` |
| `Net-(U6-PROG)` | `MCP73831_PROG` |
| `Net-(U6-STAT)` | `CHG_STAT_N` |
| `Net-(U6-VBAT)` | `MCP73831_VBAT` |
| `Net-(U7-EN)` | `REG_EN` |
| `Net-(U9-OUT)` | `SA818_RF_OUT` |
## 9) Waveshare 3.97in e-Paper HAT+ (40-pin) Integration

Source: Waveshare 3.97in e-Paper HAT+ manual and schematic.

### Recommended ESP32 net assignment (conflict-free with current Rev01)

| e-Paper signal | HAT 40-pin physical pin | ESP32-S3 GPIO | New net name |
|---|---:|---:|---|
| `DIN` | 19 | IO21 | `EPD_DIN` |
| `SCLK` | 23 | IO16 | `EPD_SCLK` |
| `CS` | 24 | IO5 | `EPD_CS` |
| `DC` | 22 | IO6 | `EPD_DC` |
| `RST` | 11 | IO7 | `EPD_RST` |
| `BUSY` | 18 | IO38 | `EPD_BUSY` |
| `PWR` | 12 | IO39 | `EPD_PWR_EN` |
| `3V3` | 1 and/or 17 | 3V3 rail | `+3.3V` |
| `5V` (optional) | 2 and/or 4 | 5V rail | `+5V` |
| `GND` | 6/9/14/20/25/30/34/39 | GND | `GND` |

Notes:
- Above keeps your existing UART/I2C/I2S/SA818 assignments untouched.
- Display board supports 3.3V or 5V supply input; for ESP32-native IO use 3.3V domain.
- Waveshare reference ESP32 mapping is `DIN=IO14`, `CLK=IO13`, `CS=IO15`, `DC=IO27`, `RST=IO26`, `BUSY=IO25`, `PWR=IO33`; reassignment above is intentional to avoid Rev01 pin conflicts.
- `IO14` is already used by `I2S_MCLK` in Rev01 and should not be reused for e-paper.
- For ESP32-S3-WROOM-1 variants with embedded Octal PSRAM (R8 variants), `IO35/IO36/IO37` are not available for user GPIO; mapping above avoids those pins.

### 40-pin socket to place in schematic/PCB

- Symbol: `Connector_Generic:Conn_02x20_Odd_Even`
- Footprint (THT): `Connector_PinSocket_2.54mm:PinSocket_2x20_P2.54mm_Vertical`
- Footprint (stackable/holder style): `Connector_PinSocket_2.54mm:PinSocket_2x20_P2.54mm_Vertical_SMD` (if you need solder-side clearance) or long-tail stackable equivalent from vendor.

Suggested accessible MPN options:
- Through-hole female socket: `Sullins PPTC202LFBN-RC` (2x20, 2.54mm)
- Stackable long-tail female header: `Adafruit 2223` equivalent style (choose vendor-specific 2x20 stackable, 2.54mm, >=8.5mm body).

### Mechanical constraints from vendor docs

Confirmed:
- HAT+ board size: `99.50 mm x 60.00 mm`
- Header pitch: `2.54 mm`, 2x20

Mounting holes:
- Waveshare public manual/schematic do not publish explicit hole XY coordinates for this specific HAT+ board.
- Use these PCB constraints for placement unless you re-measure from the physical board or vendor CAD:
  - Hole drill: `3.2 mm` (M3 clearance)
  - Keepout ring: `>= 6.0 mm` OD recommended
  - If you intend to co-mount with Raspberry Pi pattern hardware, use M2.5 hardware and verify fit against the actual HAT board before fab.
