# PAKT Rev01 Pin/Net Mapping (Bench-Proven Baseline)

This table maps firmware-proven bench wiring into schematic net names for `hardware/PCB/rev01`.

| Function | ESP32-S3-WROOM-1 | Net Name | Connected Device Pin | Notes |
|---|---|---|---|---|
| I2C SDA | GPIO3 | `I2C_SDA` | SGTL5000 I2C SDA, MAX17048 SDA, NEO-M8N SDA/SDA2 | Shared bus (`0x0A`, `0x36`, `0x42`) |
| I2C SCL | GPIO4 | `I2C_SCL` | SGTL5000 I2C SCL, MAX17048 SCL, NEO-M8N SCL/SCL2 | Shared bus |
| I2S BCLK | GPIO8 | `I2S_BCLK` | SGTL5000 BCLK | Audio serial clock |
| I2S WS/LRCLK | GPIO15 | `I2S_WS` | SGTL5000 LRCLK | Audio word select |
| I2S DOUT | GPIO12 | `I2S_DOUT` | SGTL5000 DIN | TX audio to codec |
| I2S DIN | GPIO10 | `I2S_DIN` | SGTL5000 DOUT | RX audio from codec |
| I2S MCLK | GPIO14 | `I2S_MCLK` | SGTL5000 SYS_MCLK | Must be driven before codec init |
| SA818 UART TX | GPIO13 | `SA818_RXD` | SA818 RXD (pin 16) | ESP TX -> SA818 RX |
| SA818 UART RX | GPIO9 | `SA818_TXD` | SA818 TXD (pin 17) | ESP RX <- SA818 TX |
| SA818 PTT | GPIO11 | `SA818_PTT_N` | SA818 PTT (pin 5) | Active-low PTT |
| Native USB D+ | GPIO20 (USB_D+) | `USB_D_P` | USB-C J4 A6/B6 | USB 2.0 data line |
| Native USB D- | GPIO19 (USB_D-) | `USB_D_N` | USB-C J4 A7/B7 | USB 2.0 data line |
| GPS UART TX (fallback) | GPIO17 | `GPS_UART_TX` | NEO RXD1 | Optional fallback path |
| GPS UART RX (fallback) | GPIO18 | `GPS_UART_RX` | NEO TXD1 | Optional fallback path |

## Power Nets

| Net | Source | Consumers |
|---|---|---|
| `USB_C_VBUS` | USB-C receptacle VBUS pins | Input-side 5V before rail conditioning |
| `+5V_USB` | Conditioned USB 5V rail | MCP73831 `VDD`, USB-side power consumers |
| `VBAT` | Li-ion battery node | MCP73831 `VBAT`, MAX17048 `CELL/VDD`, battery-side power consumers |
| `VHI` | Feather-style automatic source-selected rail (`VBUS` via Schottky or `VBAT` via PMOS) | SA818 `VBAT`, 3.3V regulator input |
| `+3V3` | 3.3V LDO output (`AP2112K-3.3`) | ESP32-S3, SGTL5000 digital/analog rails, GPS VCC, logic pull-ups |
| `GPS_BCKP_BAT` | GPS backup battery positive (`BR-1225A/FAN`) | GPS `V_BCKP` through Schottky isolation |
| `GPS_VBCKP` | GPS backup-domain supply | NEO-M8N/M9N `V_BCKP` |
| `VBCKP` | Implemented backup-domain net in current schematic | BT1+, U1 `V_BCKP`, D2 cathode |
| `GPS_BCKP_CHG` | Optional trickle-charge feed node | R17 output -> D2 anode |
| `GND` | Common return | All blocks |

## Analog Audio Nets

| Net | From | To | Notes |
|---|---|---|---|
| `AF_TX_CODEC_OUT` | SGTL5000 line/headphone out path | SA818 `MIC_IN` (pin 18) via coupling + attenuation | Set final divider by deviation test |
| `AF_RX_RADIO_OUT` | SA818 `AF_OUT` (pin 3) | SGTL5000 line in via AC coupling | Keep short, quiet route |
| `RF_ANT_144` | SA818 `ANT` (pin 12) | Antenna connector/matching | 50 ohm RF practice |

## Support Components Net Map

Use this section when wiring the added support parts in `rev01.kicad_sch`.

### GPS (`U1`) support

| Ref | Value | Connect pin 1 to | Connect pin 2 to | Notes |
|---|---|---|---|---|
| `BT1` | `BR-1225A/FAN` | `GPS_BCKP_BAT` | `GND` | Backup coin cell |
| `D2` | `BAT54` | `VBCKP` | `GPS_BCKP_CHG` | Current rev01 uses D2 + R17 as optional backup-battery trickle path |
| `R17` | `4.7k_GPS_BCKP_CHG` | `+3V3` | `GPS_BCKP_CHG` | Optional trickle current limiter; DNP unless using rechargeable backup cell |
| `R6` | `0R_VBCKP_LINK` | `+3V3` | `GPS_VBCKP` | Fallback link, normally DNP when battery path is used |
| `C7` | `4.7u_GPS_BULK` | `+3V3` | `GND` | Optional GPS rail bulk |
| `FB1` | `FB_GPS_VCC` | `+3V3` | `GPS_VCC` | Optional bead for GPS rail isolation |
| `L1` | `L_BIAS_TEE` | `GPS_VCC` | `GPS_ANT_BIAS` | Optional active-antenna bias path |
| `D3` | `ESD_RF_GPS` | `GPS_RF_IN` | `GND` | RF ESD shunt near GPS antenna input |
| `J5` | `GPS_ANT` | `GPS_RF_IN` | `GND` | Added dedicated GPS antenna connector (U.FL footprint) |
| `TP5` | `TP_GPS_VCC` | `GPS_VCC` | - | Test point |
| `TP6` | `TP_TIMEPULSE` | `GPS_TIMEPULSE` | - | Test point |

GPS core net reminders:
- `U1 VCC` -> `GPS_VCC` (typically from `+3V3` directly or through `FB1`)
- `U1 V_BCKP` -> `GPS_VBCKP`
- `U1 V_USB` -> `GND` (USB unused)

### SGTL5000 (`U3`) support

| Ref | Value | Connect pin 1 to | Connect pin 2 to | Notes |
|---|---|---|---|---|
| `C11` | `100n_VDDD` | `SGTL_VDDD` | `GND` | Local decoupling |
| `C12` | `100n_VDDA` | `SGTL_VDDA` | `GND` | Local decoupling |
| `C13` | `100n_VDDIO` | `SGTL_VDDIO` | `GND` | Local decoupling |
| `C14` | `1u_VAG` | `SGTL_VAG` | `GND` | Required VAG filter cap |
| `C15` | `100n_CPFLT` | `SGTL_CPFLT` | `GND` | Populate only when both `VDDA` and `VDDIO` <= 3.0V |
| `FB2` | `FB_SGTL_VDDA` | `+3V3` | `SGTL_VDDA` | Optional analog rail isolation |
| `C16` | `1u_AF_RX_IN` | `AF_RX_RADIO_OUT` | `SGTL_LINEIN` | AC coupling on RX audio path |
| `C17` | `1u_AF_TX_OUT` | `AF_TX_CODEC_OUT` | `SA818_MIC_IN_PATH` | AC coupling on TX audio path |

SGTL rail map:
- `SGTL_VDDD`, `SGTL_VDDIO`, `SGTL_VDDA` usually tie to `+3V3` (with optional bead on `VDDA`)

### SA818 (`U5`) support

| Ref | Value | Connect pin 1 to | Connect pin 2 to | Notes |
|---|---|---|---|---|
| `C8` | `100n_SA818` | `VHI` | `GND` | Local high-frequency decoupling |
| `C9` | `10u_SA818` | `VHI` | `GND` | Local bulk decoupling |
| `C10` | `100u_SA818` | `VHI` | `GND` | TX burst bulk support |
| `R7` | `47k_PTT_PULLUP` | `SA818_PTT_N` | `+3V3` | Keeps RX default when MCU floats |
| `R8` | `10k_PD_PULLUP` | `SA818_PD` | `+3V3` | Keeps module enabled |
| `R9` | `100k_HL_PD` | `SA818_HL` | `GND` | Optional low-power default |
| `D4` | `ESD_RF_SA818` | `RF_ANT_144` | `GND` | RF ESD shunt near antenna connector |
| `TP7` | `TP_SA818_VBAT` | `VHI` | - | Test point |

SA818 core net reminders:
- `U5 pin 16 RXD` -> `SA818_RXD`
- `U5 pin 17 TXD` -> `SA818_TXD`
- `U5 pin 5 PTT` -> `SA818_PTT_N`
- `U5 pin 18 MIC_IN` -> `SA818_MIC_IN_PATH`
- `U5 pin 3 AF_OUT` -> `AF_RX_RADIO_OUT`

### MAX17048 (`U4`) support

| Ref | Value | Connect pin 1 to | Connect pin 2 to | Notes |
|---|---|---|---|---|
| `C18` | `100n_FG_VDD` | `VBAT` | `GND` | Required VDD bypass |
| `R10` | `10k_ALRT_PU` | `+3V3` | `MAX17048_ALRT_N` | Alert pull-up |
| `R11` | `0R_QSTRT_GND` | `MAX17048_QSTRT` | `GND` | Tie QSTRT low |
| `TP8` | `TP_MAX17048_ALRT` | `MAX17048_ALRT_N` | - | Optional alert test point |

MAX17048 core net reminders:
- `U4 CELL` -> `VBAT`
- `U4 VDD` -> `VBAT`
- `U4 SDA` -> `I2C_SDA`
- `U4 SCL` -> `I2C_SCL`
- `U4 CTG`/EP -> `GND`

### MCP73831 (`U6`) support

| Ref | Value | Connect pin 1 to | Connect pin 2 to | Notes |
|---|---|---|---|---|
| `C19` | `4.7u_CHG_IN` | `+5V_USB` | `GND` | Required input cap |
| `C20` | `4.7u_CHG_BAT` | `VBAT` | `GND` | Required output/battery cap |
| `D5` | `TVS_USB_IN` | `+5V_USB` | `GND` | Input hot-plug transient suppression |
| `R12` | `10k_STAT_PU` | `+3V3` | `CHG_STAT_N` | Optional pull-up for MCU monitoring |
| `R5` | `5.1k_RPROG` | `MCP73831_PROG` | `GND` | ~196mA charge current baseline |
| `D1` + `R_LED_CHG` | `LED_CHG` + LED resistor | `+3V3` -> `R_LED_CHG` -> `D1` -> `CHG_STAT_N` | - | Visual charge status (resistor placeholder if used) |

### USB-C input and main-rail path

| Ref | Value | Connect pin 1 to | Connect pin 2 to | Notes |
|---|---|---|---|---|
| `J4` | `USB_C_Receptacle_USB2.0_16P` | `USB_C_VBUS` / USB2 D+/D- / CC pins | `GND` / shield | Main USB-C connector |
| `R15` | `5.1k_CC1` | `USB_CC1` | `GND` | USB sink Rd |
| `R16` | `5.1k_CC2` | `USB_CC2` | `GND` | USB sink Rd |
| `D6` | `MBR0540` | `+5V_USB` | `VHI` | Feather-style Schottky path from USB rail to system rail |
| `Q4` | `DMG3415U` | `VBAT` / `VBUS` gate / `VHI` | - | Feather-style battery ideal-path PMOS |
| `R19` | `100k_VBUS_PD` | `VBUS` | `GND` | VBUS bleed/pulldown for stable gate behavior |
| `TP11` | `TP_VSYS_MAIN` | `VHI` | - | Main rail test point |

### Main Battery Holder

| Ref | Value | Connect pin 1 to | Connect pin 2 to | Notes |
|---|---|---|---|---|
| `BT2` | `18650_LiIon` | `VBAT` | `GND` | Added 18650 holder symbol with `BatteryHolder_Keystone_1042_1x18650` footprint |

### 3.3V regulator (`U7`) support

| Ref | Value | Connect pin 1 to | Connect pin 2 to | Notes |
|---|---|---|---|---|
| `U7` | `AP2112K-3.3` | `VHI` / `EN` / `GND` / `+3V3` | per pin function | Feather-style 3.3V LDO |
| `R21` | `100k_EN_PULLUP` | `VHI` | `REG_EN` | Regulator enable pull-up |
| `C23` | `10u_VHI_IN` | `VHI` | `GND` | LDO input bulk |
| `C24` | `10u_3V3_OUT` | `+3V3` | `GND` | LDO output bulk |
| `C25` | `10u_3V3_BULK2` | `+3V3` | `GND` | Existing bulk on 3V3 branch |
| `C26` | `100n_3V3_HF` | `+3V3` | `GND` | Existing HF bypass on 3V3 branch |

### ESP32-S3 (`U2`) support

| Ref | Value | Connect pin 1 to | Connect pin 2 to | Notes |
|---|---|---|---|---|
| `R13` | `10k_EN_PULLUP` | `+3V3` | `ESP_EN` | EN pull-up |
| `R14` | `10k_IO0_PULLUP` | `+3V3` | `ESP_IO0` | Boot strap pull-up |
| `C21` | `100n_EN_DELAY` | `ESP_EN` | `GND` | Reset stability cap |
| `C22` | `10u_3V3_ENTRY` | `+3V3` | `GND` | Local 3V3 bulk near module |
| `TP9` | `TP_EN` | `ESP_EN` | - | Test point |
| `TP10` | `TP_GPIO0` | `ESP_IO0` | - | Test point |

ESP32 core net reminders:
- `ESP_EN` -> `U2 EN`
- `ESP_IO0` -> `U2 IO0`

Current rev01 implementation note:
- EN/BOOT switch nets are currently KiCad-autonamed as `Net-(U2-EN)` and `Net-(U2-IO0)`; `SW1` and `SW2` pull these lines low to `GND` when pressed.
- `BT1` is presently `BR-1225A/FAN` (primary/non-rechargeable). Keep `R17`/`D2` trickle path DNP unless backup chemistry is changed to rechargeable.

### Shared bus pull-ups

| Ref | Value | Connect pin 1 to | Connect pin 2 to |
|---|---|---|---|
| `R3` | `4.7k` | `+3V3` | `I2C_SDA` |
| `R4` | `4.7k` | `+3V3` | `I2C_SCL` |

## Place These Net Labels First

`I2C_SDA`, `I2C_SCL`, `I2S_BCLK`, `I2S_WS`, `I2S_DOUT`, `I2S_DIN`, `I2S_MCLK`, `SA818_RXD`, `SA818_TXD`, `SA818_PTT_N`, `SA818_PD`, `SA818_HL`, `GPS_UART_TX`, `GPS_UART_RX`, `GPS_VCC`, `GPS_VBCKP`, `GPS_BCKP_BAT`, `GPS_TIMEPULSE`, `GPS_RF_IN`, `SGTL_VDDD`, `SGTL_VDDA`, `SGTL_VDDIO`, `SGTL_VAG`, `SGTL_CPFLT`, `SGTL_LINEIN`, `SA818_MIC_IN_PATH`, `MAX17048_ALRT_N`, `MAX17048_QSTRT`, `MCP73831_PROG`, `CHG_STAT_N`, `ESP_EN`, `ESP_IO0`, `REG_EN`, `USB_C_VBUS`, `USB_CC1`, `USB_CC2`, `VBUS`, `VHI`, `VBAT`, `+3V3`, `+5V_USB`, `GND`, `AF_TX_CODEC_OUT`, `AF_RX_RADIO_OUT`, `RF_ANT_144`.

## Selected Part Lock (Rev01)

- ESP32 module: `ESP32-S3-WROOM-1` (PCB antenna)
- Codec: `SGTL5000XNLA3`
- GPS layout baseline: `NEO-M8M-0` / `NEO-M8N` compatible footprint (`ublox_NEO`)
- Fuel gauge: `MAX17048G+T10`
- Charger: `MCP73831T-2ACI/OT`
- Radio: `SA818S`
